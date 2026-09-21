#include "predefine_header.h"

#if TEXTURE_ROTATE  
static const GLfloat vertexVertices[] = {
  -1.0f, -0.5f,
  0.5f, -1.0f,
  -0.5f, 1.0f,
  1.0f, 0.5f,
};
#else  
static const GLfloat vertexVertices[] = {
  -1.0f, -1.0f, 0.0f,
  1.0f, -1.0f, 0.0f,
  1.0f, 1.0f, 0.0f,
  -1.0f, 1.0f, 0.0f
};
#endif  

#if TEXTURE_HALF  
static const GLfloat textureVertices[] = {
  0.0f, 1.0f,
  0.5f, 1.0f,
  0.0f, 0.0f,
  0.5f, 0.0f,
};
#else  
static const GLfloat textureVertices[] = {
  0.0f, 1.0f,
  1.0f, 1.0f,
  1.0f, 0.0f,
  0.0f, 0.0f,
};
#endif  

bool XShaderNV12::Draw(
  const unsigned  char* y, int y_pitch,
  const unsigned  char* uv, int u_pitch
) {
  //Clear
  glClear(GL_COLOR_BUFFER_BIT);

  //核心模式下属性状态保存在 VAO 中，绘制前必须重新绑定
  glBindVertexArray(vao);

  glUseProgram(p);

  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
  glVertexAttribPointer(
    0,                                // attribute. No particular reason for 1, but must match the layout in the shader.
    3,                                // size : U+V => 2
    GL_FLOAT,                         // type
    GL_FALSE,                         // normalized?
    0,                                // stride
    (void*)0                          // array buffer offset
  );
  glEnableVertexAttribArray(0);

  //     glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, textureVertices); 
  glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
  glVertexAttribPointer(
    1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
    2,                                // size : U+V => 2
    GL_FLOAT,                         // type
    GL_FALSE,                         // normalized?
    0,                                // stride
    (void*)0                          // array buffer offset
  );
  glEnableVertexAttribArray(1);

  //Y  
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, id_y);
  // 行宽按 pitch 指定, 纹理宽取可见宽度: 兼容硬解(VideoToolbox)带对齐填充的 linesize
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, y_pitch);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w, pixel_h, 0, GL_RED,
               GL_UNSIGNED_BYTE, y);
  //   glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
  glUniform1i(textureUniformY, 0);

  //UV  
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, id_uv);
  // NV12 的 UV 交织, 1 个纹素 = 2 字节(U,V), 故行宽(纹素)= pitch/2
  glPixelStorei(GL_UNPACK_ROW_LENGTH, u_pitch / 2);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, pixel_w / 2, pixel_h / 2, 0, GL_RG,
               GL_UNSIGNED_BYTE, uv);
  glUniform1i(textureUniformUV, 1);

  // Draw  
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  // Show 
  return true;
}

//Init Shader  
int XShaderNV12::InitShader() {
  std::string app_path = XTools::get_app_path();
  // Dark blue background
  glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

  GLuint VertexArrayID;
  glGenVertexArrays(1, &VertexArrayID);
  glBindVertexArray(VertexArrayID);
  vao = VertexArrayID;

  glGenBuffers(1, &uvbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(textureVertices), textureVertices, GL_STATIC_DRAW);

  glGenBuffers(1, &vertexbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertexVertices), vertexVertices, GL_STATIC_DRAW);

  GLint vertCompiled, fragCompiled, linked;

  GLint v, f;
  const char* vs, * fs;
  //Shader: step1  
  v = glCreateShader(GL_VERTEX_SHADER);
  f = glCreateShader(GL_FRAGMENT_SHADER);
  //Get source code  
//     vs = textFileRead("Shader.vsh");  
//     fs = textFileRead("Shader.fsh");  

  std::string strfv;
  std::ifstream fvStream(app_path + "/Shader_nv12.vsh", std::ios::in);
  if (fvStream.is_open()) {
    std::stringstream svStream;
    svStream << fvStream.rdbuf();
    strfv = svStream.str();
    fvStream.close();
    //       std::cout<<"vetex shader "<<strfv<<std::endl;
  } else {
    std::cerr << "vetex shader open failed." << std::endl;
  }

  std::string strff;
  std::ifstream ffStream(app_path + "/Shader_nv12.fsh", std::ios::in);
  if (ffStream.is_open()) {
    std::stringstream sfStream;
    sfStream << ffStream.rdbuf();
    strff = sfStream.str();
    ffStream.close();
    //       std::cout<<"fragment shader "<<strff<<std::endl;
  } else {
    std::cerr << "fragment shader open failed. " << std::endl;
  }

  vs = strfv.c_str();
  fs = strff.c_str();

  GLint Result = GL_FALSE;
  int InfoLogLength;

  //Shader: step2  
  glShaderSource(v, 1, &vs, NULL);
  glShaderSource(f, 1, &fs, NULL);
  //Shader: step3  
  glCompileShader(v);
  //Debug  
  glGetShaderiv(v, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(v, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
    glGetShaderInfoLog(v, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
    std::printf("%s\n", &VertexShaderErrorMessage[0]);
  }

  glCompileShader(f);
  glGetShaderiv(f, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(f, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
    glGetShaderInfoLog(f, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
    std::printf("%s\n", &VertexShaderErrorMessage[0]);
  }

  //Program: Step1  
  p = glCreateProgram();
  //Program: Step2  
  glAttachShader(p, v);
  glAttachShader(p, f);

  /* glBindAttribLocation(p, ATTRIB_VERTEX, "vertexIn");
   glBindAttribLocation(p, ATTRIB_TEXTURE, "textureIn"); */
   //Program: Step3  

  glLinkProgram(p);
  //Debug  
  glGetShaderiv(p, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(p, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
    glGetShaderInfoLog(p, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
    std::printf("%s\n", &VertexShaderErrorMessage[0]);
  }

  //Program: Step4  
  glUseProgram(p);


  //Get Uniform Variables Location  
  textureUniformY = glGetUniformLocation(p, "ourTextureY");
  textureUniformUV = glGetUniformLocation(p, "ourTextureUV");

  //     //Set Arrays  
  //     glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertexVertices);  
  //     //Enable it  
  //     glEnableVertexAttribArray(ATTRIB_VERTEX);      
  //     

      /*glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
      glEnableVertexAttribArray(ATTRIB_TEXTURE); */


      //Init Texture  
  glGenTextures(1, &id_y);
  glBindTexture(GL_TEXTURE_2D, id_y);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &id_uv);
  glBindTexture(GL_TEXTURE_2D, id_uv);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return 0;
}

XShaderNV12::XShaderNV12(int w, int h) {
  pixel_w = w;
  pixel_h = h;
  InitShader();
}
