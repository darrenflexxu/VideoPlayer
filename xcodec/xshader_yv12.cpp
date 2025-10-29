#include "predefine_header.h"

#if TEXTURE_ROTATE  
static const GLfloat vertexVertices[] = {
  -1.0f, -0.5f,
  0.5f, -1.0f,
  -0.5f, 1.0f,
  1.0f, 0.5f,
};
#else  
static GLfloat vertexVertices[] = {
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

bool XShaderYV12::Draw(
  const unsigned  char* y, int y_pitch,
  const unsigned  char* u, int u_pitch,
  const unsigned  char* v, int v_pitch
) {
  //Clear
  glClear(GL_COLOR_BUFFER_BIT);

  glLinkProgram(p);

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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, y_pitch, pixel_h, 0, GL_RED,
               GL_UNSIGNED_BYTE, y);
  //   glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
  glUniform1i(textureUniformY, 0);

  //U  
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, id_u);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, u_pitch, pixel_h / 2, 0, GL_RED,
               GL_UNSIGNED_BYTE, u);
  glUniform1i(textureUniformU, 1);

  //V  
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, id_v);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, v_pitch, pixel_h / 2, 0, GL_RED,
               GL_UNSIGNED_BYTE, v);
  glUniform1i(textureUniformV, 2);

  // Draw  
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  // Show 
  return true;
}

void rotateVertices(GLfloat* vertices, int numVertices, float angle) {
  float rad = angle * M_PI / 180.0f;  // 角度转弧度
  float cosA = cosf(rad);
  float sinA = sinf(rad);

  for (int i = 0; i < numVertices; ++i) {
    float x = vertices[i * 3 + 0];
    float y = vertices[i * 3 + 1];
    float newX = x * cosA - y * sinA;
    float newY = x * sinA + y * cosA;
    vertices[i * 3 + 0] = newX;
    vertices[i * 3 + 1] = newY;
    // z不变
  }
}

//Init Shader  
int XShaderYV12::InitShader(double rotate) {
  char pBuf[MAX_PATH] = {0};                                 //存放路径的变量     
  GetCurrentDirectory(MAX_PATH, pBuf);                   //获取程序的当前目录
  std::string app_path = pBuf;
  // Dark blue background
  glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

  GLuint VertexArrayID;
  glGenVertexArrays(1, &VertexArrayID);
  glBindVertexArray(VertexArrayID);

  glGenBuffers(1, &uvbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(textureVertices), textureVertices, GL_STATIC_DRAW);

  glGenBuffers(1, &vertexbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);

  if (std::fabs(rotate) > FLT_EPSILON) {
    rotateVertices(vertexVertices, 4, rotate);
  }
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
  std::ifstream fvStream(app_path + "\\Shader.vsh", std::ios::in);
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
  std::ifstream ffStream(app_path + "\\Shader.fsh", std::ios::in);
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
  textureUniformY = glGetUniformLocation(p, "tex_y");
  textureUniformU = glGetUniformLocation(p, "tex_u");
  textureUniformV = glGetUniformLocation(p, "tex_v");

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

  glGenTextures(1, &id_u);
  glBindTexture(GL_TEXTURE_2D, id_u);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &id_v);
  glBindTexture(GL_TEXTURE_2D, id_v);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return 0;
}

XShaderYV12::XShaderYV12(int w, int h, double rotate) {
  pixel_w = w;
  pixel_h = h;
  InitShader(rotate);
}

