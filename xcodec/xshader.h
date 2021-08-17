#ifndef __WINDOW_VIEW__
#define __WINDOW_VIEW__

#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <vector>
#include "xvideo_view.h"

extern "C"{
// Include GLEW
#include <GL/glew.h>
}

#define ATTRIB_VERTEX 3  
#define ATTRIB_TEXTURE 4 

class XShader :public XVideoView {
  GLuint id_y;
  GLuint id_u;
  GLuint id_v; // Texture id  
  GLuint textureUniformY, textureUniformU,textureUniformV; 
  GLuint vertexbuffer, uvbuffer;
  GLuint p;

  GLsizei pixel_w;
  GLsizei pixel_h;

  int InitShader();
public:
  XShader();

  void Close() override;
  ////////////////////////////////////////////////
  /// 初始化渲染窗口 线程安全
  /// @para w 窗口宽度
  /// @para h 窗口高度
  /// @para fmt 绘制的像素格式
  /// @para win_id 窗口句柄，如果为空，创建新窗口
  /// @return 是否创建成功
  bool Init(int w, int h,
            Format fmt = RGBA) override;

  //////////////////////////////////////////////////
  /// 渲染图像 线程安全
  ///@para data 渲染的二进制数据
  ///@para linesize 一行数据的字节数，对于YUV420P就是Y一行字节数
  /// linesize<=0 就根据宽度和像素格式自动算出大小
  /// @return 渲染是否成功
  bool Draw(const unsigned  char* data,
            int linesize = 0) override;
  bool Draw(
      const unsigned  char* y, int y_pitch,
      const unsigned  char* u, int u_pitch,
      const unsigned  char* v, int v_pitch
  ) override;

  bool IsExit() override;
};
#endif