FILE(GLOB_RECURSE FORM_SRC "*.cpp")
FILE(GLOB_RECURSE FORM_HEADER "*.h")
FILE(GLOB_RECURSE FORM_UI "*.ui_")

include_directories(.
../VideoPlayer
"${ThirdParty}/ffmpeg/include"
)

add_definitions(-DUNICODE)

link_directories("${ThirdParty}/ffmpeg/lib"
)

ADD_LIBRARY(Form STATIC ${FORM_SRC} ${FORM_HEADER} ${FORM_UI})

TARGET_LINK_LIBRARIES(Form Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::OpenGL Qt5::WinExtras OpenGL32 Glu32 avcodec avutil)