#include "D3DUtil.h"
#include "ShapesApp.h"

#include "FrameResource.h"


std::vector<std::unique_ptr<FrameResource>> mFrameResources;
FrameResource* mCurrentFrameResource = nullptr;
int mCurrFrameResourceIndex = 0;

void ShapesApp::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; i++)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(device, 1, (UINT)mAllRitems.size()));
    }
}
 
void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrentFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        if (e->NumFramesDirty > 0)
        {
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&e->World);

            ObjectConsts objConsts;
            DirectX::XMStoreFloat4x4(&ObjectConsts.World, DirectX::XMMatrixTranspose(world));

            currObjectCB->CopyData(e->ObjCBIndex, objConsts);
            e->NumFramesDirty--;
        }
    }
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
    DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&m_View);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&m_Proj);

    DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
    /*XMMatrixDeterminant(view) 计算矩阵的行列式，用于判断矩阵是否可逆（行列式非零则可逆）。
    若矩阵不可逆（比如奇异矩阵），返回的逆矩阵无意义，DirectX 会返回全零矩阵。*/
    DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(view), view);
    DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(proj), proj);
    DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(viewProj), viewProj);

    DirectX::XMStoreFloat4x4(&m_MainPassCB.View, DirectX::XMMatrixTranspose(view));
    DirectX::XMStoreFloat4x4(&m_MainPassCB.InvView, DirectX::XMMatrixTranspose(invView));
    DirectX::XMStoreFloat4x4(&m_MainPassCB.Proj, DirectX::XMMatrixTranspose(proj));
    DirectX::XMStoreFloat4x4(&m_MainPassCB.InvProj, DirectX::XMMatrixTranspose(invProj));
    DirectX::XMStoreFloat4x4(&m_MainPassCB.ViewProj, DirectX::XMMatrixTranspose(viewProj));
    DirectX::XMStoreFloat4x4(&m_MainPassCB.InvViewProj, DirectX::XMMatrixTranspose(invViewProj));
    m_MainPassCB.EyePosW = m_EyePos;
    m_MainPassCB.RenderTargetSize = DirectX::XMFLOAT2(static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight));
    m_MainPassCB.InvRenderTargetSize = DirectX::XMFLOAT2(static_cast<float>(1.0f / m_ClientWidth), static_cast<float>(1.0f / m_ClientHeight));
    m_MainPassCB.NearZ = 1.0f;
    m_MainPassCB.FarZ = 1000.0f;
    m_MainPassCB.TotalTime = gt.TotalTime();
    m_MainPassCB.DeltaTime = gt.DeltaTime();

    auto currPassCB = mCurrentFrameResource->PassCB.get();
    currPassCB->CopyData(0, m_MainPassCB);
}

void ShapesApp::Update(const GameTimer& gt)
{
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrentFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    //gpu端是否执行完处理当前帧资源的所有命令？
    //如果没有就让CPU等待
    if (mCurrentFrameResource->Fence != 0 && m_Fence->GetCompletedValue() < mCurrentFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_Fence->SetEventOnCompletion(mCurrentFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    ...更新资源
}

void ShapesApp::Draw(const GameTimer& gt)
{
    mCurrentFrameResource->Fence = ++m_CurrentFence;
    m_CommandQueue->Signal(m_Fence.Get(), m_CurrentFence);
}


