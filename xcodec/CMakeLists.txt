FILE(GLOB_RECURSE VIDEO_PLAYER_SRC "*.cpp")
FILE(GLOB_RECURSE VIDEO_PLAYER_HEADER "*.h")

include_directories(.
"${ThirdParty}/ffmpeg/include"
"${ThirdParty}/SDL2/include"
)

link_directories("${ThirdParty}/ffmpeg/lib"
"${ThirdParty}/SDL2/lib"
)

add_definitions(-DXCODEC_EXPORTS)

ADD_LIBRARY(XCodec SHARED ${VIDEO_PLAYER_SRC} ${VIDEO_PLAYER_HEADER})

TARGET_LINK_LIBRARIES(XCodec avcodec avdevice avfilter avformat avutil postproc swresample swscale SDL2)