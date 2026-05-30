#pragma once

#include <X11/Xlib.h>

typedef struct {
    Display *display;
    Window window;
    Atom wmDeleteMessage;
} LinuxX11;
