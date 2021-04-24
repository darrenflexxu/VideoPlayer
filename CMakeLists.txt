CMAKE_MINIMUM_REQUIRED(VERSION 3.8)

PROJECT(VideoProc)

set(ThirdParty "${PROJECT_SOURCE_DIR}/thirdparty")

SET(CMAKE_PREFIX_PATH C:/Qt/Qt5.10.0/5.10.0/msvc2015_64/lib/cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5Gui REQUIRED)
FIND_PACKAGE(Qt5Core REQUIRED)
FIND_PACKAGE(Qt5OpenGL REQUIRED)
FIND_PACKAGE(Qt5LinguistTools REQUIRED)
FIND_PACKAGE(Qt5Qml REQUIRED)
FIND_PACKAGE(Qt5Quick REQUIRED)
FIND_PACKAGE(Qt5WinExtras REQUIRED)

include_directories("${PROJECT_SOURCE_DIR}/FramelessHelper"
"${PROJECT_SOURCE_DIR}/VideoPlayer"
"${PROJECT_SOURCE_DIR}/Form"
"${ThirdParty}/ffmpeg/include"
"${ThirdParty}/SDL2/include"
)

add_subdirectory(FramelessHelper)
add_subdirectory(VideoPlayer)
add_subdirectory(Form)

FILE(GLOB SRC "${PROJECT_SOURCE_DIR}/*.cpp")
FILE(GLOB HEADER "${PROJECT_SOURCE_DIR}/*.h")
FILE(GLOB UI "${PROJECT_SOURCE_DIR}/*.ui")
FILE(GLOB QRC "${PROJECT_SOURCE_DIR}/*.qrc")

SOURCE_GROUP(UI FILES ${UI})
SOURCE_GROUP(Resouces FILES ${QRC})

set(TS_FILES ${PROJECT_SOURCE_DIR}/translate/lang_zh.ts)
set(QM_FILES ${PROJECT_SOURCE_DIR}/translate/lang_zh.qm)

foreach(_ts_file ${TS_FILES})
  add_custom_command(
           OUTPUT ${_ts_file}
           COMMAND ${Qt5_LUPDATE_EXECUTABLE} -recursive 
                      ${CMAKE_CURRENT_SOURCE_DIR} -ts ${_ts_file}
           COMMENT "Updating ${_ts_file}"
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )
endforeach()

foreach(_ts_file ${TS_FILES})
  add_custom_command(
           OUTPUT ${QM_FILES}
           COMMAND ${Qt5_LRELEASE_EXECUTABLE} -verbose 
                      ${_ts_file} -qm ${QM_FILES}
           COMMENT "Updating ${_ts_file}"
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )
endforeach()

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC} ${HEADER} ${UI} ${QRC} ${QM_FILES}) 

TARGET_LINK_LIBRARIES(${PROJECT_NAME} Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::OpenGL Qt5::WinExtras OpenGL32 Glu32 FramelessHelper VideoPlayer Form)