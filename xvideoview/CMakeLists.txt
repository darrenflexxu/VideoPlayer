FILE(GLOB XVIDEO_VIEW_SRC "./*.cpp")
FILE(GLOB XVIDEO_VIEW_HEADER "./*.h")
FILE(GLOB XVIDEO_VIEW_QRC "./*.qrc")
FILE(GLOB XVIDEO_VIEW_UI "./*.ui")

add_executable(XVideoView ${XVIDEO_VIEW_SRC} ${XVIDEO_VIEW_HEADER} ${XVIDEO_VIEW_UI} ${XVIDEO_VIEW_QRC})

TARGET_LINK_LIBRARIES(XVideoView PRIVATE XCodec Qt5::Widgets Qt5::Core Qt5::Gui)
TARGET_LINK_DIRECTORIES(XVideoView PRIVATE "${ThirdParty}/ffmpeg/lib" "${ThirdParty}/SDL2/lib" "${ThirdParty}/GLEW/lib")
target_include_directories(XVideoView PRIVATE "." "../xcodec")
target_compile_definitions(XVideoView PRIVATE -DUNICODE)

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
