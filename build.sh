#!/bin/bash

# ============================================================================
# libMedia 动态库构建脚本
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
INSTALL_PREFIX="/usr/local"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_step() {
    echo -e "${GREEN}[STEP]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 解析命令行参数
BUILD_TYPE="Release"
CLEAN_BUILD=false
INSTALL=false
CROSS_COMPILE=false
BUILD_EXAMPLES=true

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -x|--cross-compile)
            CROSS_COMPILE=true
            shift
            ;;
        --no-examples)
            BUILD_EXAMPLES=false
            shift
            ;;
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        -h|--help)
            cat << EOF
Usage: $0 [OPTIONS]

Options:
    -d, --debug           Build in debug mode
    -c, --clean           Clean build directory before building
    -i, --install         Install after building
    -x, --cross-compile   Enable cross-compilation for Luckfox Pico
    --no-examples         Disable building example programs
    --prefix PREFIX       Installation prefix (default: /usr/local)
    -h, --help            Show this help message

Examples:
    $0                              # Build in release mode with examples
    $0 -d                           # Build in debug mode
    $0 -c -i                        # Clean, build, and install
    $0 -x -i --prefix /opt/libmedia # Cross-compile and install to custom prefix
    $0 --no-examples                # Build library only, no examples

EOF
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

print_step "libMedia 动态库构建开始"

# 显示构建配置
echo "构建配置:"
echo "  构建类型: $BUILD_TYPE"
echo "  安装前缀: $INSTALL_PREFIX"
echo "  交叉编译: $([ "$CROSS_COMPILE" = true ] && echo "是" || echo "否")"
echo "  清理构建: $([ "$CLEAN_BUILD" = true ] && echo "是" || echo "否")"
echo "  自动安装: $([ "$INSTALL" = true ] && echo "是" || echo "否")"
echo "  编译示例: $([ "$BUILD_EXAMPLES" = true ] && echo "是" || echo "否")"
echo

# 检查依赖
print_step "检查构建依赖"

if ! command -v cmake &> /dev/null; then
    print_error "CMake 未安装。请安装 CMake 3.16 或更高版本。"
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+')
print_step "CMake 版本: $CMAKE_VERSION"

# 检查交叉编译工具链
if [ "$CROSS_COMPILE" = true ]; then
    TOOLCHAIN_PATH="/home/liewzheng/Workspace/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc"
    if [ ! -f "$TOOLCHAIN_PATH" ]; then
        print_error "未找到 Luckfox Pico 交叉编译工具链: $TOOLCHAIN_PATH"
        print_error "请确保已安装 Luckfox Pico SDK"
        exit 1
    fi
    print_step "交叉编译工具链: $TOOLCHAIN_PATH"
fi

# 清理构建目录
if [ "$CLEAN_BUILD" = true ] && [ -d "$BUILD_DIR" ]; then
    print_step "清理构建目录"
    rm -rf "$BUILD_DIR"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置 CMake
print_step "配置 CMake"

CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
    -DBUILD_EXAMPLES="$([ "$BUILD_EXAMPLES" = true ] && echo "ON" || echo "OFF")"
)

if [ "$CROSS_COMPILE" = true ]; then
    CMAKE_ARGS+=(
        -DENABLE_CROSS_COMPILE=ON
        -DCMAKE_SYSTEM_NAME=Linux
        -DCMAKE_SYSTEM_PROCESSOR=arm
    )
fi

cmake "${CMAKE_ARGS[@]}" ..

# 编译
print_step "编译 libMedia"
make -j$(nproc)

# 检查编译结果
if [ -f "libmedia.so" ]; then
    print_step "编译成功！"
    
    # 显示库信息
    echo "库文件信息:"
    ls -la libmedia.so*
    
    if command -v file &> /dev/null; then
        echo
        echo "文件类型:"
        file libmedia.so
    fi
    
    if [ "$CROSS_COMPILE" = true ] && command -v readelf &> /dev/null; then
        echo
        echo "目标架构:"
        readelf -h libmedia.so | grep Machine
    fi
    
    # 检查导出符号
    if command -v nm &> /dev/null; then
        echo
        echo "导出符号 (前10个):"
        nm -D libmedia.so | grep ' T ' | head -10
    fi
else
    print_error "编译失败！"
    exit 1
fi

# 安装
if [ "$INSTALL" = true ]; then
    print_step "安装 libMedia"
    
    if [ "$EUID" -ne 0 ] && [[ "$INSTALL_PREFIX" == /usr* ]]; then
        print_warning "需要 root 权限安装到系统目录"
        sudo make install
    else
        make install
    fi
    
    print_step "安装完成"
    
    # 更新动态库缓存
    if [ "$EUID" -eq 0 ] || command -v sudo &> /dev/null; then
        print_step "更新动态库缓存"
        if [ "$EUID" -eq 0 ]; then
            ldconfig
        else
            sudo ldconfig
        fi
    fi
    
    # 显示安装的文件
    echo "已安装的文件:"
    echo "  库文件: $INSTALL_PREFIX/lib/libmedia.so*"
    echo "  头文件: $INSTALL_PREFIX/include/media.h"
    echo "  pkg-config: $INSTALL_PREFIX/lib/pkgconfig/libmedia.pc"
fi

print_step "构建完成！"

# 显示使用说明
cat << EOF

libMedia 动态库已构建完成！

使用方法:
1. 编译时链接:
   gcc -o myapp myapp.c -lmedia

2. pkg-config 方式:
   gcc -o myapp myapp.c \$(pkg-config --cflags --libs libmedia)

3. CMake 项目中使用:
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(LIBMEDIA REQUIRED libmedia)
   target_link_libraries(myapp \${LIBMEDIA_LIBRARIES})

4. 运行时库路径 (如果安装到非标准位置):
   export LD_LIBRARY_PATH=$INSTALL_PREFIX/lib:\$LD_LIBRARY_PATH

EOF
