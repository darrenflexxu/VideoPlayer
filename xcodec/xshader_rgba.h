#pragma once

#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" {
// Include GLEW
#include <GL/glew.h>
}

#include "xtools.h"

/// RGBA 纹理着色器: 用于贴纸/字幕等透明叠加层
/// 与 YV12/NV12 着色器的区别: 支持 alpha 混合(GL_BLEND)与统一透明度
/// 不含 glClear: 清屏与多图层合成由外层合成器统一处理, 本类只画自己的四边形
class XCODEC_API XShaderRGBA {
  GLuint textureRGBA;            // Texture id
  GLuint textureUniform;         // uniform sampler2D
  GLuint alphaUniform;           // uniform float
  GLuint vertexbuffer, uvbuffer;
  GLuint vao = 0;                // 核心模式必须绑定 VAO
  GLuint p;

  GLsizei pixel_w;
  GLsizei pixel_h;

  int InitShader();

 public:
  XShaderRGBA(int w, int h);

  // 上传 RGBA 图并绘制(带 alpha 混合), pitch 为一行字节数
  // w/h 为图像实际尺寸; alpha: 整体透明度 0~1(与纹理自带 alpha 相乘);
  // rect 交给外层的 glViewport 控制(uv 0..1 映射整张纹理, 顶点与画布无关)
  bool Draw(const unsigned char* rgba, int pitch, int w, int h, float alpha);

  // 显式开关 alpha 混合(合成器统一控制)
  static void EnableBlend(bool on);
};