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

    ID3D12Resource *vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
} Win32Direct12;

void D3D12Initialize(Win32Direct12 *d3d12);

void D3D12DeviceInitialize(Win32Direct12 *d3d12);

void D3D12CommandsInitialize(Win32Direct12 *d3d12);

void D3D12SwapChainInitialize(Win32Direct12 *d3d12, HWND windowHandle);

void D3D12PipelineInitialize(Win32Direct12 *d3d12);

void D3D12SynchronizationInitialize(Win32Direct12 *d3d12);

void D3D12DeviceWaitForGPU(Win32Direct12 *d3d12);

void D3D12DeviceRenderFrame(Win32Direct12 *d3d12);

void D3D12VertexBufferInitialize(Win32Direct12 *d3d12);
