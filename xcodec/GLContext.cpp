#include "GLContext.h"
#include <iostream>

GLContext::GLContext() {
    _format = 0;
    _hWnd = 0;
    _hDC = 0;
    _hRC = 0;
}

GLContext::~GLContext() {
    shutdown();
}

bool GLContext::setup(HWND hWnd, HDC hDC) {

    _hWnd = hWnd;
    _hDC = hDC;
    unsigned PixelFormat;
    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0,
        0,
        0
    };

    if (_format == 0) {
        PixelFormat = ChoosePixelFormat(_hDC, &pfd);
    } else {
        PixelFormat = _format;
    }

    if (!SetPixelFormat(_hDC, PixelFormat, &pfd)) {
        return  false;
    }
    _hRC = wglCreateContext(_hDC);
    if (!wglMakeCurrent(_hDC, _hRC)) {
        return  false;
    }
    glewExperimental = GL_TRUE;
    glewInit();
    if (AllocConsole()) {
        freopen("CONOUT$", "w+t", stdout);
        freopen("CONOUT$", "w+t", stderr);
        const GLubyte* Devise = glGetString(GL_RENDERER);
        const GLubyte* str = glGetString(GL_VERSION);
        printf("OpenGL实现的版本号：%s\n", str);
        printf("硬件平台：%s\n", Devise);
    }
    return  true;
}

void    GLContext::shutdown() {
    if (_hRC != NULL) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(_hRC);
        _hRC = NULL;
    }

    if (_hDC != NULL) {
        ReleaseDC(_hWnd, _hDC);
        _hDC = NULL;
    }
}

void GLContext::swapBuffer() {
    SwapBuffers(_hDC);
}
