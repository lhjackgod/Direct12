#pragma once

// 主要用于在调试模式下启用内存泄漏检测功能
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
//作用：将标准 C/C++ 的内存分配函数（
//如 malloc, free, new, delete）映射到调试版本，
//这样可以在内存泄漏检测时显示具体的文件和行号
#include <crtdbg.h>
#endif
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#include "GameTimer.h"
using Microsoft::WRL::ComPtr;
class GameTimer;
class D3DApp
{
protected:
    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();

public:
    static D3DApp* GetApp();
    HINSTANCE AppInst() const;
    HWND MainWnd() const;
    float AspectRatio() const;
    bool Get4xMsaaState() const;
    void Set4xMsaaState(bool value);
    int Run();

    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
protected:
    virtual void CreateRtvAndDsvDescriptorHeaps();
    virtual void OnResize();
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;

    //便于重写鼠标输入消息的处理流程
    virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
    virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
    virtual void OnMouseMove(WPARAM btnState, int x, int y) {}
    
protected:
    bool InitMainWindow();
    bool InitDirect3D();
    void CreateCommandObject();
    void CreateSwapChain();

    void FlushCommandQueue();
    ID3D12Resource* CurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    void CalculateFrameStates();
    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);
protected:
    static D3DApp* m_App;

    
    HINSTANCE mhAppInst    = nullptr;// application instance handle
    HWND mhMainWnd         = nullptr; // main window handle
    bool mAppPaused        = false;// is the application paused?
    bool mMinimized        = false;// is the application minimized?
    bool mMaximized        = false;// is the application maximized?
    bool mResizing         = false;// are the resize bars being dragged?
    bool mFullScreenState  = false; // fullscreen enabled

    bool      m4xMsaaState = false;    // 4X MSAA enabled
    UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

    GameTimer mTimer;

    ComPtr<IDXGIFactory7> m_Factory;
    ComPtr<IDXGISwapChain4> m_SwapChain;
    
    ComPtr<ID3D12Device> m_Device;
    ComPtr<ID3D12Fence> m_Fence;
    UINT64 m_CurrentFence = 0;
    ComPtr<ID3D12CommandQueue> m_CommandQueue;
    ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    static const int SwapChainBufferCount = 2;
    int m_CurrentBackBufferIndex = 0;
    ComPtr<ID3D12Resource> m_SwapChainBuffers[SwapChainBufferCount];
    ComPtr<ID3D12Resource> m_DepthStencilBuffer;

    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_DsvHeap;

    D3D12_VIEWPORT m_ViewPort;
    D3D12_RECT m_ScissorRect;

    UINT m_RtvDescriptorSize;
    UINT m_DsvDescriptorSize;
    UINT m_CbvSrvUavDescriptorSize;

    // Derived class should set these in derived constructor to customize starting values.
    std::wstring m_MainWndCaption = L"d3d App";
    D3D_DRIVER_TYPE m_DriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    int m_ClientWidth = 800;
    int m_ClientHeight = 600;
};
