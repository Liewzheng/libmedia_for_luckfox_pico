# libMedia - Universal Media Device Library

libMedia 是一个通用的媒体设备库，专为 V4L2 视频采集设备设计。它提供了简化的 API 来抽象底层的 V4L2 操作，使相机和视频设备管理变得更加容易。

## 📋 功能特性

### 核心功能
- **设备管理**: 打开、关闭、枚举 V4L2 设备
- **格式配置**: 设置视频格式、分辨率、像素格式
- **缓冲区管理**: 多平面缓冲区申请、映射、队列管理
- **流控制**: 启动/停止视频流
- **帧采集**: 高效的图像帧捕获和释放
- **会话管理**: 高级会话接口，简化使用

### 兼容性支持
- **V4L2 多平面 API**: 完整支持多平面视频采集
- **v4l2_usb.c 兼容**: 提供兼容接口，便于现有代码迁移
- **交叉编译**: 支持 Luckfox Pico ARM 设备交叉编译
- **调试支持**: 分级调试输出，便于问题排查

## 🚀 快速开始

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libv4l-dev

# CentOS/RHEL
sudo yum install gcc gcc-c++ cmake kernel-headers
```

### 编译安装

```bash
# 克隆或进入项目目录
cd libMedia

# 标准编译
./build.sh

# 调试版本
./build.sh --debug

# 交叉编译 (Luckfox Pico)
./build.sh --cross-compile

# 编译并安装
./build.sh --install

# 清理重新编译
./build.sh --clean --install
```

### 基本使用示例

```c
#include <media.h>

int main() {
    // 初始化库
    libmedia_init();
    
    // 打开设备
    int handle = libmedia_open_device("/dev/video0");
    if (handle < 0) {
        printf("Failed to open device\n");
        return -1;
    }
    
    // 设置格式
    media_format_t format = {
        .width = 1920,
        .height = 1080,
        .pixelformat = V4L2_PIX_FMT_SBGGR10,
        .num_planes = 1
    };
    
    libmedia_set_format(handle, &format);
    
    // 申请缓冲区
    int buffer_count = libmedia_request_buffers(handle, 4);
    
    // 启动流
    libmedia_start_streaming(handle);
    
    // 采集帧
    media_frame_t frame;
    if (libmedia_capture_frame(handle, &frame, 1000) == 0) {
        printf("Captured frame: %zu bytes\n", frame.size);
        
        // 释放帧
        libmedia_release_frame(handle, &frame);
    }
    
    // 停止流
    libmedia_stop_streaming(handle);
    
    // 关闭设备
    libmedia_close_device(handle);
    
    // 清理库
    libmedia_deinit();
    
    return 0;
}
```

### 高级会话接口

```c
#include <media.h>

int main() {
    libmedia_init();
    
    // 配置会话
    media_session_config_t config = {
        .device_path = "/dev/video0",
        .format = {
            .width = 1920,
            .height = 1080,
            .pixelformat = V4L2_PIX_FMT_SBGGR10,
            .num_planes = 1
        },
        .buffer_count = 4,
        .use_multiplanar = 1,
        .nonblocking = 0
    };
    
    // 创建会话
    media_session_t* session = libmedia_create_session(&config);
    if (!session) {
        printf("Failed to create session\n");
        return -1;
    }
    
    // 启动会话
    libmedia_start_session(session);
    
    // 采集循环
    for (int i = 0; i < 100; i++) {
        media_frame_t frame;
        if (libmedia_session_capture_frame(session, &frame, 1000) == 0) {
            printf("Frame %d: %zu bytes\n", i, frame.size);
            libmedia_session_release_frame(session, &frame);
        }
    }
    
    // 停止会话
    libmedia_stop_session(session);
    
    // 销毁会话
    libmedia_destroy_session(session);
    
    libmedia_deinit();
    return 0;
}
```

## 🔧 v4l2_usb.c 迁移指南

为了帮助现有的 `v4l2_usb.c` 代码迁移到 libMedia，我们提供了兼容性接口：

### 原始代码
```c
// v4l2_usb.c 中的代码
int fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
check_device_caps(fd);
set_format_mp(fd, &fmt);
request_buffers_mp(fd, buffers, BUFFER_COUNT);
start_streaming_mp(fd);
```

### 迁移后代码
```c
#include <media.h>

// 使用 libMedia 接口
libmedia_init();
int handle = libmedia_open_device("/dev/video0");
libmedia_check_device_caps(handle);
struct v4l2_format fmt;
libmedia_set_format_mp(handle, &fmt, WIDTH, HEIGHT, PIXELFORMAT);
int buffer_count = libmedia_request_buffers(handle, BUFFER_COUNT);
libmedia_start_streaming_mp(handle);
```

### 兼容性接口对照表

| 原始函数 | libMedia 兼容函数 | 说明 |
|---------|------------------|------|
| `xioctl()` | `libmedia_xioctl()` | IOCTL 包装函数 |
| `check_device_caps()` | `libmedia_check_device_caps()` | 设备能力检查 |
| `set_format_mp()` | `libmedia_set_format_mp()` | 多平面格式设置 |
| `queue_buffer_mp()` | `libmedia_queue_buffer_mp()` | 缓冲区入队 |
| `dequeue_buffer_mp()` | `libmedia_dequeue_buffer_mp()` | 缓冲区出队 |
| `start_streaming_mp()` | `libmedia_start_streaming_mp()` | 启动流 |
| `stop_streaming_mp()` | `libmedia_stop_streaming_mp()` | 停止流 |

## 📦 编译集成

### 使用 CMake

```cmake
# 方法1: pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMEDIA REQUIRED libmedia)

target_include_directories(your_target PRIVATE ${LIBMEDIA_INCLUDE_DIRS})
target_link_libraries(your_target ${LIBMEDIA_LIBRARIES})

# 方法2: 直接链接
target_link_libraries(your_target media)
```

### 使用 Makefile

```makefile
# 编译标志
CFLAGS += $(shell pkg-config --cflags libmedia)
LDFLAGS += $(shell pkg-config --libs libmedia)

# 或者直接链接
LDFLAGS += -lmedia

your_program: your_program.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

### 使用 GCC

```bash
# 直接编译
gcc -o myapp myapp.c -lmedia

# 使用 pkg-config
gcc -o myapp myapp.c $(pkg-config --cflags --libs libmedia)
```

## 🐛 调试和问题排查

### 启用调试输出

```c
// 设置调试级别
libmedia_set_debug_level(3); // 0=无, 1=错误, 2=警告, 3=信息, 4=调试
```

### 错误处理

```c
// 检查错误
media_error_t error = libmedia_get_last_error();
if (error != MEDIA_ERROR_NONE) {
    printf("Error: %s\n", libmedia_get_error_string(error));
}
```

### 常见问题

1. **设备打开失败**
   ```bash
   # 检查设备权限
   ls -la /dev/video*
   # 添加用户到 video 组
   sudo usermod -a -G video $USER
   ```

2. **库文件找不到**
   ```bash
   # 更新动态库缓存
   sudo ldconfig
   # 或设置 LD_LIBRARY_PATH
   export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
   ```

3. **交叉编译问题**
   ```bash
   # 确保工具链路径正确
   ls /opt/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/
   ```

## 📊 性能特性

- **零拷贝**: 使用内存映射，避免数据拷贝
- **多线程安全**: 支持多线程并发访问
- **低延迟**: 优化的缓冲区管理
- **内存效率**: 智能的资源管理和自动清理

## 🔖 支持的格式

- **YUV 格式**: YUYV, UYVY, NV12, NV21, YUV420
- **RGB 格式**: RGB24, BGR24, RGB32, BGR32, RGB565
- **RAW 格式**: BGGR8, BGGR10, BGGR12, RGGB8, RGGB10, RGGB12
- **压缩格式**: MJPEG, JPEG, H264

## 📄 API 参考

完整的 API 文档请参考 `include/media.h` 头文件中的注释。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📜 许可证

本项目采用 MIT 许可证 - 详情请查看 LICENSE 文件。
