FILE(GLOB XVIDEO_TRANSCODE_SRC "./*.cpp")
FILE(GLOB XVIDEO_TRANSCODE_HEADER "./*.h")
FILE(GLOB XVIDEO_TRANSCODE_QRC "./*.qrc")
FILE(GLOB XVIDEO_TRANSCODE_UI "./*.ui")

add_executable(XVideoTranscode ${XVIDEO_TRANSCODE_SRC} ${XVIDEO_TRANSCODE_HEADER} ${XVIDEO_TRANSCODE_UI} ${XVIDEO_TRANSCODE_QRC})

TARGET_LINK_LIBRARIES(XVideoTranscode PRIVATE XCodec Qt6::Widgets Qt6::Core Qt6::Gui)
target_include_directories(XVideoTranscode PRIVATE "." "../xcodec" "${ThirdParty}/ffmpeg/include")
target_compile_definitions(XVideoTranscode PRIVATE -DUNICODE)
# 源文件为UTF-8, 声明编码避免中文被按GBK误读
if(WIN32)
    target_compile_options(XVideoTranscode PRIVATE /utf-8)
    TARGET_LINK_DIRECTORIES(XVideoTranscode PRIVATE "${ThirdParty}/ffmpeg/lib"  "${ThirdParty}/SDL2/lib" "${ThirdParty}/GLEW/lib")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale)
    target_link_libraries(XVideoTranscode PRIVATE ${FFMPEG_LIBRARIES} ${FFMPEG_LDFLAGS})
endif()

IF(WIN32)
add_custom_command(
        TARGET XVideoTranscode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/bin/
                \$\(OutDir\)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/opencv/bin/
                \$\(OutDir\))
ENDIF()
