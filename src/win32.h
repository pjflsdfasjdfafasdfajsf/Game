#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

void ErrorShowLast(const wchar_t *functionName);

void ErrorShowHRESULT(HRESULT hresult, const wchar_t *functionName);
