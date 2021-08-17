#include "GLContext.h"

GLContext::GLContext()
{
    this->hWnd = 0;
    this->hDC = 0;
    this->hRC = 0;
    this->format = 0;
}
GLContext::~GLContext()
{
}

void GLContext::SetupPixelFormat(HDC hDC) {
    int pixelFormat;

    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),  // size
        1,                          // version
        PFD_SUPPORT_OPENGL |        // OpenGL window
        PFD_DRAW_TO_WINDOW |        // render to window
        PFD_DOUBLEBUFFER,           // support double-buffering
        PFD_TYPE_RGBA,              // color type
        32,                         // prefered color depth
        0, 0, 0, 0, 0, 0,           // color bits (ignored)
        0,                          // no alpha buffer
        0,                          // alpha bits (ignored)
        0,                          // no accumulation buffer
        0, 0, 0, 0,                 // accum bits (ignored)
        16,                         // depth buffer
        0,                          // no stencil buffer
        0,                          // no auxiliary buffers
        PFD_MAIN_PLANE,             // main layer
        0,                          // reserved
        0, 0, 0,                    // no layer, visible, damage masks
    };

    pixelFormat = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pixelFormat, &pfd);
}

void GLContext::Setup(HWND hwnd, HDC hdc) {
    this->hWnd = hwnd;
    this->hDC = hdc;
    SetupPixelFormat(hDC);
    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

    //initialize glew
    glewExperimental = GL_TRUE;
    glewInit();
    if (AllocConsole())
    {
        freopen("CONOUT$", "w+t", stdout);
        freopen("CONOUT$", "w+t", stderr);
        const GLubyte* Devise = glGetString(GL_RENDERER);    //返回一个渲染器标识符，通常是个硬件平台  
        const GLubyte* str = glGetString(GL_VERSION);
        printf("OpenGL实现的版本号：%s\n", str);
        printf("硬件平台：%s\n", Devise);
    }
}