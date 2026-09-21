#import <AppKit/AppKit.h>
#include <SDL.h>
#include <stdio.h>

extern "C" {

SDL_Window* SDL_CreateWindowFromNSView(void* nsView) {
    NSView* view = (NSView*)nsView;
    NSWindow* window = [view window];
    printf("SDL_CreateWindowFromNSView: nsView=%p, window=%p\n", nsView, window);
    if (window) {
        return SDL_CreateWindowFrom((void*)window);
    }
    return nullptr;
}

}
