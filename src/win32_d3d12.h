#pragma once

#include <dxgi1_4.h>
#include <d3d12.h>

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
} D3D12;
