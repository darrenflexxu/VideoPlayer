FILE(GLOB_RECURSE VIDEO_PLAYER_SRC "*.cpp")
FILE(GLOB_RECURSE VIDEO_PLAYER_HEADER "*.h")

include_directories(.
"${ThirdParty}/ffmpeg/include"
"${ThirdParty}/SDL2/include"
"${ThirdParty}/GLEW/include"
"${OpenCV_INCLUDE_DIRS}"
)

link_directories("${ThirdParty}/ffmpeg/lib"
"${ThirdParty}/SDL2/lib"
"${ThirdParty}/GLEW/lib"
)

add_definitions(-DXCODEC_EXPORTS)

ADD_LIBRARY(XCodec SHARED ${VIDEO_PLAYER_SRC} ${VIDEO_PLAYER_HEADER})

target_precompile_headers(XCodec PRIVATE predefine_header.h)

TARGET_LINK_LIBRARIES(XCodec avcodec avdevice avfilter avformat avutil postproc swresample swscale SDL2 glew32 opengl32 "${OpenCV_LIBS}")