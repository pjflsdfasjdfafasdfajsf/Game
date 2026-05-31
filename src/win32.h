#pragma once

#define COBJMACROS
#define UNICODE
#define WIDL_C_INLINE_WRAPPERS

#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "game_types.h"

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
