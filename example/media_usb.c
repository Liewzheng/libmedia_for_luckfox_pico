/**
 * @file media_usb.c
 * @brief V4L2多平面图像采集与USB传输系统 (libMedia版本)
 *
 * 本程序是 v4l2_usb.c 的重构版本，使用 libMedia 库来简化V4L2操作。
 * 主要功能包括：
 * - 通过libMedia接口采集RAW格式的图像数据
 * - 使用多线程架构实现实时图像流传输
 * - 通过TCP Socket将图像数据发送给客户端
 * - 支持多平面缓冲区管理和内存映射
 *
 * @author Development Team
 * @date 2025-07-01
 * @version 2.0 (libMedia版本)
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// 引入 libMedia 头文件
#include "media.h"

// ========================== 系统配置常量 ==========================

/** @brief 图像宽度，单位：像素 */
#define WIDTH 1920

/** @brief 图像高度，单位：像素 */
#define HEIGHT 1080

/** @brief 像素格式：10位BGGR原始数据格式 */
#define PIXELFORMAT V4L2_PIX_FMT_SBGGR10

/** @brief V4L2缓冲区数量，减少内存占用 */
#define BUFFER_COUNT 4

// ========================== USB传输配置 ==========================

/** @brief 默认TCP服务器端口 */
#define DEFAULT_PORT 8888

/** @brief Luckfox Pico设备默认IP地址 */
#define DEFAULT_SERVER_IP "172.32.0.93"

/** @brief 数据帧头部大小，单位：字节 */
#define HEADER_SIZE 32

/** @brief 网络传输分块大小，64KB以提高网络效率 */
#define CHUNK_SIZE 65536

// ========================== 数据结构定义 ==========================

/**
 * @struct frame_data
 * @brief 图像帧数据结构
 *
 * 封装单帧图像的所有相关信息，用于线程间数据传递。
 */
struct frame_data
{
    uint8_t* data;      /**< 图像数据指针 */
    size_t size;        /**< 图像数据大小，单位：字节 */
    uint32_t frame_id;  /**< 帧序号，用于跟踪和调试 */
    uint64_t timestamp; /**< 时间戳，单位：纳秒 */
};

/**
 * @struct frame_header
 * @brief 数据帧头部结构
 *
 * 定义传输协议的帧头格式，包含帧的元数据信息。
 */
struct frame_header
{
    uint32_t magic;       /**< 魔数标识：0xDEADBEEF */
    uint32_t frame_id;    /**< 帧序号 */
    uint32_t width;       /**< 图像宽度 */
    uint32_t height;      /**< 图像高度 */
    uint32_t pixfmt;      /**< 像素格式 */
    uint32_t size;        /**< 数据大小 */
    uint64_t timestamp;   /**< 时间戳 */
    uint32_t reserved[2]; /**< 保留字段 */
} __attribute__((packed));

// ========================== 全局变量 ==========================

/** @brief 程序运行状态标志 */
volatile int running = 1;

/** @brief 客户端连接状态 */
volatile int client_connected = 0;

/** @brief TCP服务器文件描述符 */
int server_fd = -1;

/** @brief 客户端连接文件描述符 */
int client_fd = -1;

/** @brief 帧数据访问互斥锁 */
pthread_mutex_t frame_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief 帧准备就绪条件变量 */
pthread_cond_t frame_ready = PTHREAD_COND_INITIALIZER;

/** @brief 当前帧数据 */
struct frame_data current_frame = {0};

/** @brief libMedia 会话句柄 */
media_session_t* media_session = NULL;

// ========================== 工具函数 ==========================

/**
 * @brief 获取高精度时间戳
 */
static inline uint64_t get_time_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief 系统信号处理函数
 */
void signal_handler(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;

    // 强制关闭服务器socket
    if (server_fd >= 0) {
        shutdown(server_fd, SHUT_RDWR);
    }

    // 关闭客户端连接
    if (client_fd >= 0) {
        shutdown(client_fd, SHUT_RDWR);
    }

    // 通知条件变量
    pthread_mutex_lock(&frame_mutex);
    pthread_cond_broadcast(&frame_ready);
    pthread_mutex_unlock(&frame_mutex);
}

// ========================== 网络通信函数 ==========================

/**
 * @brief 创建TCP服务器
 */
int create_server(int port)
{
    int fd;
    struct sockaddr_in addr;
    int opt = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket failed");
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(DEFAULT_SERVER_IP);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        perror("listen failed");
        close(fd);
        return -1;
    }

    printf("Server listening on %s:%d\n", DEFAULT_SERVER_IP, port);
    return fd;
}

/**
 * @brief 发送图像帧数据到客户端
 */
int send_frame(int client_fd, void* data, size_t size, uint32_t frame_id, uint64_t timestamp)
{
    struct frame_header header = {
        .magic = 0xDEADBEEF,
        .frame_id = frame_id,
        .width = WIDTH,
        .height = HEIGHT,
        .pixfmt = PIXELFORMAT,
        .size = size,
        .timestamp = timestamp,
        .reserved = {0, 0}
    };

    // 发送帧头
    if (send(client_fd, &header, sizeof(header), MSG_NOSIGNAL) != sizeof(header)) {
        return -1;
    }

    // 分块发送数据
    size_t sent = 0;
    uint8_t* ptr = (uint8_t*)data;

    while (sent < size && running) {
        size_t to_send = (size - sent) > CHUNK_SIZE ? CHUNK_SIZE : (size - sent);
        ssize_t result = send(client_fd, ptr + sent, to_send, MSG_NOSIGNAL);

        if (result <= 0) {
            return -1;
        }

        sent += result;
    }

    return 0;
}

// ========================== 多线程处理函数 ==========================

/**
 * @brief USB数据发送线程函数
 */
void* usb_sender_thread(void* arg)
{
    (void)arg; // 避免未使用参数警告
    printf("USB sender thread started\n");

    while (running) {
        // 等待客户端连接
        if (!client_connected) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            printf("Waiting for client connection...\n");
            client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

            if (client_fd < 0) {
                if (running) {
                    perror("accept failed");
                }
                continue;
            }

            printf("Client connected from %s\n", inet_ntoa(client_addr.sin_addr));
            client_connected = 1;
        }

        // 等待新帧数据
        pthread_mutex_lock(&frame_mutex);
        while (current_frame.data == NULL && running) {
            pthread_cond_wait(&frame_ready, &frame_mutex);
        }

        if (current_frame.data && running) {
            // 发送帧数据
            if (send_frame(client_fd, current_frame.data, current_frame.size,
                          current_frame.frame_id, current_frame.timestamp) < 0) {
                printf("Client disconnected (frame %d)\n", current_frame.frame_id);
                close(client_fd);
                client_connected = 0;
                current_frame.data = NULL;
            } else {
                // 发送成功，清理当前帧
                current_frame.data = NULL;
            }
        }

        pthread_mutex_unlock(&frame_mutex);
    }

    if (client_connected) {
        close(client_fd);
    }

    printf("USB sender thread terminated\n");
    return NULL;
}

// ========================== 图像采集主循环 ==========================

/**
 * @brief 图像采集主循环函数 (使用libMedia)
 */
int capture_loop_libmedia()
{
    uint32_t frame_counter = 0;
    uint64_t last_stats_time = get_time_ns();
    uint32_t frames_in_second = 0;

    printf("Starting libMedia capture loop...\n");

    while (running) {
        media_frame_t frame;
        
        // 使用 libMedia 捕获帧 (超时1秒)
        int result = libmedia_session_capture_frame(media_session, &frame, 1000);
        
        if (result < 0) {
            if (libmedia_get_last_error() == MEDIA_ERROR_TIMEOUT) {
                printf("Timeout waiting for frame\n");
                continue;
            } else {
                printf("Frame capture failed: %s\n", 
                       libmedia_get_error_string(libmedia_get_last_error()));
                continue;
            }
        }

        uint64_t timestamp = get_time_ns();

        // 通知USB发送线程（仅在有客户端时）
        if (client_connected) {
            pthread_mutex_lock(&frame_mutex);
            current_frame.data = frame.data;
            current_frame.size = frame.size;
            current_frame.frame_id = frame_counter;
            current_frame.timestamp = timestamp;
            pthread_cond_signal(&frame_ready);
            pthread_mutex_unlock(&frame_mutex);
        }

        // 释放帧回到队列
        libmedia_session_release_frame(media_session, &frame);

        frame_counter++;
        frames_in_second++;

        // 统计信息 - 每5秒输出一次
        uint64_t current_time = get_time_ns();
        if (current_time - last_stats_time >= 5000000000ULL) {
            double fps = (double)frames_in_second * 1000000000.0 / (current_time - last_stats_time);
            printf("Frame %d, FPS: %.1f, Bytes: %zu, Connected: %s\n",
                   frame_counter, fps, frame.size, client_connected ? "YES" : "NO");
            frames_in_second = 0;
            last_stats_time = current_time;
        }
    }

    return 0;
}

// ========================== 程序主函数 ==========================

/**
 * @brief 程序入口点
 */
int main(int argc, char* argv[])
{
    const char* device = "/dev/video0";
    int port = DEFAULT_PORT;
    pthread_t usb_thread;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    printf("V4L2 USB RAW Image Streamer (libMedia版本) for Luckfox Pico Mini B\n");
    printf("================================================================\n");
    printf("Port: %d\n", port);
    printf("Server IP: %s\n", DEFAULT_SERVER_IP);
    printf("libMedia Version: %s\n", libmedia_get_version());

    // 初始化 libMedia
    if (libmedia_init() != 0) {
        printf("Failed to initialize libMedia\n");
        return -1;
    }

    // 设置调试级别
    libmedia_set_debug_level(3); // INFO级别

    // 检查系统资源
    printf("Checking system resources...\n");
    int system_ret = system("free -m | head -2 | tail -1 | awk '{print \"Memory: \" $3 \"/\" $2 \" MB used\"}'");
    (void)system_ret; // 显式忽略返回值

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    // 创建服务器
    server_fd = create_server(port);
    if (server_fd < 0) {
        goto cleanup;
    }

    // 配置 libMedia 会话
    media_session_config_t config = {
        .device_path = device,
        .format = {
            .width = WIDTH,
            .height = HEIGHT,
            .pixelformat = PIXELFORMAT,
            .num_planes = 1,
            .plane_size = {WIDTH * HEIGHT * 2} // 10位数据约为2字节/像素
        },
        .buffer_count = BUFFER_COUNT,
        .use_multiplanar = 1,
        .nonblocking = 0
    };

    // 创建 libMedia 会话
    media_session = libmedia_create_session(&config);
    if (!media_session) {
        printf("Failed to create media session: %s\n", 
               libmedia_get_error_string(libmedia_get_last_error()));
        goto cleanup;
    }

    printf("Created media session for device: %s\n", device);

    // 启动流
    if (libmedia_start_session(media_session) < 0) {
        printf("Failed to start media session: %s\n", 
               libmedia_get_error_string(libmedia_get_last_error()));
        goto cleanup;
    }

    printf("Media session started successfully\n");

    // 启动USB发送线程
    if (pthread_create(&usb_thread, NULL, usb_sender_thread, NULL) != 0) {
        perror("Failed to create USB thread");
        goto cleanup;
    }

    // 主采集循环 (使用libMedia)
    capture_loop_libmedia();

    // 等待线程结束
    pthread_join(usb_thread, NULL);

cleanup:
    if (media_session) {
        libmedia_stop_session(media_session);
        libmedia_destroy_session(media_session);
    }

    if (server_fd >= 0) {
        close(server_fd);
    }

    // 清理 libMedia
    libmedia_deinit();

    printf("Program terminated\n");
    return 0;
}
