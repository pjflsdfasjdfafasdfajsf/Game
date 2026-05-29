// NOTE: Windows platform layer implementation.
//
//     * W version of the functions are used everywhere as they use a little bit
//     less memory and are more explicit

#define UNICODE

#include <windows.h>

#define bool int
#define true 1
#define false 0

const int kDefaultWindowWidth = 1280;
const int kDefaultWindowHeight = 720;

void showError(const wchar_t *functionName) {
    if (!functionName) {
        return;
    }

    DWORD errorCode = GetLastError();
    wchar_t *errorString = 0;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   0, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&errorString, 0, 0);

    wchar_t messageBuffer[1024];
    wsprintfW(messageBuffer, L"%s failed with %lu:\n%s", functionName,
              errorCode, errorString ? errorString : L"Unknown Error");

    MessageBoxW(0, messageBuffer, L"Win32 Error", MB_ICONERROR | MB_OK);
}

LRESULT CALLBACK wndProc(HWND window, UINT message, WPARAM wParam,
                         LPARAM lParam) {
    switch (message) {
    case WM_DESTROY: {
        PostQuitMessage(0);

        return 0;
    } break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

HWND createWindow(const wchar_t *title) {
    HINSTANCE instance = GetModuleHandleW(0);
    if (!instance) {
        showError(L"GetModuleHandleW");

        return 0;
    }

    const wchar_t *className = L"Win32";

    HCURSOR cursor = LoadCursorW(0, IDC_ARROW);
    if (!cursor) {
        showError(L"LoadCursorW");

        return 0;
    }

    WNDCLASSW windowClass = {0};
    windowClass.lpfnWndProc = wndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = cursor;
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&windowClass)) {
        showError(L"RegisterClassW");

        return 0;
    }

    HWND window = CreateWindowExW(
        0, className, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        kDefaultWindowWidth, kDefaultWindowHeight, 0, 0, instance, 0);

    if (!window) {
        showError(L"CreateWindowExW");

        return 0;
    }

    return window;
}

void showWindow(HWND window) {
    ShowWindow(window, SW_SHOWDEFAULT);

    if (!UpdateWindow(window)) {
        showError(L"UpdateWindow");
    }
}

void update() {
    MSG message;
    bool hasMoreMessages = true;

    while (hasMoreMessages) {
        if (GetMessageW(&message, 0, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        } else {
            hasMoreMessages = false;
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    HWND mainWindow = createWindow(L"Win32");

    if (!mainWindow) {
        return -1;
    }
    
    showWindow(mainWindow);
    update();

    return 0;
}
