#pragma once

#ifdef WIN32
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
#else
#include <SDL.h>
#include <GL/glew.h>

class GLContext {
protected:
    SDL_Window* _window;
    SDL_GLContext _glContext;
public:
    GLContext();
    ~GLContext();

    bool    setup(SDL_Window* window);
    void    shutdown();
    void    swapBuffer();
};
#endif
