#include "win32.h"
#include "win32_d3d12.h"

#include "game_platform.h"

// NOTE: Compiled shaders.
#include "BasicGeometryVS.h"
#include "BasicGeometryPS.h"

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

    pipelineStateDescription.BlendState.RenderTarget[0].BlendEnable = TRUE;
    pipelineStateDescription.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    pipelineStateDescription.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    pipelineStateDescription.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    pipelineStateDescription.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pipelineStateDescription.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    pipelineStateDescription.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    pipelineStateDescription.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
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

    d3d12->fenceValue = 1;

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

void D3D12VertexBufferInitialize(Win32Direct12 *d3d12, UINT maximumVertexCapacity) {
    if (!d3d12 || !d3d12->device) {
        return;
    }

    HRESULT hresult;
    const UINT vertexBufferSizeInBytes = maximumVertexCapacity * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES uploadHeapProperties;
    MemoryZero(&uploadHeapProperties, sizeof(uploadHeapProperties));
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferResourceDescription;
    MemoryZero(&bufferResourceDescription, sizeof(bufferResourceDescription));
    bufferResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferResourceDescription.Alignment = 0;
    bufferResourceDescription.Width = vertexBufferSizeInBytes;
    bufferResourceDescription.Height = 1;
    bufferResourceDescription.DepthOrArraySize = 1;
    bufferResourceDescription.MipLevels = 1;
    bufferResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
    bufferResourceDescription.SampleDesc.Count = 1;
    bufferResourceDescription.SampleDesc.Quality = 0;
    bufferResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

    hresult = ID3D12Device_CreateCommittedResource(d3d12->device, &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferResourceDescription, D3D12_RESOURCE_STATE_GENERIC_READ, 0, &IID_ID3D12Resource, COM_OUT_POINTER(&d3d12->vertexBuffer));

    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateCommittedResource");
        return;
    }

    D3D12_RANGE readRange;
    MemoryZero(&readRange, sizeof(readRange));

    hresult = ID3D12Resource_Map(d3d12->vertexBuffer, 0, &readRange, &d3d12->vertexData);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"Map");
        return;
    }

    d3d12->vertexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(d3d12->vertexBuffer);
    d3d12->vertexBufferView.StrideInBytes = sizeof(Vertex);
    d3d12->vertexBufferView.SizeInBytes = vertexBufferSizeInBytes;

    d3d12->vertexCapacity = maximumVertexCapacity;
    d3d12->vertexCount = 0;
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

u32 D3D12TextureCreate(Win32Direct12 *d3d12, u32 width, u32 height, u32 bytesPerPixel, const void *pixels) {
    if (!d3d12 || !pixels) {
        return 0;
    }

    if (d3d12->loadedTextureCount >= 4096) {
        return 0;
    }

    HRESULT hresult;
    u32 textureId = d3d12->loadedTextureCount;

    D3D12_RESOURCE_DESC textureDescription;
    MemoryZero(&textureDescription, sizeof(textureDescription));
    textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.DepthOrArraySize = 1;
    textureDescription.MipLevels = 1;
    textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeapProperties;
    MemoryZero(&defaultHeapProperties, sizeof(defaultHeapProperties));
    defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    hresult = ID3D12Device_CreateCommittedResource(d3d12->device, &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDescription, D3D12_RESOURCE_STATE_COPY_DEST, 0, &IID_ID3D12Resource, COM_OUT_POINTER(&d3d12->textures[textureId]));

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

    ID3D12Resource *uploadHeap = 0;
    hresult = ID3D12Device_CreateCommittedResource(d3d12->device, &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadDescription, D3D12_RESOURCE_STATE_GENERIC_READ, 0, &IID_ID3D12Resource, COM_OUT_POINTER(&uploadHeap));

    u8 *mappedData = 0;
    ID3D12Resource_Map(uploadHeap, 0, 0, (void **)&mappedData);

    const u8 *sourcePixels = (const u8 *)pixels;
    for (u32 y = 0; y < height; ++y) {
        u8 *destinationRow = mappedData + footprint.Offset + (y * footprint.Footprint.RowPitch);
        const u8 *sourceRow = sourcePixels + (y * width * bytesPerPixel);

        if (bytesPerPixel == 4) {
            MemoryCopyForwards(destinationRow, sourceRow, width * 4);
        } else if (bytesPerPixel == 3) {
            for (u32 x = 0; x < width; ++x) {
                destinationRow[x * 4 + 0] = sourceRow[x * 3 + 0];
                destinationRow[x * 4 + 1] = sourceRow[x * 3 + 1];
                destinationRow[x * 4 + 2] = sourceRow[x * 3 + 2];
                destinationRow[x * 4 + 3] = 255;
            }
        }
    }
    ID3D12Resource_Unmap(uploadHeap, 0, 0);

    ID3D12CommandAllocator_Reset(d3d12->commandAllocator);
    ID3D12GraphicsCommandList_Reset(d3d12->commandList, d3d12->commandAllocator, 0);

    D3D12_TEXTURE_COPY_LOCATION sourceLocation;
    MemoryZero(&sourceLocation, sizeof(sourceLocation));

    sourceLocation.pResource = uploadHeap;
    sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    sourceLocation.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION destinationLocation;
    MemoryZero(&destinationLocation, sizeof(destinationLocation));

    destinationLocation.pResource = d3d12->textures[textureId];
    destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destinationLocation.SubresourceIndex = 0;

    ID3D12GraphicsCommandList_CopyTextureRegion(d3d12->commandList, &destinationLocation, 0, 0, 0, &sourceLocation, 0);

    D3D12_RESOURCE_BARRIER resourceBarrier;
    MemoryZero(&resourceBarrier, sizeof(resourceBarrier));

    resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    resourceBarrier.Transition.pResource = d3d12->textures[textureId];
    resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    ID3D12GraphicsCommandList_ResourceBarrier(d3d12->commandList, 1, &resourceBarrier);

    ID3D12GraphicsCommandList_Close(d3d12->commandList);
    ID3D12CommandList *commandLists[] = {(ID3D12CommandList *)d3d12->commandList};
    ID3D12CommandQueue_ExecuteCommandLists(d3d12->commandQueue, 1, commandLists);

    D3D12DeviceWaitForGPU(d3d12);
    ID3D12Resource_Release(uploadHeap);

    D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription;
    MemoryZero(&shaderResourceViewDescription, sizeof(shaderResourceViewDescription));
    shaderResourceViewDescription.Format = textureDescription.Format;
    shaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    shaderResourceViewDescription.Texture2D.MipLevels = 1;

    UINT descriptorIncrementSize = ID3D12Device_GetDescriptorHandleIncrementSize(d3d12->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE heapHandle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(d3d12->descriptorHeap, &heapHandle);
    heapHandle.ptr += (textureId * descriptorIncrementSize);

    ID3D12Device_CreateShaderResourceView(d3d12->device, d3d12->textures[textureId], &shaderResourceViewDescription, heapHandle);

    d3d12->loadedTextureCount++;
    return textureId;
}

void D3D12FrameBegin(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    d3d12->vertexCount = 0;

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
}

void D3D12RectangleDraw(Win32Direct12 *d3d12, u32 textureId, Vector2 origin, Vector2 size, Vector4 color) {
    if (!d3d12 || !d3d12->commandList || !d3d12->vertexData) {
        return;
    }

    const UINT verticesPerRectangle = 6;
    if (d3d12->vertexCount + verticesPerRectangle > d3d12->vertexCapacity) {
        return;
    }

    f32 leftEdge = origin.x;
    f32 rightEdge = origin.x + size.width;
    f32 topEdge = origin.y;
    f32 bottomEdge = origin.y - size.height;

    Vertex topLeft = {{leftEdge, topEdge, 0.0f}, {color.r, color.g, color.b, color.a}, {0.0f, 0.0f}};
    Vertex topRight = {{rightEdge, topEdge, 0.0f}, {color.r, color.g, color.b, color.a}, {1.0f, 0.0f}};
    Vertex bottomLeft = {{leftEdge, bottomEdge, 0.0f}, {color.r, color.g, color.b, color.a}, {0.0f, 1.0f}};
    Vertex bottomRight = {{rightEdge, bottomEdge, 0.0f}, {color.r, color.g, color.b, color.a}, {1.0f, 1.0f}};

    Vertex *currentVertexDestination = (Vertex *)d3d12->vertexData + d3d12->vertexCount;

    currentVertexDestination[0] = topLeft;
    currentVertexDestination[1] = topRight;
    currentVertexDestination[2] = bottomLeft;
    currentVertexDestination[3] = bottomLeft;
    currentVertexDestination[4] = topRight;
    currentVertexDestination[5] = bottomRight;

    UINT memoryOffsetInBytes = d3d12->vertexCount * sizeof(Vertex);

    D3D12_VERTEX_BUFFER_VIEW rectangleBufferView;
    rectangleBufferView.BufferLocation = d3d12->vertexBufferView.BufferLocation + memoryOffsetInBytes;
    rectangleBufferView.StrideInBytes = sizeof(Vertex);
    rectangleBufferView.SizeInBytes = verticesPerRectangle * sizeof(Vertex);

    ID3D12GraphicsCommandList_IASetVertexBuffers(d3d12->commandList, 0, 1, &rectangleBufferView);
    ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(d3d12->commandList, 0, 1, &textureId, 0);

    ID3D12GraphicsCommandList_DrawInstanced(d3d12->commandList, verticesPerRectangle, 1, 0, 0);

    d3d12->vertexCount += verticesPerRectangle;
}

void D3D12RectangleDrawEX(Win32Direct12 *d3d12, u32 textureId, Vector2 origin, Vector2 size, Vector2 uvMin, Vector2 uvMax, Vector4 color) {
    if (!d3d12 || !d3d12->commandList || !d3d12->vertexData) {
        return;
    }

    const UINT verticesPerRectangle = 6;
    if (d3d12->vertexCount + verticesPerRectangle > d3d12->vertexCapacity) {
        return;
    }

    f32 leftEdge = origin.x;
    f32 rightEdge = origin.x + size.x;
    f32 topEdge = origin.y;
    f32 bottomEdge = origin.y - size.y;

    Vertex topLeft = {{leftEdge, topEdge, 0.0f}, {color.r, color.g, color.b, color.a}, {uvMin.x, uvMin.y}};
    Vertex topRight = {{rightEdge, topEdge, 0.0f}, {color.r, color.g, color.b, color.a}, {uvMax.x, uvMin.y}};
    Vertex bottomLeft = {{leftEdge, bottomEdge, 0.0f}, {color.r, color.g, color.b, color.a}, {uvMin.x, uvMax.y}};
    Vertex bottomRight = {{rightEdge, bottomEdge, 0.0f}, {color.r, color.g, color.b, color.a}, {uvMax.x, uvMax.y}};

    Vertex *currentVertexDestination = (Vertex *)d3d12->vertexData + d3d12->vertexCount;

    currentVertexDestination[0] = topLeft;
    currentVertexDestination[1] = topRight;
    currentVertexDestination[2] = bottomLeft;
    currentVertexDestination[3] = bottomLeft;
    currentVertexDestination[4] = topRight;
    currentVertexDestination[5] = bottomRight;

    UINT memoryOffsetInBytes = d3d12->vertexCount * sizeof(Vertex);

    D3D12_VERTEX_BUFFER_VIEW rectangleBufferView;
    rectangleBufferView.BufferLocation = d3d12->vertexBufferView.BufferLocation + memoryOffsetInBytes;
    rectangleBufferView.StrideInBytes = sizeof(Vertex);
    rectangleBufferView.SizeInBytes = verticesPerRectangle * sizeof(Vertex);

    ID3D12GraphicsCommandList_IASetVertexBuffers(d3d12->commandList, 0, 1, &rectangleBufferView);
    ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(d3d12->commandList, 0, 1, &textureId, 0);

    ID3D12GraphicsCommandList_DrawInstanced(d3d12->commandList, verticesPerRectangle, 1, 0, 0);

    d3d12->vertexCount += verticesPerRectangle;
}

void D3D12FrameEnd(Win32Direct12 *d3d12) {
    if (!d3d12) {
        return;
    }

    HRESULT hresult;

    D3D12_RESOURCE_BARRIER barrier;
    MemoryZero(&barrier, sizeof(barrier));

    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = d3d12->renderTargets[d3d12->frameIndex];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
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

void D3D12Initialize(Win32Direct12 *d3d12, HWND window) {
    if (!d3d12) {
        return;
    }

    MemoryZero(d3d12, sizeof(Win32Direct12));

    D3D12DeviceInitialize(d3d12);
    D3D12CommandsInitialize(d3d12);
    D3D12SwapChainInitialize(d3d12, window);
    D3D12HeapInitialize(d3d12);
    D3D12PipelineInitialize(d3d12);
    D3D12SynchronizationInitialize(d3d12);
    D3D12VertexBufferInitialize(d3d12, 4096);
}

