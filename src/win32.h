#pragma once

#include "game_platform.h"

#define COBJMACROS
#define UNICODE

#include <mmdeviceapi.h>
#include <audioclient.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define COM_OUT_POINTER(pointer) ((void **)(pointer))

typedef struct {
  IMMDeviceEnumerator *deviceEnumerator;
  IMMDevice *audioDevice;
  IAudioClient *audioClient;
  IAudioRenderClient *renderClient;

  u32 bufferFrameCount;
  u32 channels;
  u32 samplesPerSecond;

  f32 phase;
  f32 phaseIncrement;  

  HANDLE threadHandle;
  volatile bool isRunning;
  volatile bool isPaused;
} Win32Audio;

void ErrorShowLast(const wchar_t *functionName);

void ErrorShowHRESULT(HRESULT hresult, const wchar_t *functionName);
