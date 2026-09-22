FILE(GLOB XVIDEO_EDITOR_SRC "./*.cpp")
FILE(GLOB XVIDEO_EDITOR_HEADER "./*.h")
#共享 xvideoview 的 QOpenGLWidget 预览控件
set(XVIDEO_EDITOR_SHARED "../xvideoview/x_qt_opengl_widget.cpp")

add_executable(XVideoEditor ${XVIDEO_EDITOR_SRC} ${XVIDEO_EDITOR_HEADER} ${XVIDEO_EDITOR_SHARED})

TARGET_LINK_LIBRARIES(XVideoEditor PRIVATE XCodec Qt6::Widgets Qt6::Core Qt6::Gui Qt6::OpenGLWidgets)
target_include_directories(XVideoEditor PRIVATE "." "../xcodec" "../xvideoview")
target_compile_definitions(XVideoEditor PRIVATE -DUNICODE)

# UI 层的 QOpenGLWidget 渲染使用 XCodec 导出的着色器与 GLEW/OpenGL, 并直接操作 AVFrame
if(WIN32)
    target_include_directories(XVideoEditor PRIVATE
        "${ThirdParty}/ffmpeg/include"
        "${ThirdParty}/GLEW/include")
    link_directories("${ThirdParty}/ffmpeg/lib" "${ThirdParty}/GLEW/lib")
    target_link_libraries(XVideoEditor PRIVATE avutil glew32 opengl32)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    find_library(GLEW_LIBRARY GLEW)
    find_library(OPENGL_LIBRARY OpenGL)
    target_include_directories(XVideoEditor PRIVATE
        ${SDL2_INCLUDE_DIRS}
        "${ThirdParty}/ffmpeg/include"
        "${ThirdParty}/GLEW/include")
    target_link_libraries(XVideoEditor PRIVATE ${SDL2_LDFLAGS} ${GLEW_LIBRARY} ${OPENGL_LIBRARY})
    pkg_check_modules(AVUTIL REQUIRED libavutil)
    target_link_libraries(XVideoEditor PRIVATE ${AVUTIL_LIBRARIES} ${AVUTIL_LDFLAGS})
endif()

IF(WIN32)
add_custom_command(
        TARGET XVideoEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/bin/
                \$\(OutDir\)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/opencv/bin/
                \$\(OutDir\))
ENDIF()