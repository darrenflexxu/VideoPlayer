FILE(GLOB XVIDEO_VIEW_SRC "./*.cpp")
FILE(GLOB XVIDEO_VIEW_HEADER "./*.h")
FILE(GLOB XVIDEO_VIEW_QRC "./*.qrc")
FILE(GLOB XVIDEO_VIEW_UI "./*.ui")

add_executable(XVideoView ${XVIDEO_VIEW_SRC} ${XVIDEO_VIEW_HEADER} ${XVIDEO_VIEW_UI} ${XVIDEO_VIEW_QRC})

TARGET_LINK_LIBRARIES(XVideoView PRIVATE XCodec Qt6::Widgets Qt6::Core Qt6::Gui Qt6::OpenGLWidgets)
target_include_directories(XVideoView PRIVATE "." "../xcodec")
target_compile_definitions(XVideoView PRIVATE -DUNICODE)

# UI 层的 QOpenGLWidget 渲染使用 XCodec 导出的着色器与 GLEW/OpenGL, 并直接操作 AVFrame
if(WIN32)
    target_include_directories(XVideoView PRIVATE
        "${ThirdParty}/ffmpeg/include"
        "${ThirdParty}/GLEW/include")
    link_directories("${ThirdParty}/ffmpeg/lib" "${ThirdParty}/GLEW/lib")
    target_link_libraries(XVideoView PRIVATE avutil glew32 opengl32)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    find_library(GLEW_LIBRARY GLEW)
    find_library(OPENGL_LIBRARY OpenGL)
    target_include_directories(XVideoView PRIVATE
        ${SDL2_INCLUDE_DIRS}
        "${ThirdParty}/ffmpeg/include"
        "${ThirdParty}/GLEW/include")
    # Use pkg-config for linker flags (includes -L path and -lSDL2)
    target_link_libraries(XVideoView PRIVATE ${SDL2_LDFLAGS} ${GLEW_LIBRARY} ${OPENGL_LIBRARY})
    # libavutil: 渲染控件对解码帧做 av_frame_ref/av_frame_free
    pkg_check_modules(AVUTIL REQUIRED libavutil)
    target_link_libraries(XVideoView PRIVATE ${AVUTIL_LIBRARIES} ${AVUTIL_LDFLAGS})
endif()

IF(WIN32)
add_custom_command(
        TARGET XVideoView POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/bin/
                \$\(OutDir\)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/opencv/bin/
                \$\(OutDir\))
ENDIF()
