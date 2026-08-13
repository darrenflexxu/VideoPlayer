cmake_minimum_required(VERSION 3.6)

# 命令行版转码工具, 只依赖XCodec(不链接Qt/OpenCV/SDL)
add_executable(XVideoTranscodeCLI main.cpp)

TARGET_LINK_LIBRARIES(XVideoTranscodeCLI PRIVATE XCodec)
target_link_directories(XVideoTranscodeCLI PRIVATE
  "${ThirdParty}/ffmpeg/lib"
  "${ThirdParty}/SDL2/lib"
  "${ThirdParty}/GLEW/lib")
target_include_directories(XVideoTranscodeCLI PRIVATE
  "../xcodec"
  "${ThirdParty}/ffmpeg/include")
# 源码为UTF-8(GUI等旧目录是GBK, 这里单独声明, 避免中文被按GBK误读)
target_compile_options(XVideoTranscodeCLI PRIVATE /utf-8)

IF(WIN32)
add_custom_command(
        TARGET XVideoTranscodeCLI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/bin/
                \$\(OutDir\)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/opencv/bin/
                \$\(OutDir\))
ENDIF()