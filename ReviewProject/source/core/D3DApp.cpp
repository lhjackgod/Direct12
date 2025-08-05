#include "D3DUtil.h"
#include <GameTimer.h>
#include "D3DApp.h"

#include <array>
#include <assert.h>

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}
D3DApp* D3DApp::m_App = nullptr;
D3DApp::D3DApp(HINSTANCE hInstance)
    : mhAppInst(hInstance)
{
    assert(m_App == nullptr);
    m_App = this;
}

D3DApp::~D3DApp()
{
    if (m_Device!=nullptr)
        FlushCommandQueue();
}

D3DApp* D3DApp::GetApp()
{
    return m_App;
}

HINSTANCE D3DApp::AppInst() const
{
    return mhAppInst;
}

HWND D3DApp::MainWnd() const
{
    return mhMainWnd;
}

float D3DApp::AspectRatio() const
{
    return static_cast<float>(m_ClientWidth) / static_cast<float>(m_ClientHeight);
}

bool D3DApp::Get4xMsaaState() const
{
    return m4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value)
{
    if (m4xMsaaState != value)
    {
        m4xMsaaState = value;
        CreateSwapChain();
        OnResize();
    }
}

int D3DApp::Run()
{
    MSG mesg = {0};
    mTimer.Reset();
    while (mesg.message != WM_QUIT)
    {
        if (PeekMessage(&mesg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage( &mesg );
            DispatchMessage( &mesg );
        }
        else
        {
            mTimer.Tick();
            if (!mAppPaused)
            {
                CalculateFrameStates();
                Update(mTimer);
                Draw(mTimer);
            }
            else
            {
                Sleep(100);
            }
        }
    }
    return (int)mesg.wParam;
}

bool D3DApp::Initialize()
{
    if (!InitMainWindow()) return false;
    if (!InitDirect3D()) return false;

    OnResize();

    return true;
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        // WM_ACTIVATE is sent when the window is activated or deactivated.  
        // We pause the game when the window is deactivated and unpause it 
        // when it becomes active.  
    case WM_ACTIVATE:
        {
            if (LODWORD(wParam) == WA_INACTIVE)
            {
                mAppPaused = true;
                mTimer.Stop();
            }
            else
            {
                mAppPaused = false;
                mTimer.Start();
            }
            return 0;
        }
    case WM_SIZE:
        {
            m_ClientWidth = LOWORD(lParam);
            m_ClientHeight = HIWORD(lParam); // loword hiword代表取的位置
            if (m_Device)
            {
                if (wParam == SIZE_MINIMIZED)
                {
                    mAppPaused = true;
                    mMinimized = true;
                    mMaximized = false;
                }
                else if (wParam == SIZE_MAXIMIZED)
                {
                    mAppPaused = false;
                    mMinimized = false;
                    mMaximized = true;
                    OnResize();
                }
                else if (wParam == SIZE_RESTORED)
                {
                    if ( mMinimized )
                    {
                        mAppPaused = false;
                        mMinimized = false;
                        OnResize();
                    }
                    else if (mMaximized)
                    {
                        mAppPaused = false;
                        mMaximized = false;
                        OnResize();
                    }
                    else if (mResizing)
                    {
                        // If user is dragging the resize bars, we do not resize 
                        // the buffers here because as the user continuously 
                        // drags the resize bars, a stream of WM_SIZE messages are
                        // sent to the window, and it would be pointless (and slow)
                        // to resize for each WM_SIZE message received from dragging
                        // the resize bars.  So instead, we reset after the user is 
                        // done resizing the window and releases the resize bars, which 
                        // sends a WM_EXITSIZEMOVE message.
                    }
                    else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                    {
                        OnResize();
                    }
                }
            }
            return 0;
        }
        // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
        // when the user is grabing the window but not end operation
    case WM_ENTERSIZEMOVE:
        {
            mAppPaused = true;
            mResizing = true;
            mTimer.Stop();
            return 0;
        }
        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
    case WM_EXITSIZEMOVE:
        {
            mAppPaused = false;
            mResizing = false;
            mTimer.Start();
            OnResize();
            return 0;
        }
        // WM_DESTROY is sent when the window is being destroyed.
    case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        // The WM_MENUCHAR message is sent when a menu is active and the user presses 
        // a key that does not correspond to any mnemonic or accelerator key. 
    case WM_MENUCHAR:
        {
            return MAKELRESULT(0, MNC_CLOSE);
        }
        // Catch this message so to prevent the window from becoming too small.
    case WM_GETMINMAXINFO:
        {
            ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
            ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
            return 0;
        }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_KEYUP:
        {
            if (wParam == VK_ESCAPE)
            {
                PostQuitMessage(0);
            }
            else if ((int)wParam == VK_F2)
            {
                Set4xMsaaState(!m4xMsaaState);
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NodeMask = 0;
    rtvDesc.NumDescriptors = SwapChainBufferCount;
    m_Device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(m_RtvHeap.GetAddressOf()));
    
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.NodeMask = 0;
    dsvDesc.NumDescriptors = 1;
    m_Device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(m_DsvHeap.GetAddressOf()));
}

void D3DApp::OnResize()
{
    assert(m_Device);
    assert(m_SwapChain);
    assert(m_CommandAllocator);

    FlushCommandQueue();

    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        m_SwapChainBuffers[i].Reset();
    }
    m_DepthStencilBuffer.Reset();
    m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
    
    ThrowIfFailed(m_SwapChain->ResizeBuffers(SwapChainBufferCount,
        m_ClientWidth, m_ClientHeight, m_BackBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    m_CurrentBackBufferIndex = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        ThrowIfFailed(m_SwapChain->GetBuffer(i,
            IID_PPV_ARGS(m_SwapChainBuffers[i].GetAddressOf())));
        m_Device->CreateRenderTargetView(m_SwapChainBuffers[i].Get(),
            nullptr, rtvHandle);
        rtvHandle.Offset(1, m_RtvDescriptorSize);
    }

    D3D12_RESOURCE_DESC depthStencileDesc;
    depthStencileDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencileDesc.Alignment = 0;
    depthStencileDesc.Width = m_ClientWidth;
    depthStencileDesc.Height = m_ClientHeight;
    depthStencileDesc.DepthOrArraySize = 1;
    depthStencileDesc.MipLevels = 1;
    depthStencileDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencileDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencileDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    depthStencileDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencileDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES pHeapProperies = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_Device->CreateCommittedResource(&pHeapProperies,
        D3D12_HEAP_FLAG_NONE, &depthStencileDesc, D3D12_RESOURCE_STATE_COMMON,
        &clearValue, IID_PPV_ARGS(m_DepthStencilBuffer.GetAddressOf())));

    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
    depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;
    depthStencilViewDesc.Texture2D.MipSlice = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle(m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(),
        &depthStencilViewDesc, dsvDescriptorHandle);

    D3D12_RESOURCE_BARRIER pDsvResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_DepthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_CommandList->ResourceBarrier(1, &pDsvResourceBarrier);
    m_CommandList->Close();
    ID3D12CommandList* commandLists[] = {m_CommandList.Get()};
    m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    FlushCommandQueue();

    m_ViewPort.TopLeftX = 0;
    m_ViewPort.TopLeftY = 0;
    m_ViewPort.Width = m_ClientWidth;
    m_ViewPort.Height = m_ClientHeight;
    m_ViewPort.MinDepth = 0.0f;
    m_ViewPort.MaxDepth = 1.0f;

    m_ScissorRect = {0, 0, m_ClientWidth, m_ClientHeight};
}

bool D3DApp::InitMainWindow()
{
    WNDCLASS wc;

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hInstance = mhAppInst;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.lpfnWndProc = MainWndProc;
    wc.lpszClassName = L"MainWnd";
    wc.lpszMenuName = 0;

    if (!RegisterClass(&wc))
    {
        MessageBox(0, L"RegisterClass failed!", 0, 0);
        return false;
    }
    RECT R = {0, 0, m_ClientWidth, m_ClientHeight};
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    int width = R.right - R.left;
    int height = R.bottom - R.top;

    mhMainWnd = CreateWindow(L"MainWnd", m_MainWndCaption.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
    if (!mhMainWnd)
    {
        MessageBox(0, L"CreateWindow Failed.", 0, 0);
        return false;
    }
    ShowWindow(mhMainWnd, SW_SHOW);
    UpdateWindow(mhMainWnd);
    return true;
}

bool D3DApp::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugContex;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugContex.GetAddressOf())));
        debugContex->EnableDebugLayer();
    }
#endif
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_Factory.GetAddressOf())));
    HRESULT hardwareResult = D3D12CreateDevice(nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(m_Device.GetAddressOf()));
    if (FAILED(hardwareResult))
    {
        ComPtr<IDXGIAdapter1> pAdapter;
        ThrowIfFailed(m_Factory->EnumWarpAdapter(IID_PPV_ARGS(pAdapter.GetAddressOf())));

        ThrowIfFailed(D3D12CreateDevice(pAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(m_Device.GetAddressOf())));
    }

    ThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_Fence.GetAddressOf())));
    m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_DsvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_CbvSrvUavDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multiSamplingQualityLevels = {};
    multiSamplingQualityLevels.Format = m_BackBufferFormat;
    multiSamplingQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    multiSamplingQualityLevels.SampleCount = 4;
    multiSamplingQualityLevels.NumQualityLevels = 0;

    ThrowIfFailed(m_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &multiSamplingQualityLevels, sizeof(multiSamplingQualityLevels)));
    m4xMsaaQuality = multiSamplingQualityLevels.NumQualityLevels;
    assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level");

#if defined _DEBUG
    LogAdapters();
#endif

    CreateCommandObject();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();

    return true;
}

void D3DApp::CreateCommandObject()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.NodeMask = 0;

    ThrowIfFailed(m_Device->CreateCommandQueue(&queueDesc,
        IID_PPV_ARGS(m_CommandQueue.GetAddressOf())));
    ThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(m_CommandAllocator.GetAddressOf())));
    ThrowIfFailed(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(m_CommandList.GetAddressOf())));
    m_CommandList->Close();
}

void D3DApp::CreateSwapChain()
{
    m_SwapChain.Reset();
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Format = m_BackBufferFormat;
    swapChainDesc.BufferDesc.Height = m_ClientHeight;
    swapChainDesc.BufferDesc.Width = m_ClientWidth;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.Windowed = true;
    swapChainDesc.BufferCount = SwapChainBufferCount;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = mhMainWnd;
    swapChainDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    swapChainDesc.SampleDesc.Quality = m4xMsaaState ? m4xMsaaQuality - 1 : 0;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain> pSwapChain;
    ThrowIfFailed(m_Factory->CreateSwapChain(
        m_CommandQueue.Get(), &swapChainDesc, pSwapChain.GetAddressOf()));
    ThrowIfFailed(pSwapChain.As(&m_SwapChain));
}

void D3DApp::FlushCommandQueue()
{
    m_CurrentFence++;

    m_CommandQueue->Signal(m_Fence.Get(), m_CurrentFence);
    if (m_Fence->GetCompletedValue() < m_CurrentFence)
    {
        HANDLE pFenceEvent = CreateEventEx(nullptr, 0, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_Fence->SetEventOnCompletion(m_CurrentFence, pFenceEvent));
        WaitForSingleObject(pFenceEvent, INFINITE);
        CloseHandle(pFenceEvent);
    }
}

ID3D12Resource* D3DApp::CurrentBackBuffer() const
{
    return m_SwapChainBuffers[m_CurrentBackBufferIndex].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferHandle() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE res = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RtvHeap->GetCPUDescriptorHandleForHeapStart(), m_CurrentBackBufferIndex, m_RtvDescriptorSize);
    return res;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE res =
        CD3DX12_CPU_DESCRIPTOR_HANDLE(m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
    return res;
}

void D3DApp::CalculateFrameStates()
{
    // calculate the state per second, during this second, we calculate the frameNum,
    //because we call this func ever frame
    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;
    if (mTimer.TotalTime() - timeElapsed >= 1.0f)
    {
        float fps = static_cast<float>(frameCnt);
        float mspf = 1000.0f / fps;

        std::wstring fpsStr = std::to_wstring(fps);
        std::wstring mspfStr = std::to_wstring(mspf);

        std::wstring windowText = m_MainWndCaption +
            L"    fps: " + fpsStr +
            L"   mspf: " + mspfStr;

        SetWindowText(mhMainWnd, windowText.c_str());// 更改窗口标题栏的文字

        frameCnt = 0;
        timeElapsed += 1.0f; // ever second calculate
    }
}

void D3DApp::LogAdapters()
{
    UINT i = 0;
    ComPtr<IDXGIAdapter> pAdapter;
    std::vector<ComPtr<IDXGIAdapter>> adapterList;
    while (m_Factory->EnumAdapters(i, pAdapter.GetAddressOf()) !=
        DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc = {};
        pAdapter->GetDesc(&desc);

        std::wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";

        OutputDebugString(text.c_str());
        adapterList.push_back(pAdapter);
        ++i;
    }

    for (auto& adapter : adapterList)
    {
        LogAdapterOutputs(adapter.Get());
    }
}

void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
{
    UINT i = 0;
    ComPtr<IDXGIOutput> pOutput;
    while (adapter->EnumOutputs(i, pOutput.GetAddressOf()) !=
        DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc = {};
        pOutput->GetDesc(&desc);
        std::wstring text = L"***Output: ";
        text += desc.DeviceName;
        text += L"\n";
        OutputDebugString(text.c_str());
        LogOutputDisplayModes(pOutput.Get(), m_BackBufferFormat);
        i++;
    }
}

void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
    UINT count = 0;
    UINT flag = 0;
    output->GetDisplayModeList(format, flag, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flag, &count, modeList.data());

    for (auto& x : modeList)
    {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;

        std::wstring text =
            L"Width = " + std::to_wstring(x.Width) + L" " +
                L"Height" + std::to_wstring(x.Height) + L" " +
                    L"Refrash" + std::to_wstring(n) + L"/" + std::to_wstring(d) +
                        L"\n";
        OutputDebugString(text.c_str());
    }
}
