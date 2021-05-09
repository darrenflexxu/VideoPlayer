FILE(GLOB_RECURSE VIDEO_PLAYER_SRC "*.cpp")
FILE(GLOB_RECURSE VIDEO_PLAYER_HEADER "*.h")

include_directories(.
"${ThirdParty}/ffmpeg/include"
"${ThirdParty}/SDL2/include"
)

ADD_LIBRARY(VideoPlayer STATIC ${VIDEO_PLAYER_SRC} ${VIDEO_PLAYER_HEADER})