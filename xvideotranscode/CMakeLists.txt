FILE(GLOB XVIDEO_TRANSCODE_SRC "./*.cpp")
FILE(GLOB XVIDEO_TRANSCODE_HEADER "./*.h")
FILE(GLOB XVIDEO_TRANSCODE_QRC "./*.qrc")
FILE(GLOB XVIDEO_TRANSCODE_UI "./*.ui")

add_executable(XVideoTranscode ${XVIDEO_TRANSCODE_SRC} ${XVIDEO_TRANSCODE_HEADER} ${XVIDEO_TRANSCODE_UI} ${XVIDEO_TRANSCODE_QRC})

TARGET_LINK_LIBRARIES(XVideoTranscode PRIVATE XCodec Qt5::Widgets Qt5::Core Qt5::Gui)
TARGET_LINK_DIRECTORIES(XVideoTranscode PRIVATE "${ThirdParty}/ffmpeg/lib"  "${ThirdParty}/SDL2/lib" "${ThirdParty}/GLEW/lib")
target_include_directories(XVideoTranscode PRIVATE "." "../xcodec" "${ThirdParty}/ffmpeg/include")
target_compile_definitions(XVideoTranscode PRIVATE -DUNICODE)

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
