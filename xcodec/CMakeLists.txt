FILE(GLOB_RECURSE VIDEO_PLAYER_SRC "*.cpp")
FILE(GLOB_RECURSE VIDEO_PLAYER_HEADER "*.h")

if(APPLE)
    list(APPEND VIDEO_PLAYER_SRC macos_window.mm)
    set_source_files_properties(macos_window.mm PROPERTIES LANGUAGE OBJCXX)
endif()

include_directories(.
"${ThirdParty}/ffmpeg/include"
"${ThirdParty}/SDL2/include"
"${ThirdParty}/GLEW/include"
)

ADD_LIBRARY(XCodec SHARED ${VIDEO_PLAYER_SRC} ${VIDEO_PLAYER_HEADER})

# 公共头文件目录: 自身编译用源码目录, 外部应用安装后使用 include/XCodec
target_include_directories(XCodec PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/XCodec>)

# 编译 XCodec 自身时导出符号(与 xtools.h 中的 XCODEC_API 对应)
target_compile_definitions(XCodec PRIVATE XCODEC_EXPORTS)

# 注意: XCodec 不依赖 Qt, Qt 渲染实现位于 UI 层(xvideoview/x_qt_opengl_widget.*)

# 源码为UTF-8, 强制/utf-8保证字面量为UTF-8, 与CLI(SetConsoleOutputCP(CP_UTF8))一致, 否则中文/进度条字符编译成GBK乱码
if(WIN32)
    target_compile_options(XCodec PRIVATE /utf-8)
    link_directories("${ThirdParty}/ffmpeg/lib"
    "${ThirdParty}/SDL2/lib"
    "${ThirdParty}/GLEW/lib"
    )
    target_link_libraries(XCodec PRIVATE avcodec avdevice avfilter avformat avutil postproc swresample swscale SDL2 glew32 opengl32 opencv_core opencv_imgproc opencv_imgcodecs opencv_videoio)
else()
    # On macOS/Linux, use Homebrew/system libraries
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale)
    pkg_check_modules(OPENCV REQUIRED opencv5)
    find_library(SDL2_LIBRARY SDL2)
    find_library(GLEW_LIBRARY GLEW)
    find_library(OPENGL_LIBRARY OpenGL)
    
    target_precompile_headers(XCodec PRIVATE predefine_header.h)
    
    # FFMPEG_LIBRARIES contains flags like -L... -lavcodec -lavdevice ...
    target_link_libraries(XCodec PRIVATE ${FFMPEG_LIBRARIES} ${FFMPEG_LDFLAGS} ${OPENCV_LIBRARIES} ${OPENCV_LDFLAGS} ${SDL2_LIBRARY} ${GLEW_LIBRARY} ${OPENGL_LIBRARY})
    if(APPLE)
        find_library(APPKIT AppKit)
        target_link_libraries(XCodec PRIVATE ${APPKIT})
    endif()
endif()

# ------------------------------------------------------------------
# 安装与导出: 供外部应用程序以动态链接方式引入
#   install 后可用: find_package(XCodec REQUIRED)
#                   target_link_libraries(app PRIVATE XCodec::XCodec)
# 依赖(FFmpeg/SDL2/OpenCV/GLEW/Qt)由使用方自行提供, 此处不打包
# ------------------------------------------------------------------
set(XCODEC_VERSION 1.0.0)

install(TARGETS XCodec
    EXPORT XCodecTargets
    RUNTIME DESTINATION bin      # Windows: XCodec.dll
    LIBRARY DESTINATION lib      # macOS/Linux: libXCodec.dylib/.so
    ARCHIVE DESTINATION lib)     # Windows: XCodec.lib 导入库

# 安装公共头文件 -> <prefix>/include/XCodec/*.h
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/XCodec
    FILES_MATCHING PATTERN "*.h")

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/XCodecConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/XCodecConfig.cmake
    INSTALL_DESTINATION lib/cmake/XCodec)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/XCodecConfigVersion.cmake
    VERSION ${XCODEC_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/XCodecConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/XCodecConfigVersion.cmake
    DESTINATION lib/cmake/XCodec)

install(EXPORT XCodecTargets
    NAMESPACE XCodec::
    DESTINATION lib/cmake/XCodec)
