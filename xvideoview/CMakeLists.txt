FILE(GLOB XVIDEO_VIEW_SRC "./*.cpp")
FILE(GLOB XVIDEO_VIEW_HEADER "./*.h")
FILE(GLOB XVIDEO_VIEW_QRC "./*.qrc")
FILE(GLOB XVIDEO_VIEW_UI "./*.ui")

add_executable(XVideoView ${XVIDEO_VIEW_SRC} ${XVIDEO_VIEW_HEADER} ${XVIDEO_VIEW_UI} ${XVIDEO_VIEW_QRC})

TARGET_LINK_LIBRARIES(XVideoView PRIVATE XCodec Qt6::Widgets Qt6::Core Qt6::Gui)
target_include_directories(XVideoView PRIVATE "." "../xcodec")
target_compile_definitions(XVideoView PRIVATE -DUNICODE)
if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    target_include_directories(XVideoView PRIVATE ${SDL2_INCLUDE_DIRS})
    # Use pkg-config for linker flags (includes -L path and -lSDL2)
    target_link_libraries(XVideoView PRIVATE ${SDL2_LDFLAGS})
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
