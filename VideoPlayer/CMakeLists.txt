FILE(GLOB_RECURSE VIDEO_PLAYER_SRC "*.cpp")
FILE(GLOB_RECURSE VIDEO_PLAYER_HEADER "*.h")
FILE(GLOB_RECURSE VIDEO_PLAYER_UI "*.ui")

include_directories(.
"${ThirdParty}/ffmpeg/include"
"${ThirdParty}/SDL2/include"
)

ADD_LIBRARY(VideoPlayer STATIC ${VIDEO_PLAYER_SRC} ${VIDEO_PLAYER_HEADER} ${VIDEO_PLAYER_UI})

TARGET_LINK_LIBRARIES(VideoPlayer Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::OpenGL Qt5::WinExtras OpenGL32 Glu32)