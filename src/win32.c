// NOTE: Windows platform layer implementation.
//
//     * W version of the functions are used everywhere as they use a little bit
//     less memory and are more explicit
//
// TODO:
//    * Render a triangle
//    * Play some sound

#define COBJMACROS
#define UNICODE

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <windows.h>
#include <winuser.h>

#define bool int
#define true 1
#define false 0

typedef size_t usize;

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720
#define FRAME_COUNT 3

#define COM_OUT_POINTER(pointer) ((void **)(pointer))

int _fltused = 0;

IDXGIFactory4 *globalFactory = 0;
ID3D12Device *globalDevice = 0;
ID3D12CommandQueue *globalCommandQueue = 0;
ID3D12CommandAllocator *globalCommandAllocator = 0;
ID3D12GraphicsCommandList *globalCommandList = 0;

IDXGISwapChain3 *globalSwapChain = 0;
ID3D12DescriptorHeap *globalRenderTargetViewHeap = 0;
ID3D12Resource *globalRenderTargets[FRAME_COUNT];
UINT globalRenderTargetViewDescriptorSize = 0;
UINT globalFrameIndex = 0;

ID3D12RootSignature *globalRootSignature = 0;
ID3D12PipelineState *globalPipelineState = 0;

ID3D12Fence *globalFence = 0;
UINT64 globalFenceValue = 0;
HANDLE globalFenceEvent = 0;

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

void D3D12DeviceInitialize() {
    HRESULT hresult;

    hresult = CreateDXGIFactory1(&IID_IDXGIFactory4, COM_OUT_POINTER(&globalFactory));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateDXGIFactory1");
    }

    hresult = D3D12CreateDevice(0, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, COM_OUT_POINTER(&globalDevice));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"D3D12CreateDevice");
    }
}

void D3D12CommandsInitialize() {
    HRESULT hresult;

    D3D12_COMMAND_QUEUE_DESC commandQueueDescription;
    MemoryZero(&commandQueueDescription, sizeof(commandQueueDescription));

    commandQueueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hresult = ID3D12Device_CreateCommandQueue(globalDevice, &commandQueueDescription, &IID_ID3D12CommandQueue, COM_OUT_POINTER(&globalCommandQueue));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateCommandQueue");
    }

    hresult = ID3D12Device_CreateCommandAllocator(globalDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, COM_OUT_POINTER(&globalCommandAllocator));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateCommandAllocator");
    }

    hresult = ID3D12Device_CreateCommandList(globalDevice, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, globalCommandAllocator, 0, &IID_ID3D12GraphicsCommandList, COM_OUT_POINTER(&globalCommandList));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateCommandList");
    }

    hresult = ID3D12GraphicsCommandList_Close(globalCommandList);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandList Close Initial");
    }
}

void D3D12SwapChainInitialize(HWND windowHandle) {
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
    hresult = IDXGIFactory4_CreateSwapChainForHwnd(globalFactory, (IUnknown *)globalCommandQueue, windowHandle, &swapChainDescription, 0, 0, &temporarySwapChain);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateSwapChainForHwnd");
    }

    hresult = IDXGISwapChain1_QueryInterface(temporarySwapChain, &IID_IDXGISwapChain3, COM_OUT_POINTER(&globalSwapChain));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"QueryInterface SwapChain3");
    }
    IDXGISwapChain1_Release(temporarySwapChain);

    globalFrameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(globalSwapChain);

    D3D12_DESCRIPTOR_HEAP_DESC renderTargetViewHeapDescription;
    MemoryZero(&renderTargetViewHeapDescription, sizeof(renderTargetViewHeapDescription));

    renderTargetViewHeapDescription.NumDescriptors = FRAME_COUNT;
    renderTargetViewHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    hresult = ID3D12Device_CreateDescriptorHeap(globalDevice, &renderTargetViewHeapDescription, &IID_ID3D12DescriptorHeap, COM_OUT_POINTER(&globalRenderTargetViewHeap));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateDescriptorHeap");
    }

    globalRenderTargetViewDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(globalDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(globalRenderTargetViewHeap, &renderTargetViewHandle);

    for (index = 0; index < (UINT)FRAME_COUNT; index++) {
        hresult = IDXGISwapChain3_GetBuffer(globalSwapChain, index, &IID_ID3D12Resource, COM_OUT_POINTER(&globalRenderTargets[index]));
        if (FAILED(hresult)) {
            ErrorShowHRESULT(hresult, L"SwapChain GetBuffer");
        }

        ID3D12Device_CreateRenderTargetView(globalDevice, globalRenderTargets[index], 0, renderTargetViewHandle);
        renderTargetViewHandle.ptr += globalRenderTargetViewDescriptorSize;
    }
}

void D3D12PipelineInitialize() {
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

    hresult = ID3D12Device_CreateRootSignature(globalDevice, 0, signatureBlob->lpVtbl->GetBufferPointer(signatureBlob), signatureBlob->lpVtbl->GetBufferSize(signatureBlob), &IID_ID3D12RootSignature, COM_OUT_POINTER(&globalRootSignature));
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

    pipelineStateDescription.pRootSignature = globalRootSignature;
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

    hresult = ID3D12Device_CreateGraphicsPipelineState(globalDevice, &pipelineStateDescription, &IID_ID3D12PipelineState, COM_OUT_POINTER(&globalPipelineState));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateGraphicsPipelineState");
    }

    vertexShader->lpVtbl->Release(vertexShader);
    pixelShader->lpVtbl->Release(pixelShader);
}

void D3D12SynchronizationInitialize() {
    HRESULT hresult;

    hresult = ID3D12Device_CreateFence(globalDevice, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, COM_OUT_POINTER(&globalFence));
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CreateFence");
    }

    globalFenceEvent = CreateEventW(0, FALSE, FALSE, 0);
    if (!globalFenceEvent) {
        ErrorShowLast(L"CreateEventW");
    }
}

void D3D12DeviceWaitForGPU() {
    HRESULT hresult;
    const UINT64 fenceToWaitFor = globalFenceValue;

    hresult = ID3D12CommandQueue_Signal(globalCommandQueue, globalFence, fenceToWaitFor);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandQueue Signal");
    }

    globalFenceValue++;

    if (ID3D12Fence_GetCompletedValue(globalFence) < fenceToWaitFor) {
        hresult = ID3D12Fence_SetEventOnCompletion(globalFence, fenceToWaitFor, globalFenceEvent);

        if (FAILED(hresult)) {
            ErrorShowHRESULT(hresult, L"SetEventOnCompletion");
        }

        WaitForSingleObject(globalFenceEvent, INFINITE);
    }

    globalFrameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(globalSwapChain);
}

void D3D12DeviceRenderFrame() {
    HRESULT hresult;

    hresult = ID3D12CommandAllocator_Reset(globalCommandAllocator);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandAllocator Reset");
    }

    hresult = ID3D12GraphicsCommandList_Reset(globalCommandList, globalCommandAllocator, globalPipelineState);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandList Reset");
    }

    ID3D12GraphicsCommandList_SetGraphicsRootSignature(globalCommandList, globalRootSignature);

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, (float)DEFAULT_WINDOW_WIDTH, (float)DEFAULT_WINDOW_HEIGHT, 0.0f, 1.0f};
    ID3D12GraphicsCommandList_RSSetViewports(globalCommandList, 1, &viewport);

    D3D12_RECT scissorRectangle = {0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT};
    ID3D12GraphicsCommandList_RSSetScissorRects(globalCommandList, 1, &scissorRectangle);

    D3D12_RESOURCE_BARRIER barrier;
    MemoryZero(&barrier, sizeof(barrier));

    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = globalRenderTargets[globalFrameIndex];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    ID3D12GraphicsCommandList_ResourceBarrier(globalCommandList, 1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(globalRenderTargetViewHeap, &renderTargetViewHandle);
    renderTargetViewHandle.ptr += globalFrameIndex * globalRenderTargetViewDescriptorSize;

    ID3D12GraphicsCommandList_OMSetRenderTargets(globalCommandList, 1, &renderTargetViewHandle, FALSE, 0);

    const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D12GraphicsCommandList_ClearRenderTargetView(globalCommandList, renderTargetViewHandle, clearColor, 0, 0);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(globalCommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12GraphicsCommandList_DrawInstanced(globalCommandList, 3, 1, 0, 0);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    ID3D12GraphicsCommandList_ResourceBarrier(globalCommandList, 1, &barrier);

    hresult = ID3D12GraphicsCommandList_Close(globalCommandList);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"CommandList Close");
    }

    ID3D12CommandList *commandLists[] = {(ID3D12CommandList *)globalCommandList};
    ID3D12CommandQueue_ExecuteCommandLists(globalCommandQueue, 1, commandLists);

    hresult = IDXGISwapChain3_Present(globalSwapChain, 1, 0);
    if (FAILED(hresult)) {
        ErrorShowHRESULT(hresult, L"SwapChain Present");
    }

    D3D12DeviceWaitForGPU();
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

void RunUpdateLoop() {
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
            D3D12DeviceRenderFrame();
        }
    }
}

void WINAPI WinMainCRTStartup() {
    HWND mainWindowHandle = WindowCreate(L"Win32");

    if (!mainWindowHandle) {
        ExitProcess(1);
    }

    D3D12DeviceInitialize();
    D3D12CommandsInitialize();
    D3D12SwapChainInitialize(mainWindowHandle);
    D3D12PipelineInitialize();
    D3D12SynchronizationInitialize();

    WindowShow(mainWindowHandle);

    RunUpdateLoop();

    D3D12DeviceWaitForGPU();

    ExitProcess(0);
}
