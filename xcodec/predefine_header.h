#pragma once

#include "xaudio_play.h"
#include "xencode.h" 
#include "xmux_task.h"
#include "xvideo_filterproc.h"
#include "xplayer.h"
#include "xconvertor.h"
#include "xsdl.h"
#include "xshader.h"
#include "xshader_nv12.h"
#include "xshader_yv12.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/display.h>
}

#ifdef WIN32
#include <d3d9.h>
#include <windows.h>
#endif

#include <SDL.h>
#include <sstream>
#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>