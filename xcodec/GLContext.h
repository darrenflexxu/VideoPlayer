#pragma once

#include <windows.h>
#include <GL/glew.h>

class GLContext {
protected:
  int         _format;
  HWND        _hWnd;
  HDC         _hDC;
  HGLRC       _hRC;
public:
  GLContext();
  ~GLContext();

  bool    setup(HWND hWnd, HDC hDC);
  void    shutdown();
  void    swapBuffer();
};
