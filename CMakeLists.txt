CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

PROJECT(VideoProc)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Bin)

set(ThirdParty "${PROJECT_SOURCE_DIR}/thirdparty")

IF(WIN32)
SET(CMAKE_PREFIX_PATH C:/Qt/Qt5.12.12/5.12.12/msvc2017_64/lib/cmake;${ThirdParty}/opencv/lib)
find_package(OpenCV REQUIRED)
ELSE(WIN32)
SET(CMAKE_PREFIX_PATH /usr/local/Qt-5.9.9/lib/cmake)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fPIC")
ENDIF()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5Gui REQUIRED)
FIND_PACKAGE(Qt5Core REQUIRED)

IF(WIN32)
FIND_PACKAGE(Qt5OpenGL REQUIRED)
FIND_PACKAGE(Qt5WinExtras REQUIRED)
ENDIF()

FIND_PACKAGE(Qt5LinguistTools REQUIRED)
FIND_PACKAGE(Qt5Qml REQUIRED)
FIND_PACKAGE(Qt5Quick REQUIRED)

include_directories(
    "${PROJECT_SOURCE_DIR}/xvideoview"
)

add_subdirectory(xcodec)
add_definitions(-DUNICODE)

FILE(GLOB XVIDEO_VIEW_SRC "${PROJECT_SOURCE_DIR}/xvideoview/*.cpp")
FILE(GLOB XVIDEO_VIEW_HEADER "${PROJECT_SOURCE_DIR}/xvideoview/*.h")
FILE(GLOB XVIDEO_VIEW_QRC "${PROJECT_SOURCE_DIR}/xvideoview/*.qrc")
FILE(GLOB XVIDEO_VIEW_UI "${PROJECT_SOURCE_DIR}/xvideoview/*.ui")

add_executable(XVideoView ${XVIDEO_VIEW_SRC} ${XVIDEO_VIEW_HEADER} ${XVIDEO_VIEW_UI} ${XVIDEO_VIEW_QRC})

TARGET_LINK_LIBRARIES(XVideoView PRIVATE XCodec Qt5::Widgets Qt5::Core Qt5::Gui)
TARGET_LINK_DIRECTORIES(XVideoView PRIVATE "${ThirdParty}/ffmpeg/lib" "${ThirdParty}/SDL2/lib" "${ThirdParty}/GLEW/lib")

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
