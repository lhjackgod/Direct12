#include "D3DUtil.h"
#include "ShapesApp.h"

#include <DirectXColors.h>

#include "FrameResource.h"
#include "core/GeometryGenerator.h"

ShapesApp::ShapesApp(HINSTANCE hInstance)
    :D3DApp(hInstance)
{
}

bool ShapesApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;
    
    ThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
    BuildShapeGeometry();
    BuildRenderItems();
    BuildConstHeadDescriptor();
    BuildFrameResources();
    BuildConstantBufferViews();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildPSO();

    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* cmdLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
    FlushCommandQueue();
    return true;
}

void ShapesApp::OnResize()
{
    D3DApp::OnResize();
    
    DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(0.5f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);
    DirectX::XMStoreFloat4x4(&m_Proj, p);
}

void ShapesApp::BuildFrameResources()
{
   for (int i = 0; i < gNumFrameResources; i++)
   {
       m_FrameResources.push_back(std::make_unique<FrameResource>(m_Device.Get(), 1, (UINT)mAllRitems.size()));
   }
}

void ShapesApp::BuildRenderItems()
{
    std::unique_ptr<RenderItem> boxRitem = std::make_unique<RenderItem>();
    boxRitem->Geo = m_Geometries["shapeGeo"].get();
    DirectX::XMStoreFloat4x4(&boxRitem->World, DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f) *
        DirectX::XMMatrixTranslation(0.0f, 0.5f, 0.0f));
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->ObjCBIndex = 0;
    boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    mAllRitems.push_back(std::move(boxRitem));

    UINT objCBIndex = 1;
    for (int i = 0; i < 5; i++)
    {
        auto leftCylRitem = std::make_unique<RenderItem>();
        auto rightCylRitem = std::make_unique<RenderItem>();
        auto leftSphereRitem = std::make_unique<RenderItem>();
        auto rightSphereRitem = std::make_unique<RenderItem>();

        DirectX::XMMATRIX leftCylWorld = DirectX::XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
        DirectX::XMMATRIX rightCylWorld = DirectX::XMMatrixTranslation(5.0f, 1.5f, -10.0f + i * 5.0f);

        DirectX::XMMATRIX leftSphereWorld = DirectX::XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
        DirectX::XMMATRIX rightSphereWorld = DirectX::XMMatrixTranslation(5.0f, 3.5f, -10.0f + i * 5.0f);

        DirectX::XMStoreFloat4x4(&leftCylRitem->World, leftCylWorld);
        leftCylRitem->ObjCBIndex = objCBIndex++;
        leftCylRitem->Geo = m_Geometries["shapeGeo"].get();
        leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
        leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
        leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;

        DirectX::XMStoreFloat4x4(&rightCylRitem->World, rightCylWorld);
        rightCylRitem->Geo = m_Geometries["shapeGeo"].get();
        rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
        rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
        rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        rightCylRitem->ObjCBIndex = objCBIndex++;

        DirectX::XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
        leftSphereRitem->Geo = m_Geometries["shapeGeo"].get();
        leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
        leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
        leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
        leftSphereRitem->ObjCBIndex = objCBIndex++;

        DirectX::XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
        rightSphereRitem->Geo = m_Geometries["shapeGeo"].get();
        rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
        rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
        rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
        rightSphereRitem->ObjCBIndex = objCBIndex++;
        mAllRitems.push_back(std::move(leftCylRitem));
        mAllRitems.push_back(std::move(rightCylRitem));
        mAllRitems.push_back(std::move(leftSphereRitem));
        mAllRitems.push_back(std::move(rightSphereRitem));
    }
    for (auto& e : mAllRitems)
    {
        mOpaqueRitems.push_back(e.get());
    }
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
    // update renderItem
    for (auto& r : mAllRitems)
    {
        if (r->NumFrameDirty)
        {
            r->NumFrameDirty--;
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&r->World);
            ObjectConsts objConstant;
            DirectX::XMStoreFloat4x4(&objConstant.World, DirectX::XMMatrixTranspose(world));
            m_CurrentFrameResource->ObjectCB->CopyData(r->ObjCBIndex, objConstant);
        }
    }
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
    DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&m_View);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&m_Proj);

    DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);

    DirectX::XMVECTOR viewDeter = DirectX::XMMatrixDeterminant(view);
    DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&viewDeter, view);

    DirectX::XMVECTOR projDeter = DirectX::XMMatrixDeterminant(proj);
    DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&projDeter, proj);
    
    DirectX::XMVECTOR viewProjDeter = DirectX::XMMatrixDeterminant(viewProj);
    DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&viewProjDeter, viewProj);
    
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

    auto currPassCB = m_CurrentFrameResource->PassCB.get();
    currPassCB->CopyData(0, m_MainPassCB);
}

void ShapesApp::Update(const GameTimer& gt)
{
    //循环往复地获取帧资源循环数组中的元素
    m_CurrentFrameResourceIndex = (m_CurrentFrameResourceIndex + 1) % gNumFrameResources;
    m_CurrentFrameResource = m_FrameResources[m_CurrentFrameResourceIndex].get();
    //gpu端是否已经执行完处理当前帧资源的所有命令呢？
    //如果还没有就令cpu等待，直到gpu完成命令的执行并抵达这个围栏点
    if (m_CurrentFrameResource->Fence != 0 && m_Fence->GetCompletedValue() < m_CurrentFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_Fence->SetEventOnCompletion(m_CurrentFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    float x = m_Radius * sinf(m_Phi) * cosf(m_Theta);
    float y = m_Radius * cosf(m_Phi);
    float z = m_Radius * sinf(m_Phi) * sinf(m_Theta);

    DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
    DirectX::XMVECTOR target = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    DirectX::XMStoreFloat4x4(&m_View, view);
    UpdateMainPassCB(gt);
    UpdateObjectCBs(gt);
    
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    for (UINT i = 0; i < ritems.size(); i++)
    {
        auto ri = ritems[i];
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView = ri->Geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW indexBufferView = ri->Geo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
        cmdList->IASetIndexBuffer(&indexBufferView);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        UINT objCbvIndex = m_CurrentFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
        CD3DX12_GPU_DESCRIPTOR_HANDLE handle(m_CbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        handle.Offset(objCbvIndex, m_CbvSrvUavDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(1, handle);
        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void ShapesApp::Draw(const GameTimer& gt)
{
    auto currentCmdAlloc = m_CurrentFrameResource->CmdListAlloc;
    ThrowIfFailed(currentCmdAlloc->Reset());
    ThrowIfFailed(m_CommandList->Reset(currentCmdAlloc.Get(), m_PSOs["ShapeApp"].Get()));

    m_CommandList->RSSetViewports(1, &m_ViewPort);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

    ID3D12Resource* frameTar = CurrentBackBuffer();
    D3D12_CPU_DESCRIPTOR_HANDLE renderTarHandle = CurrentBackBufferHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE depthTarHandle = DepthStencilView();
    CD3DX12_RESOURCE_BARRIER renderTarBarrier = CD3DX12_RESOURCE_BARRIER::Transition(frameTar, D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &renderTarBarrier);
    m_CommandList->OMSetRenderTargets(1, &renderTarHandle, true, &depthTarHandle);
    m_CommandList->ClearRenderTargetView(renderTarHandle, DirectX::Colors::LightSteelBlue, 0, nullptr);
    m_CommandList->ClearDepthStencilView(depthTarHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    //绑定到渲染流水线的descriptorheap
    ID3D12DescriptorHeap* descritptorHeaps[] = { m_CbvDescriptorHeap.Get()};
    m_CommandList->SetDescriptorHeaps(_countof(descritptorHeaps), descritptorHeaps);
    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
    int passCbvIndex = m_PassCbvOffset + m_CurrentFrameResourceIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(m_CbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    handle.Offset(passCbvIndex, m_CbvSrvUavDescriptorSize);
    m_CommandList->SetGraphicsRootDescriptorTable(0, handle);
    
    DrawRenderItems(m_CommandList.Get(), mOpaqueRitems);
    renderTarBarrier = CD3DX12_RESOURCE_BARRIER::Transition(frameTar, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &renderTarBarrier);
    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* cmdLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
    ThrowIfFailed(m_SwapChain->Present(0, 0));
    m_CurrentBackBufferIndex = (m_CurrentBackBufferIndex + 1) % SwapChainBufferCount;

    m_CurrentFrameResource->Fence = ++m_CurrentFence;
    m_CommandQueue->Signal(m_Fence.Get(), m_CurrentFence);
}

void ShapesApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    UINT boxVertexOffset = 0;
    UINT sphereVertexOffset = (UINT)box.Vertices.size();
    UINT cylinderVertexOffset = (UINT)sphere.Vertices.size() + sphereVertexOffset;

    UINT boxIndexOffset = 0;
    UINT sphereIndexOffset = (UINT)box.Indices32.size();
    UINT cylinderIndexOffset = (UINT)sphere.Indices32.size() + sphereIndexOffset;

    d3dUtil::SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    d3dUtil::SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    d3dUtil::SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    //提取出所需的顶点元素，再将所有网络的顶点装进一个顶点缓冲区
    auto totalVertexCount = box.Vertices.size() +
        sphere.Vertices.size() + cylinder.Vertices.size();
    std::vector<Vertex> vertices(totalVertexCount);
    UINT k = 0;
    
    for (size_t i = 0; i < box.Vertices.size(); i++, k++)
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Color = DirectX::XMFLOAT4(DirectX::Colors::DarkGreen);
    }
    for (size_t i = 0; i < sphere.Vertices.size(); i++, k++)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Color = DirectX::XMFLOAT4(DirectX::Colors::ForestGreen);
    }
    for (size_t i = 0; i < cylinder.Vertices.size(); i++, k++)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Color = DirectX::XMFLOAT4(DirectX::Colors::Crimson);
    }
    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
    indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());
    indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
    auto geo = std::make_unique<d3dUtil::MeshGeometry>();
    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, geo->VertexBufferCPU.GetAddressOf()));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, geo->IndexBufferCPU.GetAddressOf()));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexByteSize = vbByteSize;
    geo->IndexBufferByteSize = ibByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;

    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;
    
    m_Geometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildConstHeadDescriptor()
{
    UINT objCount = (UINT)mAllRitems.size();

    //我们需要为每个帧资源中的每个物体都创建一个cbv描述符
    //为了容纳每个帧资源中的渲染过程CBV而+1
    UINT numDescriptors = (objCount + 1) * gNumFrameResources;
    m_PassCbvOffset = gNumFrameResources * objCount;
    
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc{};
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.NodeMask = 0;
    cbvHeapDesc.NumDescriptors = numDescriptors;
    m_Device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(m_CbvDescriptorHeap.GetAddressOf()));
}

void ShapesApp::BuildConstantBufferViews()
{
    UINT objCBByteSize = d3dUtil::CalcConstBufferByteSize(sizeof(ObjectConsts));

    UINT objCount = (UINT)mOpaqueRitems.size();
    for (UINT i = 0; i < gNumFrameResources; i++)
    {
        ID3D12Resource* objectCB = (ID3D12Resource*)*m_FrameResources[i]->ObjectCB;
        for (UINT j = 0; j < objCount; j++)
        {
            D3D12_GPU_VIRTUAL_ADDRESS cbvAdress = objectCB->GetGPUVirtualAddress();
            cbvAdress += j * objCBByteSize;
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = cbvAdress;
            cbvDesc.SizeInBytes = objCBByteSize;

            int heapIndex = i * objCount + j;

            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CbvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(heapIndex, m_CbvSrvUavDescriptorSize);
            m_Device->CreateConstantBufferView(&cbvDesc, handle); 
        }
    }

    UINT passCBByteSize = d3dUtil::CalcConstBufferByteSize(sizeof(PassConstant));

    for (UINT frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++)
    {
        ID3D12Resource* passCB = (ID3D12Resource*)*m_FrameResources[frameIndex]->PassCB;
        D3D12_GPU_VIRTUAL_ADDRESS cbvAdress = passCB->GetGPUVirtualAddress();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = cbvAdress;
        cbvDesc.SizeInBytes = passCBByteSize;
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CbvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(m_PassCbvOffset + frameIndex, m_CbvSrvUavDescriptorSize);
        m_Device->CreateConstantBufferView(&cbvDesc, handle);
    }
}

void ShapesApp::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE passCbv;
    passCbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE objCbv;
    objCbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    
    CD3DX12_ROOT_PARAMETER rootParameters[2];
    rootParameters[0].InitAsDescriptorTable(1, &passCbv);
    rootParameters[1].InitAsDescriptorTable(1, &objCbv);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(2, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> error;
    ComPtr<ID3DBlob> rootSignature;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, rootSignature.GetAddressOf(), error.GetAddressOf());
    if (error != nullptr)
    {
        OutputDebugStringA((char*)error->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    m_Device->CreateRootSignature(0, rootSignature->GetBufferPointer(), rootSignature->GetBufferSize(), IID_PPV_ARGS(m_RootSignature.GetAddressOf()));
}

void ShapesApp::BuildShadersAndInputLayout()
{
    m_InputElementDesc = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    m_VSShader = d3dUtil::CompileShader(L"source/shader/ShapeApp.usf", nullptr, "VS", "vs_5_0");
    m_PSShader = d3dUtil::CompileShader(L"source/shader/ShapeApp.usf", nullptr, "PS", "ps_5_0");
}

void ShapesApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_RootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSShader.Get());
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = {m_InputElementDesc.data(), (UINT)m_InputElementDesc.size()};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_BackBufferFormat;
    psoDesc.DSVFormat = m_DepthStencilFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? m4xMsaaQuality - 1 : 0;
    psoDesc.NodeMask = 0;

    ComPtr<ID3D12PipelineState> pipelineStaet;
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineStaet.GetAddressOf())));
    
    m_PSOs["ShapeApp"] = std::move(pipelineStaet);
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    m_MousePos = {x, y};
    SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) !=0 )
    {
        float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - m_MousePos.x));
        float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - m_MousePos.y));

        m_Phi += dy;
        m_Theta += dx;
        m_Phi = MathHelper::Clamp(m_Phi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.005f * static_cast<float>(x - m_MousePos.x);
        float dy = 0.005f * static_cast<float>(y - m_MousePos.y);
        m_Radius += (dx - dy);
        m_Radius = MathHelper::Clamp(m_Radius, 3.0f, 15.0f);
    }
    m_MousePos = {x, y};
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}


