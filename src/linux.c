// TODO:
//     * Simple Vulkan renderer for a triangle
//     * Audio
//     * Image rendering
//     * Font rendering
//

#include "linux.h"
#include "game_platform.h"

// TODO: Can we get rid of it?
#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdio.h>

LinuxX11 WindowCreate(const char *title) {
    LinuxX11 x11;
    MemoryZero(&x11, sizeof(x11));

    Display *display = XOpenDisplay(0);
    if (!display) {
        fprintf(stderr, "Could not open X11 display.\n");

        return x11;
    }

    int defaultScreen = DefaultScreen(display);
    Window rootWindow = RootWindow(display, defaultScreen);

    unsigned long blackPixel = BlackPixel(display, defaultScreen);
    unsigned long whitePixel = WhitePixel(display, defaultScreen);

    Window window = XCreateSimpleWindow(display, rootWindow, 0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 1, blackPixel, whitePixel);
    if (!window) {
        fprintf(stderr, "Could not create X11 window.\n");

        return x11;
    }

    XStoreName(display, window, title);

    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", false);
    if (!XSetWMProtocols(display, window, &wmDeleteMessage, 1)) {
        fprintf(stderr, "Could not set WM_DELETE_WINDOW protocol.\n");

        return x11;
    }

    long eventMask = ExposureMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | StructureNotifyMask;
    XSelectInput(display, window, eventMask);

    x11.display = display;
    x11.window = window;
    x11.wmDeleteMessage = wmDeleteMessage;

    return x11;
}

void WindowShow(LinuxX11 *x11) {
    if (!x11 || x11->display) {
        return;
    }

    XMapWindow(x11->display, x11->window);
    XFlush(x11->display);
}

void RunDraw() {
    // TODO
}

void RunUpdate(LinuxX11 *x11) {
    if (!x11 || !x11->display) {
        return;
    }

    bool isRunning = true;

    while (isRunning) {
        while (XPending(x11->display) > 0) {
            XEvent event;
            XNextEvent(x11->display, &event);

            switch (event.type) {
            case ClientMessage: {
                if ((Atom)event.xclient.data.l[0] == x11->wmDeleteMessage) {
                    isRunning = false;
                }
            } break;
            }
        }
    }
}

int main() {
    LinuxX11 x11 = WindowCreate("Linux");

    if (!x11.display) {
        return 1;
    }

    WindowShow(&x11);

    RunUpdate(&x11);
}
