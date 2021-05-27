CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

PROJECT(VideoProc)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Bin)

set(ThirdParty "${PROJECT_SOURCE_DIR}/thirdparty")

IF(WIN32)
SET(CMAKE_PREFIX_PATH C:/Qt/Qt5.10.0/5.10.0/msvc2015_64/lib/cmake)
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

include_directories("${PROJECT_SOURCE_DIR}/FramelessHelper"
"${PROJECT_SOURCE_DIR}/VideoPlayer/Interface"
"${PROJECT_SOURCE_DIR}/Form"
)

add_subdirectory(VideoPlayer)

IF(WIN32)
add_subdirectory(FramelessHelper)
add_subdirectory(Form)
ENDIF()

IF(WIN32)
set(SRC "${PROJECT_SOURCE_DIR}/main.cpp")
ELSE(WIN32)
set(SRC "${PROJECT_SOURCE_DIR}/main_console.cpp")
ENDIF()
FILE(GLOB HEADER "${PROJECT_SOURCE_DIR}/*.h")

IF(WIN32)
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

link_directories("${ThirdParty}/ffmpeg/lib"
"${ThirdParty}/SDL2/lib"
)
ENDIF()

add_definitions(-DUNICODE)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC} ${HEADER} ${UI} ${QRC} ${QM_FILES}) 

TARGET_LINK_LIBRARIES(${PROJECT_NAME} Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick VideoPlayer)

IF(WIN32)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} Qt5::Widgets Qt5::OpenGL Qt5::WinExtras OpenGL32 FramelessHelper Form)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ThirdParty}/bin/
                \$\(OutDir\))
ENDIF()
