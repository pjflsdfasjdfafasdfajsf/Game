// NOTE: Windows platform layer implementation.
//
//     * W version of the functions are used everywhere as they use a little bit
//     less memory and are more explicit
//
// TODO:
//    * Play some sound

#define COBJMACROS
#define UNICODE

#include "win32_d3d12.h"

#include <d3dcompiler.h>
#include <windows.h>
#include <winuser.h>

#define bool int
#define true 1
#define false 0

typedef size_t usize;

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define COM_OUT_POINTER(pointer) ((void **)(pointer))

int _fltused = 0;

void MemoryZero(void *destination, usize count) {
    char *bytePointer = (char *)destination;

    while (count--) {
        *bytePointer++ = 0;
    }
}

usize StringGetLength(const char *string) {
    const char *characterPointer = string;

    while (*characterPointer) {
        characterPointer++;
    }

    return characterPointer - string;
}

void ErrorShowLast(const wchar_t *functionName) {
    if (!functionName) {
        return;
    }

    DWORD errorCode = GetLastError();
    wchar_t *errorString = 0;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorString, 0, 0);

    wchar_t messageBuffer[1024];
    wsprintfW(messageBuffer, L"%s failed with %lu:\n%s", functionName, errorCode, errorString ? errorString : L"Unknown Error");

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
    wsprintfW(messageBuffer, L"%s failed with HRESULT 0x%08X:\n%s", functionName, hresult, errorString ? errorString : L"Unknown Error");

    MessageBoxW(0, messageBuffer, L"DirectX Error", MB_ICONERROR | MB_OK);

    if (errorString) {
        LocalFree(errorString);
    }

    ExitProcess(1);
}

void D3D12Initialize(D3D12 *d3d12) {
    if (!d3d12) {
        return;
    }

    MemoryZero(d3d12, sizeof(D3D12));
}

void D3D12DeviceInitialize(D3D12 *d3d12) {
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

void D3D12CommandsInitialize(D3D12 *d3d12) {
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

void D3D12SwapChainInitialize(D3D12 *d3d12, HWND windowHandle) {
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

void D3D12PipelineInitialize(D3D12 *d3d12) {
    HRESULT hresult;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDescription;
    MemoryZero(&rootSignatureDescription, sizeof(rootSignatureDescription));

    rootSignatureDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob *signatureBlob = 0;
    ID3DBlob *errorBlob = 0;

    hresult = D3D12SerializeRootSignature(&rootSignatureDescription, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hresult)) {
        if (errorBlob) {
            OutputDebugStringA((char *)errorBlob->lpVtbl->GetBufferPointer(errorBlob));
        }
        ErrorShowHRESULT(hresult, L"SerializeRootSignature");
    }

    hresult = ID3D12Device_CreateRootSignature(d3d12->device, 0, signatureBlob->lpVtbl->GetBufferPointer(signatureBlob), signatureBlob->lpVtbl->GetBufferSize(signatureBlob), &IID_ID3D12RootSignature, COM_OUT_POINTER(&d3d12->rootSignature));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateRootSignature");
    }

    signatureBlob->lpVtbl->Release(signatureBlob);

    const char *shaderCode =
        "struct PSInput {\n"
        "    float4 position : SV_POSITION;\n"
        "    float4 color : COLOR;\n"
        "};\n"
        "PSInput VSMain(uint vertexID : SV_VertexID) {\n"
        "    PSInput result;\n"
        "    if (vertexID == 0) { result.position = float4(0.0, 0.5, 0.0, 1.0); result.color = float4(1.0, 0.0, 0.0, 1.0); }\n"
        "    else if (vertexID == 1) { result.position = float4(0.5, -0.5, 0.0, 1.0); result.color = float4(0.0, 1.0, 0.0, 1.0); }\n"
        "    else { result.position = float4(-0.5, -0.5, 0.0, 1.0); result.color = float4(0.0, 0.0, 1.0, 1.0); }\n"
        "    return result;\n"
        "}\n"
        "float4 PSMain(PSInput input) : SV_TARGET {\n"
        "    return input.color;\n"
        "}\n";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob *vertexShader = 0;
    hresult = D3DCompile(shaderCode, StringGetLength(shaderCode), 0, 0, 0, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorBlob);

    if (FAILED(hresult)) {
        if (errorBlob) {
            OutputDebugStringA((char *)errorBlob->lpVtbl->GetBufferPointer(errorBlob));
        }
        ErrorShowHRESULT(hresult, L"Compile Vertex Shader");
    }

    ID3DBlob *pixelShader = 0;
    hresult = D3DCompile(shaderCode, StringGetLength(shaderCode), 0, 0, 0, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob);

    if (FAILED(hresult)) {
        if (errorBlob) {
            OutputDebugStringA((char *)errorBlob->lpVtbl->GetBufferPointer(errorBlob));
        }
        ErrorShowHRESULT(hresult, L"Compile Pixel Shader");
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDescription;
    MemoryZero(&pipelineStateDescription, sizeof(pipelineStateDescription));

    pipelineStateDescription.pRootSignature = d3d12->rootSignature;
    pipelineStateDescription.VS.pShaderBytecode = vertexShader->lpVtbl->GetBufferPointer(vertexShader);
    pipelineStateDescription.VS.BytecodeLength = vertexShader->lpVtbl->GetBufferSize(vertexShader);
    pipelineStateDescription.PS.pShaderBytecode = pixelShader->lpVtbl->GetBufferPointer(pixelShader);
    pipelineStateDescription.PS.BytecodeLength = pixelShader->lpVtbl->GetBufferSize(pixelShader);

    pipelineStateDescription.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipelineStateDescription.SampleMask = UINT_MAX;
    pipelineStateDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineStateDescription.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pipelineStateDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDescription.NumRenderTargets = 1;
    pipelineStateDescription.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineStateDescription.SampleDesc.Count = 1;

    hresult = ID3D12Device_CreateGraphicsPipelineState(d3d12->device, &pipelineStateDescription, &IID_ID3D12PipelineState, COM_OUT_POINTER(&d3d12->pipelineState));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateGraphicsPipelineState");
    }

    vertexShader->lpVtbl->Release(vertexShader);
    pixelShader->lpVtbl->Release(pixelShader);
}

void D3D12SynchronizationInitialize(D3D12 *d3d12) {
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

void D3D12DeviceWaitForGPU(D3D12 *d3d12) {
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

void D3D12DeviceRenderFrame(D3D12 *d3d12) {
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

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, (float)DEFAULT_WINDOW_WIDTH, (float)DEFAULT_WINDOW_HEIGHT, 0.0f, 1.0f};
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

    const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D12GraphicsCommandList_ClearRenderTargetView(d3d12->commandList, renderTargetViewHandle, clearColor, 0, 0);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(d3d12->commandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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
    MemoryZero(&windowClass, sizeof(windowClass));

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

void WindowShow(HWND windowHandle) {
    ShowWindow(windowHandle, SW_SHOWDEFAULT);

    if (!UpdateWindow(windowHandle)) {
        ErrorShowLast(L"UpdateWindow");
    }
}

void RunUpdateLoop(D3D12 *d3d12) {
    MSG message;
    MemoryZero(&message, sizeof(message));

    bool isRunning = true;

    while (isRunning) {
        if (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                isRunning = false;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        } else {
            D3D12DeviceRenderFrame(d3d12);
        }
    }
}

void WINAPI WinMainCRTStartup() {
    HWND mainWindowHandle = WindowCreate(L"Win32");

    if (!mainWindowHandle) {
        ExitProcess(1);
    }

    D3D12 d3d12;
    D3D12Initialize(&d3d12);

    D3D12DeviceInitialize(&d3d12);
    D3D12CommandsInitialize(&d3d12);
    D3D12SwapChainInitialize(&d3d12, mainWindowHandle);
    D3D12PipelineInitialize(&d3d12);
    D3D12SynchronizationInitialize(&d3d12);

    WindowShow(mainWindowHandle);

    RunUpdateLoop(&d3d12);

    D3D12DeviceWaitForGPU(&d3d12);

    ExitProcess(0);
}
