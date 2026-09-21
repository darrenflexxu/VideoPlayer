#include "GLContext.h"
#include <iostream>

#ifdef WIN32
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
#else
GLContext::GLContext() : _window(nullptr), _glContext(nullptr) {
}

GLContext::~GLContext() {
    shutdown();
}

bool GLContext::setup(SDL_Window* window) {
    _window = window;
    // Request OpenGL 3.3 core profile on macOS
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    
    _glContext = SDL_GL_CreateContext(window);
    if (!_glContext) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return false;
    }
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    printf("OpenGL实现的版本号：%s\n", version);
    printf("硬件平台：%s\n", renderer);
    return true;
}

void GLContext::shutdown() {
    if (_glContext) {
        SDL_GL_DeleteContext(_glContext);
        _glContext = nullptr;
    }
    _window = nullptr;
}

void GLContext::swapBuffer() {
    if (_window) {
        SDL_GL_SwapWindow(_window);
    }
}
#endif
