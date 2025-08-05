#include "D3DUtil.h"
#include "review/reviewWinApp.h"
#include <DirectXMath.h>
#include <DirectXColors.h>
#include "core/UploadBuffer.h"
bool ReviewApp::Initialize()
{
    if (!D3DApp::Initialize())
    {
        return false;
    }
    return true;
}

void ReviewApp::Update(const GameTimer& gt)
{
}

void ReviewApp::Draw(const GameTimer& gt)
{
    ThrowIfFailed(m_CommandAllocator->Reset());

    ThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(),
        nullptr));

    D3D12_RESOURCE_BARRIER rtvResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_SwapChainBuffers[m_CurrentBackBufferIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &rtvResourceBarrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DepthStencilView();
    m_CommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);
    m_CommandList->RSSetViewports(1, &m_ViewPort);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

    float clearColor[4] = {1.0f, 1.0f, 0.0f, 1.0f};
    m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    D3D12_RESOURCE_BARRIER rtvResourceBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_SwapChainBuffers[m_CurrentBackBufferIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &rtvResourceBarrier2);
    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* commandLists[] = {m_CommandList.Get()};
    m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    ThrowIfFailed(m_SwapChain->Present(0, 0));
    m_CurrentBackBufferIndex = (m_CurrentBackBufferIndex + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}
