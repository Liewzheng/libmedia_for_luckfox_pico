/**
 * @file media_simple.c
 * @brief libMedia 简单示例程序
 *
 * 演示如何使用 libMedia 库进行基本的视频捕获操作。
 * 这是一个最小化的示例，展示了库的基本用法。
 *
 * @author Development Team
 * @date 2025-07-01
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

// 引入 libMedia 头文件
#include "media.h"

// ========================== 全局变量 ==========================

/** @brief 程序运行状态标志 */
volatile int running = 1;

/** @brief libMedia 会话句柄 */
media_session_t* session = NULL;

// ========================== 工具函数 ==========================

/**
 * @brief 信号处理函数
 */
void signal_handler(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
}

/**
 * @brief 打印帧信息
 */
void print_frame_info(const media_frame_t* frame, int frame_count)
{
    printf("Frame %d: %zu bytes, timestamp: %lu ns\n", 
           frame_count, frame->size, (unsigned long)frame->timestamp);
}

// ========================== 程序主函数 ==========================

/**
 * @brief 程序入口点
 */
int main(int argc, char* argv[])
{
    const char* device = "/dev/video0";
    int frame_count = 0;
    int max_frames = 100; // 默认捕获100帧

    // 解析命令行参数
    if (argc > 1) {
        max_frames = atoi(argv[1]);
        if (max_frames <= 0) {
            max_frames = 100;
        }
    }

    printf("libMedia Simple Capture Example\n");
    printf("===============================\n");
    printf("Device: %s\n", device);
    printf("Max frames: %d\n", max_frames);
    printf("libMedia Version: %s\n", libmedia_get_version());

    // 初始化 libMedia
    if (libmedia_init() != 0) {
        printf("Failed to initialize libMedia\n");
        return -1;
    }

    // 设置调试级别
    libmedia_set_debug_level(2); // WARNING级别

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 配置会话
    media_session_config_t config = {
        .device_path = device,
        .format = {
            .width = 640,
            .height = 480,
            .pixelformat = V4L2_PIX_FMT_YUYV, // 使用更常见的格式
            .num_planes = 1,
            .plane_size = {640 * 480 * 2} // YUYV为2字节/像素
        },
        .buffer_count = 4,
        .use_multiplanar = 0, // 使用单平面
        .nonblocking = 0
    };

    // 创建会话
    session = libmedia_create_session(&config);
    if (!session) {
        printf("Failed to create media session: %s\n", 
               libmedia_get_error_string(libmedia_get_last_error()));
        goto cleanup;
    }

    printf("Created media session successfully\n");

    // 启动捕获
    if (libmedia_start_session(session) < 0) {
        printf("Failed to start session: %s\n", 
               libmedia_get_error_string(libmedia_get_last_error()));
        goto cleanup;
    }

    printf("Session started, beginning capture...\n");

    // 捕获循环
    while (running && frame_count < max_frames) {
        media_frame_t frame;
        
        // 捕获帧
        int result = libmedia_session_capture_frame(session, &frame, 1000);
        
        if (result < 0) {
            media_error_t error = libmedia_get_last_error();
            if (error == MEDIA_ERROR_TIMEOUT) {
                printf("Timeout waiting for frame\n");
                continue;
            } else {
                printf("Frame capture failed: %s\n", 
                       libmedia_get_error_string(error));
                break;
            }
        }

        // 处理帧数据
        frame_count++;
        print_frame_info(&frame, frame_count);

        // 这里可以添加帧数据处理代码
        // 例如：保存到文件、图像处理等

        // 释放帧
        libmedia_session_release_frame(session, &frame);

        // 每10帧暂停一下
        if (frame_count % 10 == 0) {
            printf("Captured %d frames, continuing...\n", frame_count);
            usleep(100000); // 100ms
        }
    }

    printf("\nCapture completed. Total frames: %d\n", frame_count);

cleanup:
    if (session) {
        libmedia_stop_session(session);
        libmedia_destroy_session(session);
    }

    libmedia_deinit();
    return 0;
}
