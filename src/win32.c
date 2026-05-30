// NOTE: Windows platform layer implementation.
//
// TODO:
//     * Define a proper cross-platform renderer interface
//     * Separate game and platform
//

#include <windows.h>

#include "win32.h"
#include "win32_d3d12.h"

#include "game_platform.h"
#include "game_types.h"
#include "game_ttf.h"
// #include "game_png.h"

MemoryStream *GlobalErrorStream;
MemoryStream *GlobalInfoStream;

int _fltused = 0;

void ErrorShowLast(const wchar_t *functionName) {
    if (!functionName) {
        return;
    }

    DWORD errorCode = GetLastError();
    wchar_t *errorString = 0;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorString, 0, 0);

    wchar_t messageBuffer[1024];
    wsprintfW(messageBuffer, L"%s (%lu):\n%s", functionName, errorCode, errorString ? errorString : L"Unknown Error");

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
    wsprintfW(messageBuffer, L"%s (HRESULT 0x%08X):\n%s", functionName, hresult, errorString ? errorString : L"Unknown Error");

    MessageBoxW(0, messageBuffer, L"Win32 Error", MB_ICONERROR | MB_OK);

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
    ZeroStruct(windowClass);

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

// NOTE: Audio implementation

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

DWORD WINAPI AudioThreadProcedure(void *threadParameter) {
    Win32Audio *audio = (Win32Audio *)threadParameter;

    HRESULT hresult = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CoInitializeEx on Audio Thread");
    }

    while (audio->isRunning) {
        if (!audio->isPaused) {
            AudioUpdate(audio);
        }

        Sleep(10);
    }

    CoUninitialize();
    return 0;
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

    audio->isRunning = true;
    audio->isPaused = false;
    audio->threadHandle = CreateThread(0, 0, AudioThreadProcedure, audio, 0, 0);

    if (!audio->threadHandle) {
        ErrorShowLast(L"CreateThread for Audio");
    }
}

void AudioPause(Win32Audio *audio) {
    if (!audio || !audio->audioClient) {
        return;
    }

    audio->isPaused = true;

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

    audio->isPaused = false;
}

void WindowShow(HWND windowHandle) {
    ShowWindow(windowHandle, SW_SHOWDEFAULT);

    if (!UpdateWindow(windowHandle)) {
        ErrorShowLast(L"UpdateWindow");
    }
}

void DrawTextTEMP(Win32Direct12 *d3d12, u32 atlasTextureId, const TrueTypeBakedGlyph *glyphs, u32 firstCharacter, u32 characterCount, const char *text, Vector2 startPosition, f32 scaleToScreen) {
    if (!glyphs || !text) {
        return;
    }

    Vector2 cursor = startPosition;

    for (u32 stringIndex = 0; text[stringIndex] != '\0'; stringIndex++) {
        u8 character = (u8)text[stringIndex];

        if (character == '\n') {
            cursor.x = startPosition.x;
            cursor.y -= (64.0f * scaleToScreen);

            continue;
        }

        if (character < firstCharacter || character >= (firstCharacter + characterCount)) {
            continue;
        }

        u32 glyphIndex = character - firstCharacter;
        const TrueTypeBakedGlyph *glyph = &glyphs[glyphIndex];

        if (glyph->isValid) {
            Vector2 position;
            position.x = cursor.x + (glyph->offset.x * scaleToScreen);
            position.y = cursor.y + (glyph->offset.y * scaleToScreen);

            Vector2 size;
            size.x = glyph->size.x * scaleToScreen;
            size.y = glyph->size.y * scaleToScreen;

            D3D12RectangleDrawEX(d3d12, atlasTextureId, position, size, glyph->uvMin, glyph->uvMax, V4(1.0f, 1.0f, 1.0f, 1.0f));

            cursor.x += (glyph->size.x + 2.0f) * scaleToScreen;
        } else if (character == ' ') {
            cursor.x += 16.0f * scaleToScreen;
        }
    }
}

static inline void GlobalStreamsDrain() {
    if (GlobalInfoStream && GlobalInfoStream->offset > 0) {
        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);

        if (stdOut && stdOut != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(stdOut, GlobalInfoStream->memory, (DWORD)GlobalInfoStream->offset, &written, 0);
        }

        GlobalInfoStream->offset = 0;
    }

    if (GlobalErrorStream && GlobalErrorStream->offset > 0) {
        HANDLE stdErr = GetStdHandle(STD_ERROR_HANDLE);

        if (stdErr && stdErr != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(stdErr, GlobalErrorStream->memory, (DWORD)GlobalErrorStream->offset, &written, 0);
        }

        GlobalErrorStream->offset = 0;
    }
}

u32 textureIdTEMP = 0;

void RunDraw(Win32Direct12 *d3d12, const TrueTypeBakedGlyph *glyphsTEMP) {
    D3D12FrameBegin(d3d12);
    {
        f32 scaleToScreenX = 1.0f / (DEFAULT_WINDOW_WIDTH / 2.0f);
        f32 scaleToScreenY = 1.0f / (DEFAULT_WINDOW_HEIGHT / 2.0f);

        f32 scale = scaleToScreenY;

        Vector2 startPosition = V2(-0.8f, 0.5f);

        DrawTextTEMP(d3d12, textureIdTEMP, glyphsTEMP, 32, 95, "eeee omg ! = - ````", startPosition, scale);
    }
    D3D12FrameEnd(d3d12);
}

void RunUpdate(Win32Direct12 *d3d12, Win32Audio *audio, const TrueTypeBakedGlyph *glyphsTEMP, HWND windowHandle) {
    MSG message;
    ZeroStruct(message);

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
            GlobalStreamsDrain();

            // NOTE: This logic below is to stop playing sound when the window is alt-tabbed because I am a good programmer.
            bool isFocused = (GetForegroundWindow() == windowHandle);

            if (!isFocused && wasFocused) {
                AudioPause(audio);
            } else if (isFocused && !wasFocused) {
                AudioResume(audio);
            }

            wasFocused = isFocused;

            if (isFocused) {
                RunDraw(d3d12, glyphsTEMP);
            } else {
                Sleep(10);
            }
        }
    }
}

void mainCRTStartup() {
    HWND window = WindowCreate(L"Win32");

    // NOTE: If you're changing something in Error* API and want to test it:
#if 0
    ErrorShowLast(L"ErrorShowLast");
    ErrorShowHRESULT(1, L"ErrorShowHRESULT");

    ExitProcess(1);
#endif

    if (!window) {
        ExitProcess(1);
    }

    usize permanentArenaSize = Megabytes(64);
    usize temporaryArenaSize = Megabytes(256);

    void *permanentMemoryBlock = VirtualAlloc(0, permanentArenaSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    void *temporaryMemoryBlock = VirtualAlloc(0, temporaryArenaSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    MemoryArena permanentArena;
    MemoryArenaInitialize(&permanentArena, permanentMemoryBlock, permanentArenaSize);

    MemoryArena temporaryArena;
    MemoryArenaInitialize(&temporaryArena, temporaryMemoryBlock, temporaryArenaSize);

    usize globalErrorStreamSize = Kilobytes(64);
    usize globalInfoStreamSize = Kilobytes(256);

    GlobalErrorStream = MemoryArenaPushArray(&permanentArena, MemoryStream, 1);
    GlobalInfoStream = MemoryArenaPushArray(&permanentArena, MemoryStream, 1);

    MemoryStreamInitializeWritable(GlobalErrorStream, MemoryArenaPushBytes(&permanentArena, globalErrorStreamSize), globalErrorStreamSize);
    MemoryStreamInitializeWritable(GlobalInfoStream, MemoryArenaPushBytes(&permanentArena, globalInfoStreamSize), globalInfoStreamSize);

    GlobalInfoStreamWrite("Win32 says hello!");

    static Win32Direct12 d3d12;
    D3D12Initialize(&d3d12, window);

    static Win32Audio audio;
    AudioInitialize(&audio);

    //     static const char watermelon[] = {
    // #include "watermelon.png.h"
    //     };

    //     Image image = ImageLoadFromPNG(watermelon, sizeof(watermelon));

    static const char arial[] = {
#include "arial.ttf.h"
    };

    TrueTypeFont font = TrueTypeFontLoadFromMemory(&temporaryArena, arial, sizeof(arial));
    TrueTypeBakedGlyph *glyphs = MemoryArenaPushArray(&permanentArena, TrueTypeBakedGlyph, TRUETYPE_CHARACTER_COUNT_FOR_ASCII);

    Image image = TrueTypeFontBakeAtlas(&permanentArena, &temporaryArena, &font, 64, 1024, 1024, TRUETYPE_FIRST_CHARACTER_FOR_ASCII, TRUETYPE_CHARACTER_COUNT_FOR_ASCII, glyphs);
    textureIdTEMP = D3D12TextureCreate(&d3d12, image.size.width, image.size.height, image.bytesPerPixel, image.pixels);

    WindowShow(window);

    RunUpdate(&d3d12, &audio, glyphs, window);

    ExitProcess(0);
}
