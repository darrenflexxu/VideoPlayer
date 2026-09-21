cmake_minimum_required(VERSION 3.16)

# XCodec 外部应用示例(独立工程, 不属于主构建)
# 演示以动态链接方式引入已安装的 XCodec:
#   1. 先安装 XCodec 到某个前缀:
#        cmake --install <xcodec-build> --prefix /path/to/xcodec-install
#   2. 配置本示例并指定该前缀:
#        cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/xcodec-install
#        cmake --build build
# 说明: XCodec 公共头文件会引用 FFmpeg 的类型定义, 故编译期需提供 FFmpeg 头文件;
#       FFmpeg/SDL2/OpenCV/GLEW/Qt 等运行时依赖由使用方自行保证(本工程不打包)

project(XCodecConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(XCodec REQUIRED)

add_executable(xcodec_consumer main.cpp)
target_link_libraries(xcodec_consumer PRIVATE XCodec::XCodec)

# 编译期 FFmpeg 头文件: 优先 pkg-config, 其次 FFMPEG_ROOT
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(FFMPEG QUIET libavutil libavcodec)
endif()
if(FFMPEG_FOUND)
  target_include_directories(xcodec_consumer PRIVATE ${FFMPEG_INCLUDE_DIRS})
elseif(DEFINED FFMPEG_ROOT)
  target_include_directories(xcodec_consumer PRIVATE ${FFMPEG_ROOT}/include)
else()
  message(FATAL_ERROR "未找到 FFmpeg 头文件: 请设置 -DFFMPEG_ROOT=<ffmpeg> 或提供 pkg-config")
endif()
