// NOTE: Windows platform layer implementation.
//
//     * W version of the functions are used everywhere as they use a little bit
//     less memory and are more explicit
//
// TODO:
//    * Play some sound

#define UNICODE

#include "win32.h"
#include "win32_d3d12.h"

#include "game_platform.h"

#include <windows.h>

int _fltused = 0;

void ErrorShowLast(const wchar_t *functionName) {
    if (!functionName) {
        return;
    }

    DWORD errorCode = GetLastError();
    wchar_t *errorString = 0;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorString, 0, 0);

    wchar_t messageBuffer[1024];
    wsprintfW(messageBuffer, L"%s failed with %lu:\n%s", functionName, errorCode, errorString ? errorString : L"Unknown Error");

    MessageBoxW(0, messageBuffer, L"Win32 Error", MB_ICONERROR | MB_OK);

    if (errorString) {
        LocalFree(errorString);
    }
}

void ErrorShowHRESULT(HRESULT hresult, const wchar_t *functionName) {
    if (!hresult || !functionName) {
        return;
    }

    wchar_t *errorString = 0;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, hresult, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorString, 0, 0);

    wchar_t messageBuffer[1024];
    wsprintfW(messageBuffer, L"%s failed with HRESULT 0x%08X:\n%s", functionName, hresult, errorString ? errorString : L"Unknown Error");

    MessageBoxW(0, messageBuffer, L"DirectX Error", MB_ICONERROR | MB_OK);

    if (errorString) {
        LocalFree(errorString);
    }

    ExitProcess(1);
}
LRESULT CALLBACK WindowProcedure(HWND windowHandle, UINT message, WPARAM wordParameter, LPARAM longParameter) {
    switch (message) {
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    } break;
    }

    return DefWindowProcW(windowHandle, message, wordParameter, longParameter);
}

HWND WindowCreate(const wchar_t *title) {
    HINSTANCE instance = GetModuleHandleW(0);
    if (!instance) {
        ErrorShowLast(L"GetModuleHandleW");
        return 0;
    }

    const wchar_t *className = L"Win32";

    HCURSOR cursor = LoadCursorW(0, (LPCWSTR)IDC_ARROW);
    if (!cursor) {
        ErrorShowLast(L"LoadCursorW");
        return 0;
    }

    WNDCLASSW windowClass;
    MemoryZero(&windowClass, sizeof(windowClass));

    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = cursor;
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&windowClass)) {
        ErrorShowLast(L"RegisterClassW");
        return 0;
    }

    HWND windowHandle = CreateWindowExW(0, className, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0, 0, instance, 0);
    if (!windowHandle) {
        ErrorShowLast(L"CreateWindowExW");
        return 0;
    }

    return windowHandle;
}

void WindowShow(HWND windowHandle) {
    ShowWindow(windowHandle, SW_SHOWDEFAULT);

    if (!UpdateWindow(windowHandle)) {
        ErrorShowLast(L"UpdateWindow");
    }
}

void RunUpdateLoop(D3D12 *d3d12) {
    MSG message;
    MemoryZero(&message, sizeof(message));

    bool isRunning = true;

    while (isRunning) {
        if (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                isRunning = false;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        } else {
            D3D12DeviceRenderFrame(d3d12);
        }
    }
}

void WINAPI WinMainCRTStartup() {
    HWND mainWindowHandle = WindowCreate(L"Win32");

    if (!mainWindowHandle) {
        ExitProcess(1);
    }

    D3D12 d3d12;
    D3D12Initialize(&d3d12);

    D3D12DeviceInitialize(&d3d12);
    D3D12CommandsInitialize(&d3d12);
    D3D12SwapChainInitialize(&d3d12, mainWindowHandle);
    D3D12PipelineInitialize(&d3d12);
    D3D12SynchronizationInitialize(&d3d12);

    WindowShow(mainWindowHandle);

    RunUpdateLoop(&d3d12);

    D3D12DeviceWaitForGPU(&d3d12);

    ExitProcess(0);
}
