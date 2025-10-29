#pragma once

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

extern "C"{
// Include GLEW
#include <GL/glew.h>
}

#define ATTRIB_VERTEX 3  
#define ATTRIB_TEXTURE 4 

class XShaderYV12 {
  GLuint id_y;
  GLuint id_u;
  GLuint id_v; // Texture id  
  GLuint textureUniformY, textureUniformU,textureUniformV; 
  GLuint vertexbuffer, uvbuffer;
  GLuint p;

  GLsizei pixel_w;
  GLsizei pixel_h;
  int width_ = 0;
  int height_ = 0;

  int InitShader(double rotate);

 public:
  XShaderYV12(int w, int h, double rotate);

  bool Draw(
      const unsigned  char* y, int y_pitch,
      const unsigned  char* u, int u_pitch,
      const unsigned  char* v, int v_pitch
  );
};

