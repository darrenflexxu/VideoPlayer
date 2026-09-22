#include "predefine_header.h"
#include "xshader_rgba.h"

#include <vector>

//XShaderRGBA 的 GLSL 内嵌在代码中, 不依赖运行时 shader 文件, 便于叠加层合成
static const char* kRgbaVertexShader =
    "#version 330 core\n"
    "layout(location = 0) in vec3 vertexIn;\n"
    "layout(location = 1) in vec2 textureIn;\n"
    "out vec2 textureOut;\n"
    "void main(void)\n"
    "{\n"
    "  gl_Position = vec4(vertexIn, 1);\n"
    "  textureOut = textureIn;\n"
    "}\n";

static const char* kRgbaFragmentShader =
    "#version 330 core\n"
    "in vec2 textureOut;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform float uAlpha;\n"
    "void main(void)\n"
    "{\n"
    "  vec4 c = texture(uTex, textureOut);\n"
    "  FragColor = vec4(c.rgb, c.a * uAlpha);\n"
    "}\n";

static const GLfloat kRgbaVertexVertices[] = {
    -1.0f, -1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    -1.0f, 1.0f, 0.0f
};

static const GLfloat kRgbaTextureVertices[] = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 0.0f
};

void XShaderRGBA::EnableBlend(bool on) {
  if (on) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  } else {
    glDisable(GL_BLEND);
  }
}

bool XShaderRGBA::Draw(const unsigned char* rgba, int pitch, int w, int h, float alpha) {
  if (!rgba)
    return false;
  if (alpha <= 0.0f)
    return true;

  // 核心模式下属性状态保存在 VAO 中, 绘制前必须重新绑定
  glBindVertexArray(vao);
  glUseProgram(p);

  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(1);

  // 上传 RGBA 纹理(行宽按 pitch, 兼容带对齐填充)
  // 注意: GL_UNPACK_ROW_LENGTH 以"像素"为单位(RGBA=4字节/像素), 这里换算字节行距
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureRGBA);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >= 4 ? pitch / 4 : w);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, rgba);
  glUniform1i(textureUniform, 0);
  glUniform1f(alphaUniform, alpha > 1.0f ? 1.0f : alpha);

  // alpha 混合在 EnableBlend 中由合成器统一控制, 此处只画
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  return true;
}

//Init Shader
int XShaderRGBA::InitShader() {
  GLuint VertexArrayID;
  glGenVertexArrays(1, &VertexArrayID);
  glBindVertexArray(VertexArrayID);
  vao = VertexArrayID;

  glGenBuffers(1, &uvbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kRgbaTextureVertices),
               kRgbaTextureVertices, GL_STATIC_DRAW);

  glGenBuffers(1, &vertexbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kRgbaVertexVertices),
               kRgbaVertexVertices, GL_STATIC_DRAW);

  GLint v, f;
  v = glCreateShader(GL_VERTEX_SHADER);
  f = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(v, 1, &kRgbaVertexShader, NULL);
  glShaderSource(f, 1, &kRgbaFragmentShader, NULL);
  glCompileShader(v);
  glCompileShader(f);

  GLint Result = GL_FALSE;
  int InfoLogLength;
  glGetShaderiv(v, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(v, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> err(InfoLogLength + 1);
    glGetShaderInfoLog(v, InfoLogLength, NULL, &err[0]);
    fprintf(stderr, "XShaderRGBA: vertex shader log: %s\n", &err[0]);
  }
  glGetShaderiv(f, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(f, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> err(InfoLogLength + 1);
    glGetShaderInfoLog(f, InfoLogLength, NULL, &err[0]);
    fprintf(stderr, "XShaderRGBA: fragment shader log: %s\n", &err[0]);
  }

  p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glBindAttribLocation(p, 0, "vertexIn");
  glBindAttribLocation(p, 1, "textureIn");
  glLinkProgram(p);
  glGetProgramiv(p, GL_LINK_STATUS, &Result);
  glGetProgramiv(p, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> err(InfoLogLength + 1);
    glGetProgramInfoLog(p, InfoLogLength, NULL, &err[0]);
    fprintf(stderr, "XShaderRGBA: link log: %s\n", &err[0]);
  }
  glUseProgram(p);

  textureUniform = glGetUniformLocation(p, "uTex");
  alphaUniform = glGetUniformLocation(p, "uAlpha");

  glGenTextures(1, &textureRGBA);
  glBindTexture(GL_TEXTURE_2D, textureRGBA);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return 0;
}

XShaderRGBA::XShaderRGBA(int w, int h) {
  pixel_w = w;
  pixel_h = h;
  InitShader();
}