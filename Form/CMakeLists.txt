FILE(GLOB_RECURSE FORM_SRC "*.cpp")
FILE(GLOB_RECURSE FORM_HEADER "*.h")
FILE(GLOB_RECURSE FORM_UI "*.ui")

include_directories(.
../VideoPlayer
)

add_definitions(-DUNICODE)

ADD_LIBRARY(Form STATIC ${FORM_SRC} ${FORM_HEADER} ${FORM_UI})

TARGET_LINK_LIBRARIES(Form Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::OpenGL Qt5::WinExtras OpenGL32 Glu32)