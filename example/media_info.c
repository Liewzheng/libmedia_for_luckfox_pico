/**
 * @file media_info.c
 * @brief libMedia 设备信息查询示例
 *
 * 演示如何使用 libMedia 库查询和显示V4L2设备的详细信息。
 * 包括设备能力、支持的格式、分辨率等。
 *
 * @author Development Team
 * @date 2025-07-01
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>

// 引入 libMedia 头文件
#include "media.h"

// ========================== 工具函数 ==========================

/**
 * @brief 显示设备基本信息
 */
void show_device_info(const char* device_path)
{
    printf("=== Device Information ===\n");
    printf("Device: %s\n", device_path);
    
    // 打开设备
    int handle = libmedia_open_device(device_path);
    if (handle < 0) {
        printf("Failed to open device: %s\n", 
               libmedia_get_error_string(libmedia_get_last_error()));
        return;
    }
    
    // 检查设备能力
    if (libmedia_check_device_caps(handle) == 0) {
        printf("Device capabilities: OK (supports multiplanar streaming)\n");
    } else {
        printf("Device capabilities: Failed\n");
    }
    
    // 关闭设备
    libmedia_close_device(handle);
}

/**
 * @brief 显示支持的像素格式信息
 */
void show_format_info()
{
    printf("\n=== Supported Pixel Formats ===\n");
    
    // 常见的像素格式
    uint32_t formats[] = {
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_UYVY,
        V4L2_PIX_FMT_NV12,
        V4L2_PIX_FMT_NV21,
        V4L2_PIX_FMT_YUV420,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_BGR24,
        V4L2_PIX_FMT_RGB32,
        V4L2_PIX_FMT_BGR32,
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_JPEG,
        V4L2_PIX_FMT_H264,
        V4L2_PIX_FMT_SBGGR8,
        V4L2_PIX_FMT_SBGGR10,
        V4L2_PIX_FMT_SBGGR12,
        0
    };
    
    for (int i = 0; formats[i] != 0; i++) {
        const char* name = libmedia_get_format_name(formats[i]);
        int bpp = libmedia_get_bytes_per_pixel(formats[i]);
        printf("  %s (0x%08x): %d bytes/pixel\n", name, formats[i], bpp);
    }
}

/**
 * @brief 显示示例配置
 */
void show_example_configs()
{
    printf("\n=== Example Configurations ===\n");
    
    // 配置1: 标清YUYV
    printf("1. Standard Definition (YUYV):\n");
    printf("   Resolution: 640x480\n");
    printf("   Format: YUYV (0x%08x)\n", V4L2_PIX_FMT_YUYV);
    printf("   Frame size: %zu bytes\n", (size_t)(640 * 480 * 2));
    
    // 配置2: 高清MJPEG
    printf("\n2. High Definition (MJPEG):\n");
    printf("   Resolution: 1920x1080\n");
    printf("   Format: MJPEG (0x%08x)\n", V4L2_PIX_FMT_MJPEG);
    printf("   Frame size: Variable (compressed)\n");
    
    // 配置3: RAW格式
    printf("\n3. RAW Bayer (BGGR10):\n");
    printf("   Resolution: 1920x1080\n");
    printf("   Format: BGGR10 (0x%08x)\n", V4L2_PIX_FMT_SBGGR10);
    printf("   Frame size: %zu bytes\n", (size_t)(1920 * 1080 * 2));
}

/**
 * @brief 显示库信息
 */
void show_library_info()
{
    printf("\n=== libMedia Library Information ===\n");
    printf("Version: %s\n", libmedia_get_version());
    printf("Features:\n");
    printf("  - V4L2 multiplanar support\n");
    printf("  - High-level session management\n");
    printf("  - Memory mapped buffers\n");
    printf("  - Cross-platform compatibility\n");
    printf("  - Thread-safe operations\n");
}

/**
 * @brief 测试基本功能
 */
void test_basic_functions(const char* device_path)
{
    printf("\n=== Basic Function Test ===\n");
    
    printf("Testing libMedia initialization...\n");
    if (libmedia_init() != 0) {
        printf("FAILED: libMedia initialization\n");
        return;
    }
    printf("OK: libMedia initialized\n");
    
    printf("Testing device open...\n");
    int handle = libmedia_open_device(device_path);
    if (handle < 0) {
        printf("FAILED: Device open (%s)\n", 
               libmedia_get_error_string(libmedia_get_last_error()));
        libmedia_deinit();
        return;
    }
    printf("OK: Device opened (handle: %d)\n", handle);
    
    printf("Testing device capabilities...\n");
    if (libmedia_check_device_caps(handle) == 0) {
        printf("OK: Device capabilities check passed\n");
    } else {
        printf("WARNING: Device capabilities check failed\n");
    }
    
    printf("Testing device close...\n");
    libmedia_close_device(handle);
    printf("OK: Device closed\n");
    
    printf("Testing libMedia cleanup...\n");
    libmedia_deinit();
    printf("OK: libMedia cleanup completed\n");
}

// ========================== 程序主函数 ==========================

/**
 * @brief 程序入口点
 */
int main(int argc, char* argv[])
{
    const char* device = "/dev/video0";
    
    // 解析命令行参数
    if (argc > 1) {
        device = argv[1];
    }
    
    printf("libMedia Device Information Tool\n");
    printf("================================\n");
    
    // 显示库信息
    show_library_info();
    
    // 显示设备信息
    show_device_info(device);
    
    // 显示格式信息
    show_format_info();
    
    // 显示示例配置
    show_example_configs();
    
    // 测试基本功能
    test_basic_functions(device);
    
    printf("\n=== Usage Examples ===\n");
    printf("To query different device:\n");
    printf("  %s /dev/video1\n", argv[0]);
    printf("\nTo test capture:\n");
    printf("  ./media_simple 10    # Capture 10 frames\n");
    printf("\nTo start USB streaming:\n");
    printf("  ./media_usb 8888     # TCP server on port 8888\n");
    
    return 0;
}
