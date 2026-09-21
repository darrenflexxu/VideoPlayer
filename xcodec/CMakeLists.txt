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

# XQtOpenGLWidget 基于 Qt OpenGL，需链接 Qt 模块(含 AUTOMOC 所需的 moc)
target_link_libraries(XCodec PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGLWidgets)

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