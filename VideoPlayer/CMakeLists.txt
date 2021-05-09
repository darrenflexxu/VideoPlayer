FILE(GLOB_RECURSE VIDEO_PLAYER_SRC "*.cpp")
FILE(GLOB_RECURSE VIDEO_PLAYER_HEADER "*.h")

include_directories(.
"${ThirdParty}/ffmpeg/include"
"${ThirdParty}/SDL2/include"
)

source_group(Interface FILES "Interface/IVideoPlayer.h" "Interface/VideoPlayerEventHandle.h"
"Interface/types.h" "Interface/IVideoFrame.h")

link_directories("${ThirdParty}/ffmpeg/lib"
"${ThirdParty}/SDL2/lib"
)

ADD_LIBRARY(VideoPlayer SHARED ${VIDEO_PLAYER_SRC} ${VIDEO_PLAYER_HEADER})

TARGET_LINK_LIBRARIES(VideoPlayer avcodec avdevice avfilter avformat avutil postproc swresample swscale SDL2)