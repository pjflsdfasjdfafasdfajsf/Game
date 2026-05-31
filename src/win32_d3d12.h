#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>

#include "game_platform.h"
#include "game_types.h"

#define FRAME_COUNT 3

typedef struct {
    IDXGIFactory4 *factory;
    ID3D12Device *device;
    ID3D12CommandQueue *commandQueue;
    ID3D12CommandAllocator *commandAllocator;
    ID3D12GraphicsCommandList *commandList;

    IDXGISwapChain3 *swapChain;
    ID3D12DescriptorHeap *renderTargetViewHeap;
    ID3D12Resource *renderTargets[FRAME_COUNT];
    UINT renderTargetViewDescriptorSize;
    UINT frameIndex;

    ID3D12RootSignature *rootSignature;
    ID3D12PipelineState *pipelineState;

    ID3D12Fence *fence;
    UINT64 fenceValue;
    HANDLE fenceEvent;

    ID3D12Resource *vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    void *vertexData;
    UINT vertexCapacity;
    UINT vertexCount;

    ID3D12Resource *textures[4096];
    UINT loadedTextureCount;

    ID3D12DescriptorHeap *descriptorHeap;
} Win32Direct12;

void D3D12Initialize(Win32Direct12 *d3d12, HWND window);

void D3D12FrameBegin(Win32Direct12 *d3d12, RenderCommandBuffer *commandBuffer);
void D3D12FrameEnd(Win32Direct12 *d3d12, const RenderCommandBuffer *commandBuffer);
