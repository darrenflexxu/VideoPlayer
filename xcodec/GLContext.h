#pragma once
#include <windows.h>
#include <GL/glew.h>
#include <iostream>

class GLContext
{
public:
    GLContext();
    ~GLContext();
    void Setup(HWND,HDC);
    void SetupPixelFormat(HDC);
private:
    HWND hWnd;
    HDC hDC;
    HGLRC hRC;
    int format;
};