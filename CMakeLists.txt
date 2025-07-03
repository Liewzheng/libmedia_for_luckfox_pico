cmake_minimum_required(VERSION 3.16)
project(libMedia VERSION 1.0.0 LANGUAGES C)

# ============================================================================
# 编译选项配置
# ============================================================================

# 设置 C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译标志
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -fPIC")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")

# ============================================================================
# 交叉编译工具链配置
# ============================================================================

# 添加选项来控制是否启用交叉编译
option(ENABLE_CROSS_COMPILE "Enable cross-compilation for Luckfox Pico" OFF)

# 检测是否需要交叉编译（针对 Luckfox Pico ARM 设备）
if(ENABLE_CROSS_COMPILE AND EXISTS "/home/liewzheng/Workspace/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc")
    message(STATUS "Configuring for Luckfox Pico cross-compilation")
    
    # 设置交叉编译工具链路径
    set(TOOLCHAIN_PREFIX "/home/liewzheng/Workspace/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf")
    set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc")
    set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}/bin/arm-rockchip830-linux-uclibcgnueabihf-g++")
    set(CMAKE_AR "${TOOLCHAIN_PREFIX}/bin/arm-rockchip830-linux-uclibcgnueabihf-ar")
    set(CMAKE_STRIP "${TOOLCHAIN_PREFIX}/bin/arm-rockchip830-linux-uclibcgnueabihf-strip")
    
    # 设置系统根目录
    set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_PREFIX}")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    
    # ARM 架构特定标志
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
    
    # 设置交叉编译标志
    set(CMAKE_CROSSCOMPILING TRUE)
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR arm)
else()
    message(STATUS "Configuring for native compilation")
    set(CMAKE_CROSSCOMPILING FALSE)
endif()

# ============================================================================
# 源文件和头文件配置
# ============================================================================

# 头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 源文件列表
set(LIBMEDIA_SOURCES
    source/media.c
)

# 头文件列表（用于安装）
set(LIBMEDIA_HEADERS
    include/media.h
)

# ============================================================================
# 库目标构建
# ============================================================================

# 创建动态库
add_library(media SHARED ${LIBMEDIA_SOURCES})

# 设置库的版本信息
set_target_properties(media PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "media"
    PUBLIC_HEADER "${LIBMEDIA_HEADERS}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ============================================================================
# 编译定义和链接库
# ============================================================================

# 编译定义
target_compile_definitions(media PRIVATE
    LIBMEDIA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    LIBMEDIA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    LIBMEDIA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# 链接系统库
target_link_libraries(media PRIVATE
    pthread
    rt
)

# ============================================================================
# 安装配置
# ============================================================================

# 安装库文件
install(TARGETS media
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# 创建 pkg-config 文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libmedia.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libmedia.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libmedia.pc
    DESTINATION lib/pkgconfig
)

# ============================================================================
# 调试和测试选项
# ============================================================================

# 提供调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building libMedia in Debug mode")
    target_compile_definitions(media PRIVATE MEDIA_DEBUG=1)
endif()

# ============================================================================
# 示例程序编译
# ============================================================================

# 选项：是否编译示例程序
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    # 示例程序源文件
    set(EXAMPLE_SOURCES
        example/media_simple.c
        example/media_usb.c
        example/media_info.c
    )
    
    # 为每个示例创建可执行文件
    foreach(EXAMPLE_SRC ${EXAMPLE_SOURCES})
        # 提取文件名（不含扩展名）作为目标名
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)
        
        # 创建可执行文件
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC})
        
        # 链接 libMedia 库
        target_link_libraries(${EXAMPLE_NAME} PRIVATE media)
        
        # 设置头文件路径
        target_include_directories(${EXAMPLE_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        
        # 链接系统库
        target_link_libraries(${EXAMPLE_NAME} PRIVATE pthread rt)
        
        # 设置输出目录
        set_target_properties(${EXAMPLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples
        )
        
        message(STATUS "Added example: ${EXAMPLE_NAME}")
    endforeach()
    
    # 安装示例程序（可选）
    install(TARGETS media_simple media_usb media_info
        RUNTIME DESTINATION bin/examples
        OPTIONAL
    )
    
    # 创建示例程序说明文件
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/example/README.md
        ${CMAKE_CURRENT_BINARY_DIR}/examples/README.md
        COPYONLY
    )
endif()

# 显示编译信息
message(STATUS "libMedia Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C Flags: ${CMAKE_C_FLAGS}")
message(STATUS "  Cross Compiling: ${CMAKE_CROSSCOMPILING}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build Examples: ${BUILD_EXAMPLES}")
