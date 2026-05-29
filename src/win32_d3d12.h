#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>

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

void D3D12Initialize(D3D12 *d3d12);

void D3D12DeviceInitialize(D3D12 *d3d12);

void D3D12CommandsInitialize(D3D12 *d3d12);

void D3D12SwapChainInitialize(D3D12 *d3d12, HWND windowHandle);

void D3D12PipelineInitialize(D3D12 *d3d12);

void D3D12SynchronizationInitialize(D3D12 *d3d12);

void D3D12DeviceWaitForGPU(D3D12 *d3d12);

void D3D12DeviceRenderFrame(D3D12 *d3d12);
