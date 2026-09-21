FILE(GLOB_RECURSE VIDEO_PLAYER_SRC "*.cpp")
FILE(GLOB_RECURSE VIDEO_PLAYER_HEADER "*.h")

include_directories(.
"${ThirdParty}/ffmpeg/include"
"${ThirdParty}/SDL2/include"
"${ThirdParty}/GLEW/include"
)

ADD_LIBRARY(XCodec SHARED ${VIDEO_PLAYER_SRC} ${VIDEO_PLAYER_HEADER})

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
endif()