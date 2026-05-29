// NOTE: Windows platform layer implementation.
//
//     * W version of the functions are used everywhere as they use a little bit
//     less memory and are more explicit
//
// TODO:
//     * Put sound on its own thread so it isn't stopped when the window is being resized or moved.

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

void AudioInitialize(Win32Audio *audio) {
    if (!audio) {
        return;
    }

    HRESULT hresult;

    MemoryZero(audio, sizeof(Win32Audio));

    // NOTE: These are missing WASAPI GUIDS that are not in the headers for some reason.
    GUID clsid_MMDeviceEnumerator;
    GUID iid_IMMDeviceEnumerator;
    GUID iid_IAudioClient;
    GUID iid_IAudioRenderClient;

    CLSIDFromString(L"{BCDE0395-E52F-467C-8E3D-C4579291692E}", &clsid_MMDeviceEnumerator);
    IIDFromString(L"{A95664D2-9614-4F35-A746-DE8DB63617E6}", &iid_IMMDeviceEnumerator);
    IIDFromString(L"{1CB9AD4C-DBFA-4C32-B178-C2F568A703B2}", &iid_IAudioClient);
    IIDFromString(L"{F294ACFC-3146-4483-A7BF-ADDCA7C260E2}", &iid_IAudioRenderClient);

    hresult = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CoInitializeEX");
    }

    hresult = CoCreateInstance(&clsid_MMDeviceEnumerator, 0, CLSCTX_ALL, &iid_IMMDeviceEnumerator, COM_OUT_POINTER(&audio->deviceEnumerator));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CoCreateInstance MMDeviceEnumerator");
    }

    hresult = IMMDeviceEnumerator_GetDefaultAudioEndpoint(audio->deviceEnumerator, eRender, eConsole, &audio->audioDevice);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"GetDefaultAudioEndpoint");
    }

    hresult = IMMDevice_Activate(audio->audioDevice, &iid_IAudioClient, CLSCTX_ALL, 0, COM_OUT_POINTER(&audio->audioClient));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"IMMDevice_Activate");
    }

    WAVEFORMATEX *waveFormat = 0;
    hresult = IAudioClient_GetMixFormat(audio->audioClient, &waveFormat);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"IAudioClient_GetMixFormat");
    }

    audio->channels = waveFormat->nChannels;
    audio->samplesPerSecond = waveFormat->nSamplesPerSec;

    audio->phaseIncrement = 440.0f / (f32)audio->samplesPerSecond;

    REFERENCE_TIME bufferDuration = 500000;
    hresult = IAudioClient_Initialize(audio->audioClient, AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, waveFormat, 0);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"IAudioClient_Initialize");
    }

    CoTaskMemFree(waveFormat);

    hresult = IAudioClient_GetBufferSize(audio->audioClient, &audio->bufferFrameCount);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"IAudioClient_GetBufferSize");
    }

    hresult = IAudioClient_GetService(audio->audioClient, &iid_IAudioRenderClient, COM_OUT_POINTER(&audio->renderClient));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"IAudioClient_GetService");
    }

    hresult = IAudioClient_Start(audio->audioClient);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"IAudioClient_Start");
    }
}
void AudioUpdate(Win32Audio *audio) {
    if (!audio) {
        return;
    }

    HRESULT hresult;

    u32 paddingFrameCount;
    hresult = IAudioClient_GetCurrentPadding(audio->audioClient, &paddingFrameCount);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"GetCurrentPadding");
    }

    u32 availableFrameCount = audio->bufferFrameCount - paddingFrameCount;

    if (availableFrameCount == 0) {
        return;
    }

    u8 *audioData;
    hresult = IAudioRenderClient_GetBuffer(audio->renderClient, availableFrameCount, &audioData);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"GetBuffer");
    }

    f32 *samplePointer = (f32 *)audioData;

    for (u32 frameIndex = 0; frameIndex < availableFrameCount; frameIndex++) {
        f32 sampleValue = (audio->phase < 0.5f) ? 0.05f : -0.05f;

        audio->phase += audio->phaseIncrement;
        if (audio->phase > 1.0f) {
            audio->phase -= 1.0f;
        }

        for (u32 channelIndex = 0; channelIndex < audio->channels; channelIndex++) {
            *samplePointer++ = sampleValue;
        }
    }

    hresult = IAudioRenderClient_ReleaseBuffer(audio->renderClient, availableFrameCount, 0);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"ReleaseBuffer");
    }
}

void AudioPause(Win32Audio *audio) {
    if (!audio || !audio->audioClient) {
        return;
    }

    HRESULT hresult = IAudioClient_Stop(audio->audioClient);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"IAudioClient_Stop");
    }
}

void AudioResume(Win32Audio *audio) {
    if (!audio || !audio->audioClient) {
        return;
    }

    HRESULT hresult = IAudioClient_Start(audio->audioClient);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"IAudioClient_Start");
    }
}

void WindowShow(HWND windowHandle) {
    ShowWindow(windowHandle, SW_SHOWDEFAULT);

    if (!UpdateWindow(windowHandle)) {
        ErrorShowLast(L"UpdateWindow");
    }
}

void RunUpdateLoop(Win32Direct12 *d3d12, Win32Audio *audio, HWND windowHandle) {
    MSG message;
    MemoryZero(&message, sizeof(message));

    bool isRunning = true;
    bool wasFocused = true;

    while (isRunning) {
        if (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                isRunning = false;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        } else {
            // NOTE: This logic below is to stop playing sound when the window is alt-tabbed because I am a good programmer.
            bool isFocused = (GetForegroundWindow() == windowHandle);

            if (!isFocused && wasFocused) {
                AudioPause(audio);
            } else if (isFocused && !wasFocused) {
                AudioResume(audio);
            }

            wasFocused = isFocused;

            if (isFocused) {
                D3D12DeviceRenderFrame(d3d12);
                AudioUpdate(audio);
            } else {
                Sleep(10);
            }
        }
    }
}
void WINAPI WinMainCRTStartup() {
    HWND mainWindowHandle = WindowCreate(L"Win32");

    if (!mainWindowHandle) {
        ExitProcess(1);
    }

    Win32Direct12 d3d12;
    D3D12Initialize(&d3d12);

    Win32Audio audio;
    AudioInitialize(&audio);

    D3D12DeviceInitialize(&d3d12);
    D3D12CommandsInitialize(&d3d12);
    D3D12SwapChainInitialize(&d3d12, mainWindowHandle);
    D3D12PipelineInitialize(&d3d12);
    D3D12SynchronizationInitialize(&d3d12);

    WindowShow(mainWindowHandle);

    RunUpdateLoop(&d3d12, &audio, mainWindowHandle);

    D3D12DeviceWaitForGPU(&d3d12);

    ExitProcess(0);
}
