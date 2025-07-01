# libMedia 示例程序

本目录包含了使用 libMedia 库的示例程序，展示了库的各种功能和用法。

## 📋 示例程序列表

### 1. media_simple - 基础捕获示例

**功能描述**：
- 演示 libMedia 的基本使用方法
- 简单的视频帧捕获和显示
- 适合初学者学习库的基本概念

**使用方法**：
```bash
# 捕获默认100帧
./media_simple

# 捕获指定数量的帧
./media_simple 50
```

**代码要点**：
- 会话创建和配置
- 基本的帧捕获循环
- 错误处理
- 资源清理

### 2. media_usb - USB流传输示例

**功能描述**：
- 基于原 v4l2_usb.c 重构的版本
- 使用 libMedia 库进行视频采集
- 通过TCP Socket实时传输图像数据
- 多线程架构，支持并发采集和传输

**使用方法**：
```bash
# 使用默认端口8888
./media_usb

# 指定TCP端口
./media_usb 9999
```

**代码要点**：
- 高级会话管理
- 多线程编程
- 网络数据传输
- 实时性能优化

### 3. media_info - 设备信息查询

**功能描述**：
- 查询和显示V4L2设备信息
- 显示支持的像素格式
- 提供配置示例
- 基本功能测试

**使用方法**：
```bash
# 查询默认设备 /dev/video0
./media_info

# 查询指定设备
./media_info /dev/video1
```

**代码要点**：
- 设备能力查询
- 格式信息获取
- 库功能测试
- 信息展示

## 🚀 编译和运行

### 前置条件

```bash
# 确保 libMedia 已编译
cd /path/to/libMedia
./build.sh

# 或者使用CMake直接编译
mkdir build && cd build
cmake ..
make
```

### 编译示例程序

```bash
# 方法1: 使用构建脚本（推荐）
./build.sh

# 方法2: 使用CMake（示例程序会自动编译）
cd build
make

# 示例程序位于 build/examples/ 目录
ls build/examples/
```

### 运行示例

```bash
cd build/examples

# 运行设备信息查询
./media_info

# 运行简单捕获示例
./media_simple 10

# 运行USB流传输（需要摄像头设备）
./media_usb 8888
```

## 📖 学习路径

### 初学者

1. **从 media_info 开始**：了解设备信息和库功能
2. **学习 media_simple**：掌握基本的视频捕获流程
3. **进阶到 media_usb**：学习实际应用开发

### 开发者

1. **分析代码结构**：理解库的设计模式
2. **自定义配置**：修改示例以适应你的需求
3. **性能优化**：学习多线程和内存管理技巧

## 🔧 常见问题

### Q: 编译时找不到 media.h

**解决方案**：
```bash
# 确保头文件路径正确
export C_INCLUDE_PATH=/usr/local/include:$C_INCLUDE_PATH
# 或者在编译时指定
gcc -I../include example.c -lmedia
```

### Q: 运行时找不到 libmedia.so

**解决方案**：
```bash
# 更新动态库路径
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
# 或者更新系统库缓存
sudo ldconfig
```

### Q: 摄像头设备打开失败

**解决方案**：
```bash
# 检查设备权限
ls -la /dev/video*
# 添加用户到video组
sudo usermod -a -G video $USER
# 重新登录或使用 sudo 运行
```

### Q: 没有摄像头设备怎么测试？

**解决方案**：
```bash
# 使用v4l2loopback创建虚拟摄像头
sudo modprobe v4l2loopback
# 或者修改示例代码中的设备路径
```

## 📚 代码分析

### 错误处理模式

所有示例都遵循统一的错误处理模式：

```c
if (function_call() < 0) {
    printf("Error: %s\n", libmedia_get_error_string(libmedia_get_last_error()));
    goto cleanup;
}
```

### 资源管理模式

确保所有资源都得到正确释放：

```c
cleanup:
    if (session) {
        libmedia_stop_session(session);
        libmedia_destroy_session(session);
    }
    libmedia_deinit();
```

### 配置参数模式

使用结构体来配置会话参数：

```c
media_session_config_t config = {
    .device_path = "/dev/video0",
    .format = { /* ... */ },
    .buffer_count = 4,
    /* ... */
};
```

## 🎯 扩展建议

1. **保存图像到文件**：在 media_simple 基础上添加文件保存功能
2. **图像格式转换**：实现不同像素格式之间的转换
3. **性能基准测试**：测量帧率、延迟等性能指标
4. **GUI界面**：使用GTK或Qt为示例添加图形界面
5. **网络客户端**：为 media_usb 编写配套的客户端程序

## 📝 许可证

这些示例程序与 libMedia 库使用相同的 MIT 许可证。
