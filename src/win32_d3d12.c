#include "win32.h"
#include "win32_d3d12.h"

#include "game_platform.h"

// NOTE: Compiled shaders.
#include "BasicGeometryVS.h"
#include "BasicGeometryPS.h"

void D3D12Initialize(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    MemoryZero(d3d12, sizeof(Win32Direct12));
}

void D3D12DeviceInitialize(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    hresult = CreateDXGIFactory1(&IID_IDXGIFactory4, COM_OUT_POINTER(&d3d12->factory));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateDXGIFactory1");
    }

    hresult = D3D12CreateDevice(0, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, COM_OUT_POINTER(&d3d12->device));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"D3D12CreateDevice");
    }
}

void D3D12CommandsInitialize(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    D3D12_COMMAND_QUEUE_DESC commandQueueDescription;
    MemoryZero(&commandQueueDescription, sizeof(commandQueueDescription));

    commandQueueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hresult = ID3D12Device_CreateCommandQueue(d3d12->device, &commandQueueDescription, &IID_ID3D12CommandQueue, COM_OUT_POINTER(&d3d12->commandQueue));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateCommandQueue");
    }

    hresult = ID3D12Device_CreateCommandAllocator(d3d12->device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, COM_OUT_POINTER(&d3d12->commandAllocator));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateCommandAllocator");
    }

    hresult = ID3D12Device_CreateCommandList(d3d12->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12->commandAllocator, 0, &IID_ID3D12GraphicsCommandList, COM_OUT_POINTER(&d3d12->commandList));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateCommandList");
    }

    hresult = ID3D12GraphicsCommandList_Close(d3d12->commandList);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandList Close Initial");
    }
}

void D3D12SwapChainInitialize(Win32Direct12 *d3d12, HWND windowHandle) {
    if (!d3d12 || !windowHandle) {
        return;
    }

    HRESULT hresult;
    UINT index;

    DXGI_SWAP_CHAIN_DESC1 swapChainDescription;
    MemoryZero(&swapChainDescription, sizeof(swapChainDescription));

    swapChainDescription.Width = DEFAULT_WINDOW_WIDTH;
    swapChainDescription.Height = DEFAULT_WINDOW_HEIGHT;
    swapChainDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDescription.SampleDesc.Count = 1;
    swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDescription.BufferCount = FRAME_COUNT;
    swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain1 *temporarySwapChain = 0;
    hresult = IDXGIFactory4_CreateSwapChainForHwnd(d3d12->factory, (IUnknown *)d3d12->commandQueue, windowHandle, &swapChainDescription, 0, 0, &temporarySwapChain);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateSwapChainForHwnd");
    }

    hresult = IDXGISwapChain1_QueryInterface(temporarySwapChain, &IID_IDXGISwapChain3, COM_OUT_POINTER(&d3d12->swapChain));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"QueryInterface SwapChain3");
    }
    IDXGISwapChain1_Release(temporarySwapChain);

    d3d12->frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(d3d12->swapChain);

    D3D12_DESCRIPTOR_HEAP_DESC renderTargetViewHeapDescription;
    MemoryZero(&renderTargetViewHeapDescription, sizeof(renderTargetViewHeapDescription));

    renderTargetViewHeapDescription.NumDescriptors = FRAME_COUNT;
    renderTargetViewHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    hresult = ID3D12Device_CreateDescriptorHeap(d3d12->device, &renderTargetViewHeapDescription, &IID_ID3D12DescriptorHeap, COM_OUT_POINTER(&d3d12->renderTargetViewHeap));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateDescriptorHeap");
    }

    d3d12->renderTargetViewDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(d3d12->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(d3d12->renderTargetViewHeap, &renderTargetViewHandle);

    for (index = 0; index < (UINT)FRAME_COUNT; index++) {
        hresult = IDXGISwapChain3_GetBuffer(d3d12->swapChain, index, &IID_ID3D12Resource, COM_OUT_POINTER(&d3d12->renderTargets[index]));
        if (FAILED(hresult)) {
            ErrorShowHRESULT(hresult, L"SwapChain GetBuffer");
        }

        ID3D12Device_CreateRenderTargetView(d3d12->device, d3d12->renderTargets[index], 0, renderTargetViewHandle);
        renderTargetViewHandle.ptr += d3d12->renderTargetViewDescriptorSize;
    }
}

void D3D12PipelineInitialize(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    D3D12_ROOT_PARAMETER rootParameter;
    MemoryZero(&rootParameter, sizeof(rootParameter));

    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameter.Constants.ShaderRegister = 0;
    rootParameter.Constants.RegisterSpace = 0;
    rootParameter.Constants.Num32BitValues = 1;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSamplerDescription;
    MemoryZero(&staticSamplerDescription, sizeof(staticSamplerDescription));

    staticSamplerDescription.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplerDescription.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplerDescription.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplerDescription.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplerDescription.ShaderRegister = 0;
    staticSamplerDescription.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDescription;
    MemoryZero(&rootSignatureDescription, sizeof(rootSignatureDescription));

    rootSignatureDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
    rootSignatureDescription.NumParameters = 1;
    rootSignatureDescription.pParameters = &rootParameter;
    rootSignatureDescription.NumStaticSamplers = 1;
    rootSignatureDescription.pStaticSamplers = &staticSamplerDescription;

    ID3DBlob *signatureBlob = 0;
    ID3DBlob *errorBlob = 0;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDescription;
    MemoryZero(&versionedDescription, sizeof(versionedDescription));
    versionedDescription.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    versionedDescription.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
    versionedDescription.Desc_1_1.NumParameters = 1;
    versionedDescription.Desc_1_1.pParameters = (const D3D12_ROOT_PARAMETER1 *)&rootParameter;
    versionedDescription.Desc_1_1.NumStaticSamplers = 1;
    versionedDescription.Desc_1_1.pStaticSamplers = &staticSamplerDescription;

    hresult = D3D12SerializeVersionedRootSignature(&versionedDescription, &signatureBlob, &errorBlob);
    if (FAILED(hresult)) {
        if (errorBlob) {
            OutputDebugStringA((char *)errorBlob->lpVtbl->GetBufferPointer(errorBlob));
        }

        ErrorShowHRESULT(hresult, L"SerializeVersionedRootSignature");
    }

    hresult = ID3D12Device_CreateRootSignature(d3d12->device, 0, signatureBlob->lpVtbl->GetBufferPointer(signatureBlob), signatureBlob->lpVtbl->GetBufferSize(signatureBlob), &IID_ID3D12RootSignature, COM_OUT_POINTER(&d3d12->rootSignature));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateRootSignature");
    }

    signatureBlob->lpVtbl->Release(signatureBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDescription;
    MemoryZero(&pipelineStateDescription, sizeof(pipelineStateDescription));

    pipelineStateDescription.pRootSignature = d3d12->rootSignature;

    pipelineStateDescription.VS.pShaderBytecode = g_VSMain;
    pipelineStateDescription.VS.BytecodeLength = sizeof(g_VSMain);

    pipelineStateDescription.PS.pShaderBytecode = g_PSMain;
    pipelineStateDescription.PS.BytecodeLength = sizeof(g_PSMain);

    pipelineStateDescription.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipelineStateDescription.SampleMask = UINT_MAX;
    pipelineStateDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineStateDescription.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pipelineStateDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDescription.NumRenderTargets = 1;
    pipelineStateDescription.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineStateDescription.SampleDesc.Count = 1;

    D3D12_INPUT_ELEMENT_DESC inputElementDescriptions[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    pipelineStateDescription.InputLayout.pInputElementDescs = inputElementDescriptions;
    pipelineStateDescription.InputLayout.NumElements = sizeof(inputElementDescriptions) / sizeof(D3D12_INPUT_ELEMENT_DESC);

    hresult = ID3D12Device_CreateGraphicsPipelineState(d3d12->device, &pipelineStateDescription, &IID_ID3D12PipelineState, COM_OUT_POINTER(&d3d12->pipelineState));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateGraphicsPipelineState");
    }
}

void D3D12SynchronizationInitialize(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    hresult = ID3D12Device_CreateFence(d3d12->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, COM_OUT_POINTER(&d3d12->fence));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateFence");
    }

    d3d12->fenceEvent = CreateEventW(0, FALSE, FALSE, 0);
    if (!d3d12->fenceEvent) {
        ErrorShowLast(L"CreateEventW");
    }
}

void D3D12DeviceWaitForGPU(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;
    const UINT64 fenceToWaitFor = d3d12->fenceValue;

    hresult = ID3D12CommandQueue_Signal(d3d12->commandQueue, d3d12->fence, fenceToWaitFor);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandQueue Signal");
    }

    d3d12->fenceValue++;

    if (ID3D12Fence_GetCompletedValue(d3d12->fence) < fenceToWaitFor) {
        hresult = ID3D12Fence_SetEventOnCompletion(d3d12->fence, fenceToWaitFor, d3d12->fenceEvent);

        if (FAILED(hresult)) {
            ErrorShowHRESULT(hresult, L"SetEventOnCompletion");
        }

        WaitForSingleObject(d3d12->fenceEvent, INFINITE);
    }

    d3d12->frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(d3d12->swapChain);
}

void D3D12DeviceRenderFrame(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    hresult = ID3D12CommandAllocator_Reset(d3d12->commandAllocator);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandAllocator Reset");
    }

    hresult = ID3D12GraphicsCommandList_Reset(d3d12->commandList, d3d12->commandAllocator, d3d12->pipelineState);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandList Reset");
    }

    ID3D12GraphicsCommandList_SetGraphicsRootSignature(d3d12->commandList, d3d12->rootSignature);

    ID3D12DescriptorHeap *descriptorHeaps[] = {d3d12->descriptorHeap};
    ID3D12GraphicsCommandList_SetDescriptorHeaps(d3d12->commandList, 1, descriptorHeaps);

    u32 textureIndex = 0;
    ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(d3d12->commandList, 0, 1, &textureIndex, 0);

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, (f32)DEFAULT_WINDOW_WIDTH, (f32)DEFAULT_WINDOW_HEIGHT, 0.0f, 1.0f};
    ID3D12GraphicsCommandList_RSSetViewports(d3d12->commandList, 1, &viewport);

    D3D12_RECT scissorRectangle = {0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT};
    ID3D12GraphicsCommandList_RSSetScissorRects(d3d12->commandList, 1, &scissorRectangle);

    D3D12_RESOURCE_BARRIER barrier;
    MemoryZero(&barrier, sizeof(barrier));

    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = d3d12->renderTargets[d3d12->frameIndex];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    ID3D12GraphicsCommandList_ResourceBarrier(d3d12->commandList, 1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(d3d12->renderTargetViewHeap, &renderTargetViewHandle);
    renderTargetViewHandle.ptr += d3d12->frameIndex * d3d12->renderTargetViewDescriptorSize;

    ID3D12GraphicsCommandList_OMSetRenderTargets(d3d12->commandList, 1, &renderTargetViewHandle, FALSE, 0);

    const f32 clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D12GraphicsCommandList_ClearRenderTargetView(d3d12->commandList, renderTargetViewHandle, clearColor, 0, 0);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(d3d12->commandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D12GraphicsCommandList_IASetVertexBuffers(d3d12->commandList, 0, 1, &d3d12->vertexBufferView);

    ID3D12GraphicsCommandList_DrawInstanced(d3d12->commandList, 3, 1, 0, 0);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    ID3D12GraphicsCommandList_ResourceBarrier(d3d12->commandList, 1, &barrier);

    hresult = ID3D12GraphicsCommandList_Close(d3d12->commandList);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandList Close");
    }

    ID3D12CommandList *commandLists[] = {(ID3D12CommandList *)d3d12->commandList};
    ID3D12CommandQueue_ExecuteCommandLists(d3d12->commandQueue, 1, commandLists);

    hresult = IDXGISwapChain3_Present(d3d12->swapChain, 1, 0);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"SwapChain Present");
    }

    D3D12DeviceWaitForGPU(d3d12);
}

void D3D12VertexBufferInitialize(Win32Direct12 *d3d12) {
    if (!d3d12 || !d3d12->device) {
        return;
    }

    HRESULT hresult;

    const Vertex triangleVertices[] = {
        {{0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};
    const UINT vertexBufferSize = sizeof(triangleVertices);

    D3D12_HEAP_PROPERTIES heapProperties;
    MemoryZero(&heapProperties, sizeof(heapProperties));

    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDescription;
    MemoryZero(&resourceDescription, sizeof(resourceDescription));

    resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDescription.Alignment = 0;
    resourceDescription.Width = vertexBufferSize;
    resourceDescription.Height = 1;
    resourceDescription.DepthOrArraySize = 1;
    resourceDescription.MipLevels = 1;
    resourceDescription.Format = DXGI_FORMAT_UNKNOWN;
    resourceDescription.SampleDesc.Count = 1;
    resourceDescription.SampleDesc.Quality = 0;
    resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

    hresult = ID3D12Device_CreateCommittedResource(d3d12->device, &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, 0, &IID_ID3D12Resource, COM_OUT_POINTER(&d3d12->vertexBuffer));

    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateCommittedResource (Vertex Buffer)");
    }

    u8 *vertexDataBegin = 0;
    D3D12_RANGE readRange;
    MemoryZero(&readRange, sizeof(readRange));

    hresult = ID3D12Resource_Map(d3d12->vertexBuffer, 0, &readRange, (void **)&vertexDataBegin);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"Map (Vertex Buffer)");
    }

    MemoryCopyForwards(vertexDataBegin, triangleVertices, vertexBufferSize);
    ID3D12Resource_Unmap(d3d12->vertexBuffer, 0, 0);

    d3d12->vertexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(d3d12->vertexBuffer);
    d3d12->vertexBufferView.StrideInBytes = sizeof(Vertex);
    d3d12->vertexBufferView.SizeInBytes = vertexBufferSize;
}

void D3D12HeapInitialize(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    D3D12_DESCRIPTOR_HEAP_DESC heapDescription;
    MemoryZero(&heapDescription, sizeof(heapDescription));
    heapDescription.NumDescriptors = 4096;
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hresult = ID3D12Device_CreateDescriptorHeap(d3d12->device, &heapDescription, &IID_ID3D12DescriptorHeap, COM_OUT_POINTER(&d3d12->descriptorHeap));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"ID3D12Device_CreateDescriptorHeap");
    }
}

void D3D12InitializeTextureTEMP(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    const u32 textureWidth = 256;
    const u32 textureHeight = 256;
    const u32 texturePixelSize = 4;

    D3D12_RESOURCE_DESC textureDescription;
    MemoryZero(&textureDescription, sizeof(textureDescription));

    textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDescription.Width = textureWidth;
    textureDescription.Height = textureHeight;
    textureDescription.DepthOrArraySize = 1;
    textureDescription.MipLevels = 1;
    textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeapProperties;
    MemoryZero(&defaultHeapProperties, sizeof(defaultHeapProperties));

    defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    hresult = ID3D12Device_CreateCommittedResource(d3d12->device, &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDescription, D3D12_RESOURCE_STATE_COPY_DEST, 0, &IID_ID3D12Resource, COM_OUT_POINTER(&d3d12->someRandomTextureIdk));

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT64 totalUploadBufferSize = 0;
    ID3D12Device_GetCopyableFootprints(d3d12->device, &textureDescription, 0, 1, 0, &footprint, 0, 0, &totalUploadBufferSize);

    D3D12_HEAP_PROPERTIES uploadHeapProperties;
    MemoryZero(&uploadHeapProperties, sizeof(uploadHeapProperties));

    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDescription;
    MemoryZero(&uploadDescription, sizeof(uploadDescription));

    uploadDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDescription.Width = totalUploadBufferSize;
    uploadDescription.Height = 1;
    uploadDescription.DepthOrArraySize = 1;
    uploadDescription.MipLevels = 1;
    uploadDescription.SampleDesc.Count = 1;
    uploadDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hresult = ID3D12Device_CreateCommittedResource(d3d12->device, &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadDescription, D3D12_RESOURCE_STATE_GENERIC_READ, 0, &IID_ID3D12Resource, COM_OUT_POINTER(&d3d12->textureUploadHeap));

    u8 *mappedData = 0;
    ID3D12Resource_Map(d3d12->textureUploadHeap, 0, 0, (void **)&mappedData);

    for (u32 y = 0; y < textureHeight; ++y) {
        u32 *row = (u32 *)(mappedData + footprint.Offset + (y * footprint.Footprint.RowPitch));

        for (u32 x = 0; x < textureWidth; ++x) {
            bool isWhite = ((x / 32) % 2) == ((y / 32) % 2);
            row[x] = isWhite ? 0xFFFFFFFF : 0xFF000000;
        }
    }
    ID3D12Resource_Unmap(d3d12->textureUploadHeap, 0, 0);

    ID3D12CommandAllocator_Reset(d3d12->commandAllocator);
    ID3D12GraphicsCommandList_Reset(d3d12->commandList, d3d12->commandAllocator, 0);

    D3D12_TEXTURE_COPY_LOCATION sourceLocation;
    MemoryZero(&sourceLocation, sizeof(sourceLocation));

    sourceLocation.pResource = d3d12->textureUploadHeap;
    sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    sourceLocation.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION destinationLocation;
    MemoryZero(&destinationLocation, sizeof(destinationLocation));

    destinationLocation.pResource = d3d12->someRandomTextureIdk;
    destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destinationLocation.SubresourceIndex = 0;

    ID3D12GraphicsCommandList_CopyTextureRegion(d3d12->commandList, &destinationLocation, 0, 0, 0, &sourceLocation, 0);

    D3D12_RESOURCE_BARRIER resourceBarrier;
    MemoryZero(&resourceBarrier, sizeof(resourceBarrier));

    resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    resourceBarrier.Transition.pResource = d3d12->someRandomTextureIdk;
    resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    ID3D12GraphicsCommandList_ResourceBarrier(d3d12->commandList, 1, &resourceBarrier);

    ID3D12GraphicsCommandList_Close(d3d12->commandList);
    ID3D12CommandList *commandLists[] = {(ID3D12CommandList *)d3d12->commandList};
    ID3D12CommandQueue_ExecuteCommandLists(d3d12->commandQueue, 1, commandLists);

    D3D12DeviceWaitForGPU(d3d12);

    D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription;
    MemoryZero(&shaderResourceViewDescription, sizeof(shaderResourceViewDescription));

    shaderResourceViewDescription.Format = textureDescription.Format;
    shaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    shaderResourceViewDescription.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE heapHandle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(d3d12->descriptorHeap, &heapHandle);

    ID3D12Device_CreateShaderResourceView(d3d12->device, d3d12->someRandomTextureIdk, &shaderResourceViewDescription, heapHandle);
}
