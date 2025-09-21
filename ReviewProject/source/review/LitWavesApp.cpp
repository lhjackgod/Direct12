#include "D3DUtil.h"
#include "LitWavesApp.h"

#include <DirectXColors.h>

#include "GeometryGenerator.h"

#ifdef LITWAVESAPP
LitWavesApp::LitWavesApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
    
}

void LitWavesApp::OnResize()
{
    D3DApp::OnResize();
    DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(0.5f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    DirectX::XMStoreFloat4x4(&m_Projection, p);
}


bool LitWavesApp::Initialize()
{
    if (!D3DApp::Initialize())
    {
        return false;
    }
    m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);

    BuildMaterial();
    CreateLandGeometry();
    CreateSeaGeometry();
    CreateLandRenderItems();
    CreateSeaRenderItems();
    CreateConstBuffer();
    CreateConstDescriptor();
    CreateConstBufferView();
    CreateRootSignature();
    BuildShadersAndInputLayout();
    BuildsPSO();

    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* cmdList[] = { m_CommandList.Get()};
    m_CommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
    FlushCommandQueue();
    return true;
}

float LitWavesApp::GetHillHeight(float x, float z) const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

DirectX::XMFLOAT3 LitWavesApp::GetHillsNormal(float x, float z) const
{
    DirectX::XMFLOAT3 n(-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z), 1.0f,
        -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));
    DirectX::XMVECTOR unitNormal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&n));
    DirectX::XMStoreFloat3(&n, unitNormal);
    return n;
}


void LitWavesApp::CreateLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData landGeometry = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    std::vector<Vertex> vertices(landGeometry.Vertices.size());
    for (UINT i = 0; i < landGeometry.Vertices.size(); i++)
    {
        Vertex v;
        v.Pos = landGeometry.Vertices[i].Position;
        v.Pos.y = GetHillHeight(v.Pos.x, v.Pos.z);
        vertices[i] = std::move(v);
        v.Normal = GetHillsNormal(v.Pos.x, v.Pos.z);
    }
    auto geo = std::make_unique<d3dUtil::MeshGeometry>();
    geo->Name = "land";
    UINT vbByteSize = sizeof(Vertex) * landGeometry.Vertices.size();
    UINT ibByteSize = sizeof(uint16_t) * landGeometry.GetIndices16().size();
    ThrowIfFailed(D3DCreateBlob(vbByteSize, geo->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibByteSize, geo->IndexBufferCPU.GetAddressOf()));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), sizeof(Vertex) * vertices.size());
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), landGeometry.GetIndices16().data(), sizeof(uint16_t) * landGeometry.GetIndices16().size());
    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), geo->VertexBufferCPU->GetBufferPointer(),
        sizeof(Vertex) * landGeometry.Vertices.size(), geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), geo->IndexBufferCPU->GetBufferPointer(),
        sizeof(uint16_t) * landGeometry.GetIndices16().size(), geo->IndexBufferUploader);
    
    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    d3dUtil::SubmeshGeometry gridSubMesh;
    gridSubMesh.IndexCount = landGeometry.GetIndices16().size();
    gridSubMesh.StartIndexLocation = 0;
    gridSubMesh.BaseVertexLocation = 0;
    geo->DrawArgs["grid"] = std::move(gridSubMesh);
    m_Geometries[geo->Name] = std::move(geo);
}

void LitWavesApp::CreateSeaGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData sea = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);
    std::unique_ptr<d3dUtil::MeshGeometry> seaGeo = std::make_unique<d3dUtil::MeshGeometry>();

    UINT vbByteSize = sizeof(Vertex) * sea.Vertices.size();
    UINT ibByteSize = sizeof(uint16_t) * sea.GetIndices16().size();
    seaGeo->Name = "sea";
    ThrowIfFailed(D3DCreateBlob(vbByteSize, seaGeo->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibByteSize, seaGeo->IndexBufferCPU.GetAddressOf()));
    CopyMemory(seaGeo->VertexBufferCPU->GetBufferPointer(), sea.Vertices.data(), vbByteSize);
    CopyMemory(seaGeo->IndexBufferCPU->GetBufferPointer(), sea.GetIndices16().data(), ibByteSize);

    m_SeaVertexBuffer = std::make_unique<UploadBuffer<Vertex>>(m_Device.Get(), sea.Vertices.size(), false);
    for (int i = 0; i < sea.Vertices.size(); i++)
    {
        Vertex p;
        p.Pos = sea.Vertices[i].Position;
        p.Normal = GetHillsNormal(p.Pos.x, p.Pos.z);
        m_SeaVertexBuffer->CopyData(i, p);
    }
    seaGeo->VertexBufferGPU = (ID3D12Resource*)*m_SeaVertexBuffer;
    seaGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), seaGeo->IndexBufferCPU->GetBufferPointer(), ibByteSize, seaGeo->IndexBufferUploader);
    seaGeo->VertexByteStride = sizeof(Vertex);
    seaGeo->VertexByteSize = vbByteSize;
    seaGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    seaGeo->IndexBufferByteSize = ibByteSize;

    d3dUtil::SubmeshGeometry seaSubmesh;
    seaSubmesh.IndexCount = sea.GetIndices16().size();
    seaSubmesh.BaseVertexLocation = 0;
    seaSubmesh.StartIndexLocation = 0;
    seaGeo->DrawArgs["grid"] = std::move(seaSubmesh);
    m_Geometries[seaGeo->Name] = std::move(seaGeo);
}


void LitWavesApp::CreateLandRenderItems()
{
    std::unique_ptr<RenderItem> landRItem = std::make_unique<RenderItem>();
    landRItem->Geo = m_Geometries["land"].get();
    landRItem->ObjCBIndex = 0;
    landRItem->IndexCount = landRItem->Geo->DrawArgs["grid"].IndexCount;
    landRItem->StartIndexLocation = landRItem->Geo->DrawArgs["grid"].StartIndexLocation;
    landRItem->BaseVertexLocation = landRItem->Geo->DrawArgs["grid"].BaseVertexLocation;
    landRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    landRItem->mat = m_Materials["grass"].get();

    m_OpaqueRitems.push_back(landRItem.get());
    m_AllRitems.push_back(std::move(landRItem));
}

void LitWavesApp::CreateSeaRenderItems()
{
    std::unique_ptr<RenderItem> seaRItem = std::make_unique<RenderItem>();
    seaRItem->ObjCBIndex = 1;
    seaRItem->Geo = m_Geometries["sea"].get();
    seaRItem->mat = m_Materials["water"].get();
    seaRItem->IndexCount = seaRItem->Geo->DrawArgs["grid"].IndexCount;
    seaRItem->StartIndexLocation = seaRItem->Geo->DrawArgs["grid"].StartIndexLocation;
    seaRItem->BaseVertexLocation = seaRItem->Geo->DrawArgs["grid"].BaseVertexLocation;
    seaRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    m_OpaqueRitems.push_back(seaRItem.get());
    m_AllRitems.push_back(std::move(seaRItem));
}



void LitWavesApp::BuildMaterial()
{
    std::unique_ptr<d3dUtil::Material> grass = std::make_unique<d3dUtil::Material>();
    grass->Name = "grass";
    grass->MatCBIndex = 0;
    grass->DiffuseAlbedo = DirectX::XMFLOAT4(0.2f, 0.6f, 0.2f, 1.0f);
    grass->FresnelR0 = DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f);
    grass->Roughness = 0.125f;

    //当前这种水的材质定义得并不是很好，但是由于我们还未学会所需的全部渲染工具（如透明度、环境反射等），因此暂时先用这些数据解决
    auto water = std::make_unique<d3dUtil::Material>();
    water->Name = "water";
    water->MatCBIndex = 1;
    water->DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.2f, 0.6f, 1.0f);
    water->FresnelR0 = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
    water->Roughness = 0.0f;

    m_Materials[grass->Name] = std::move(grass);
    m_Materials[water->Name] = std::move(water);
}

void LitWavesApp::CreateConstBuffer()
{
    for (UINT i = 0; i < gNumFrameResources; i++)
    {
        m_FrameResources.push_back(std::make_unique<FrameResource>(m_Device.Get(), 1, (UINT)m_AllRitems.size(), (UINT)m_Materials.size()));
    }
}


void LitWavesApp::CreateConstDescriptor()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    UINT constBufferCount = gNumFrameResources * (1 + m_AllRitems.size() + m_Materials.size());
    desc.NumDescriptors = constBufferCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.NodeMask = 0;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_CbvHeap.GetAddressOf())));
}

void LitWavesApp::CreateConstBufferView()
{
    UINT objectCount = (UINT)m_AllRitems.size();
    UINT objectByteSize = d3dUtil::CalcConstBufferByteSize(sizeof(ObjectConsts));
    UINT k = 0; //descriptor中的实际偏移位置
    for (UINT frame = 0; frame < gNumFrameResources; frame++)
    {
        ID3D12Resource* objCurrentResource = (ID3D12Resource*)*m_FrameResources[frame]->ObjectCB;
        D3D12_GPU_VIRTUAL_ADDRESS objGPUAddress = objCurrentResource->GetGPUVirtualAddress();
        for (UINT i = 0; i < objectCount; ++i, ++k)
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
            viewDesc.BufferLocation = objGPUAddress + (UINT)(i * objectByteSize);
            viewDesc.SizeInBytes = objectByteSize;
            CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_CbvHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(k, m_CbvSrvUavDescriptorSize);
            m_Device->CreateConstantBufferView(&viewDesc, handle);
        }
    }
    // material
    m_MaterialCbvBeginIndex = k;
    UINT materialCount = m_Materials.size();
    UINT materialByteSize = d3dUtil::CalcConstBufferByteSize(sizeof(MaterialConstants));
    for (UINT frame = 0; frame < gNumFrameResources; frame++)
    {
        ID3D12Resource* materialCurrentResource = (ID3D12Resource*)*m_FrameResources[frame]->MaterialCB;
        D3D12_GPU_VIRTUAL_ADDRESS materialGPUAddress = materialCurrentResource->GetGPUVirtualAddress();
        for (UINT i = 0; i < materialCount; ++i, ++k)
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
            viewDesc.BufferLocation = materialGPUAddress + (UINT)(i * materialByteSize);
            viewDesc.SizeInBytes = materialByteSize;
            CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_CbvHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(k, m_CbvSrvUavDescriptorSize);
            m_Device->CreateConstantBufferView(&viewDesc, handle);
        }
    }
    //pass
    UINT passByteSize = d3dUtil::CalcConstBufferByteSize(sizeof(PassConstant));
    m_PassCbvBeginIndex = k;
    for (UINT frame = 0; frame < gNumFrameResources; ++frame, ++k)
    {
        ID3D12Resource* passCurrentResource = (ID3D12Resource*)*m_FrameResources[frame]->PassCB;
        D3D12_GPU_VIRTUAL_ADDRESS passGPUAddress= passCurrentResource->GetGPUVirtualAddress();
        D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
        viewDesc.BufferLocation = passGPUAddress;
        viewDesc.SizeInBytes = passByteSize;
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_CbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(k, m_CbvSrvUavDescriptorSize);
        m_Device->CreateConstantBufferView(&viewDesc, handle);
    }
}

void LitWavesApp::CreateRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE descriptorRange[3];
    descriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    descriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    descriptorRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
    CD3DX12_ROOT_PARAMETER rootParameter[3];
    rootParameter[0].InitAsDescriptorTable( 1, &descriptorRange[0]);
    rootParameter[1].InitAsDescriptorTable(1, &descriptorRange[1]);
    rootParameter[2].InitAsDescriptorTable(1, &descriptorRange[2]);
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(3, rootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> error;
    ComPtr<ID3DBlob> rootSignature;
    
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, rootSignature.GetAddressOf(), error.GetAddressOf());
    if (error != nullptr)
    {
        OutputDebugStringA((char*)error->GetBufferPointer());
    }
    ThrowIfFailed(m_Device->CreateRootSignature(0, rootSignature->GetBufferPointer(), rootSignature->GetBufferSize(), IID_PPV_ARGS(m_RootSignature.GetAddressOf())));
}

void LitWavesApp::BuildShadersAndInputLayout()
{
    m_InputElementDescs = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    m_VSShader = d3dUtil::CompileShader(L"source/shader/Default.hlsl", nullptr, "VS", "vs_5_0");
    m_PSShader = d3dUtil::CompileShader(L"source/shader/Default.hlsl", nullptr, "PS", "ps_5_0");
}

void LitWavesApp::BuildsPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = m_RootSignature.Get();
    desc.VS = CD3DX12_SHADER_BYTECODE(m_VSShader.Get());
    desc.PS = CD3DX12_SHADER_BYTECODE(m_PSShader.Get());
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.InputLayout = {m_InputElementDescs.data(), (UINT)m_InputElementDescs.size()};
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = m_BackBufferFormat;
    desc.DSVFormat = m_DepthStencilFormat;
    desc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    desc.SampleDesc.Quality = m4xMsaaState ? m4xMsaaQuality - 1 : 0;
    desc.NodeMask = 0;
    ComPtr<ID3D12PipelineState> pPipeline;
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pPipeline.GetAddressOf())));
    m_PipelineStates["landSea"] = std::move(pPipeline);
}

void LitWavesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    m_MousePos = {x, y};
    SetCapture(mhMainWnd);
}

void LitWavesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
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
        m_Radius = MathHelper::Clamp(m_Radius, 3.0f, 100.0f);
    }
    m_MousePos = {x, y};
}

void LitWavesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}


void LitWavesApp::Update(const GameTimer& gt)
{
    m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % gNumFrameResources;
    m_CurrentFrameResource = m_FrameResources[m_CurrentFrameIndex].get();

    if (m_CurrentFrameResource->Fence != 0 && m_Fence->GetCompletedValue() < m_CurrentFrameResource->Fence)
    {
        HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_Fence->SetEventOnCompletion(m_CurrentFrameResource->Fence, event));
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }
    // 更新view
    float x = m_Radius * sinf(m_Phi) * cosf(m_Theta);
    float y = m_Radius * cosf(m_Phi);
    float z = m_Radius * sinf(m_Phi) * sinf(m_Theta);
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
    DirectX::XMVECTOR target = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    DirectX::XMStoreFloat4x4(&m_View, view);
    DirectX::XMStoreFloat3(&m_Eyepos, pos);
    UpdateFrameBuffer(gt);
    UpdateMaterialCBs(gt);
    UpdateWaterHeight(gt);
    UpdateRenderItems(gt);
}

void LitWavesApp::UpdateFrameBuffer(const GameTimer& gt)
{
    PassConstant passCB;

    DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&m_View);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&m_Projection);

    DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);

    DirectX::XMVECTOR viewDeter = DirectX::XMMatrixDeterminant(view);
    DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(&viewDeter, view);

    DirectX::XMVECTOR projDeter = DirectX::XMMatrixDeterminant(proj);
    DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&projDeter, proj);
    
    DirectX::XMVECTOR viewProjDeter = DirectX::XMMatrixDeterminant(viewProj);
    DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(&viewProjDeter, viewProj);
    
    DirectX::XMStoreFloat4x4(&passCB.View, DirectX::XMMatrixTranspose(view));
    DirectX::XMStoreFloat4x4(&passCB.InvView, DirectX::XMMatrixTranspose(invView));
    DirectX::XMStoreFloat4x4(&passCB.Proj, DirectX::XMMatrixTranspose(proj));
    DirectX::XMStoreFloat4x4(&passCB.InvProj, DirectX::XMMatrixTranspose(invProj));
    DirectX::XMStoreFloat4x4(&passCB.ViewProj, DirectX::XMMatrixTranspose(viewProj));
    DirectX::XMStoreFloat4x4(&passCB.InvViewProj, DirectX::XMMatrixTranspose(invViewProj));
    passCB.EyePosW = m_Eyepos;
    passCB.RenderTargetSize = DirectX::XMFLOAT2(static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight));
    passCB.InvRenderTargetSize = DirectX::XMFLOAT2(static_cast<float>(1.0f / m_ClientWidth), static_cast<float>(1.0f / m_ClientHeight));
    passCB.NearZ = 1.0f;
    passCB.FarZ = 1000.0f;
    passCB.TotalTime = gt.TotalTime();
    passCB.DeltaTime = gt.DeltaTime();
    DirectX::XMStoreFloat4(&passCB.gAmbientLight, DirectX::XMVectorSet(0.2f, 0.2f, 0.2f, 1.0f));

    float sunX = 1.0f * sinf(m_SunPhi) * cosf(m_SunTheta);
    float sunY = 1.0f * cosf(m_SunPhi);
    float sunZ = 1.0f * sinf(m_SunTheta) * sinf(m_SunPhi);
    DirectX::XMVECTOR lightDir = DirectX::XMVectorSet(-sunX, -sunY, -sunZ, 0.0f);
    DirectX::XMStoreFloat3(&passCB.gLights[0].Direction, lightDir);
    passCB.gLights[0].Strength = {1.0f, 1.0f, 0.9f};
    m_CurrentFrameResource->PassCB->CopyData(0, passCB);
}

void LitWavesApp::UpdateRenderItems(const GameTimer& gt)
{
    for (auto& r : m_AllRitems)
    {
        if (r->NumFrameDirty > 0)
        {
            r->NumFrameDirty--;
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&r->World);
            DirectX::XMMATRIX TInvWorld =MathHelper::InverseTanspose(world);
            ObjectConsts objConsts;
            DirectX::XMStoreFloat4x4(&objConsts.World, DirectX::XMMatrixTranspose(world));
            DirectX::XMStoreFloat4x4(&objConsts.TInvWorld, DirectX::XMMatrixTranspose(TInvWorld));

            m_CurrentFrameResource->ObjectCB->CopyData(r->ObjCBIndex, objConsts);
        }
    }
}

void LitWavesApp::UpdateMaterialCBs(const GameTimer& gt)
{
    UploadBuffer<MaterialConstants>* currMaterialCB = m_CurrentFrameResource->MaterialCB.get();
    for (auto& e : m_Materials)
    {
        d3dUtil::Material* mat = e.second.get();
        if (mat->NumFrameDirty > 0)
        {
            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;
            matConstants.MatTransform = mat->MatTransform;

            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
            mat->NumFrameDirty--;
        }
    }
}

float LitWavesApp::CalculateWaveHeight(float amplitude, float waveSpeed, float speed, float time, DirectX::XMFLOAT2 direction, DirectX::XMFLOAT2 pos_xy)
{
    float w = 2.0f / waveSpeed;
    float phi = speed * w;
    float dDotPos = direction.x * pos_xy.x + direction.y * pos_xy.y;
    float y = amplitude * sinf(w * dDotPos + time * phi);
    return y;
}

DirectX::XMFLOAT3 LitWavesApp::calculateNormal(float amplitude, float waveSpeed, float speed, float time, DirectX::XMFLOAT2 direction, DirectX::XMFLOAT2 pos_xy)
{
    DirectX::XMFLOAT3 normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
    float w = 2.0f / waveSpeed;
    float phi = speed * w;
    float k = cosf((direction.x * pos_xy.x + direction.y * pos_xy.y) * w + time * phi);
        
    float x = -1.0f * amplitude * w * direction.x * k;
    float z = -1.0f * amplitude * w * direction.y * k;
    return {x, 1.0f, z};
}


void LitWavesApp::UpdateWaterHeight(const GameTimer& gt)
{
    // 从原始网格数据获取顶点数量
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData sea = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    WaveParams waveParams;

    //然后根据数据去修改
    float time = gt.TotalTime();
    UINT vIdx = 0;
    for (auto& vv : sea.Vertices)
    {
        Vertex v;
        v.Pos = vv.Position;
        v.Normal = vv.Normal;
        DirectX::XMFLOAT2 seed;
        seed.x = MathHelper::frac(v.Pos.x * 0.1234f + v.Pos.z * 0.5678f);
        seed.y = MathHelper::frac(v.Pos.x * 0.8765f - v.Pos.z * 0.4321f);
        for (UINT i = 0; i <3; i++)
        {
            float seedOffset = static_cast<float>(i) * 0.333f;
            DirectX::XMFLOAT2 waveSeed;
            waveSeed.x = MathHelper::frac(seed.x + seedOffset);
            waveSeed.y = MathHelper::frac(seed.y + seedOffset * 1.7f);
            
            float amplitude = waveParams.A_min + 
                (waveParams.A_max - waveParams.A_min) * waveSeed.x;
            float waveLength = waveParams.WaveLength_min + 
                (waveParams.WaveLength_max - waveParams.WaveLength_min) * waveSeed.y;
            float speed = waveParams.Speed_min + 
                (waveParams.Speed_max - waveParams.Speed_min) * 
                MathHelper::frac(waveSeed.x + waveSeed.y);
            
            // 生成波浪方向
            float angle = waveSeed.x * DirectX::XM_2PI;
            DirectX::XMFLOAT2 direction(cosf(angle), sinf(angle));
            DirectX::XMFLOAT2 pos_xy = {v.Pos.x, v.Pos.z};
            
            // 累加波浪高度
            v.Pos.y += CalculateWaveHeight(amplitude, waveLength, speed, 
                                          time, direction, pos_xy);
            
            // 累加法线偏移
            DirectX::XMFLOAT3 deltaNormal = calculateNormal(amplitude, waveLength, 
                                                           speed, time, direction, pos_xy);
            v.Normal.x += deltaNormal.x;
            v.Normal.z += deltaNormal.z;
            v.Normal.y = 1.0f;
        }
        DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&v.Normal);
        n = DirectX::XMVector3Normalize(n);
        DirectX::XMStoreFloat3(&v.Normal, n);
        m_SeaVertexBuffer->CopyData(vIdx, v);
        ++vIdx;
    }
}


void LitWavesApp::Draw(const GameTimer& gt)
{
    ID3D12CommandAllocator* alloc = m_CurrentFrameResource->CmdListAlloc.Get();
    ThrowIfFailed(alloc->Reset());
    ThrowIfFailed(m_CommandList->Reset(alloc, m_PipelineStates["landSea"].Get()));
    m_CommandList->RSSetViewports(1, &m_ViewPort);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

    ID3D12Resource* renderTarget =  CurrentBackBuffer();
    CD3DX12_RESOURCE_BARRIER rtvBarrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE depthHandle = DepthStencilView();
    
    m_CommandList->ResourceBarrier(1, &rtvBarrier);
    m_CommandList->OMSetRenderTargets(1, &rtvHandle, true, &depthHandle);
    m_CommandList->ClearRenderTargetView(rtvHandle, DirectX::Colors::LightSteelBlue, 0, nullptr);
    m_CommandList->ClearDepthStencilView(depthHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    ID3D12DescriptorHeap* cbvD[] = { m_CbvHeap.Get() };
    m_CommandList->SetDescriptorHeaps(_countof(cbvD), cbvD);
    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_CbvHeap->GetGPUDescriptorHandleForHeapStart());
    cbvHandle.Offset(m_PassCbvBeginIndex + m_CurrentFrameIndex, m_CbvSrvUavDescriptorSize);
    m_CommandList->SetGraphicsRootDescriptorTable(2, cbvHandle);
    DrawRenderItems(m_CommandList.Get(), gt);
    rtvBarrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &rtvBarrier);
    ThrowIfFailed(m_CommandList->Close());

    ID3D12CommandList* cmdList[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
    ThrowIfFailed(m_SwapChain->Present(0, 0));
    m_CurrentBackBufferIndex = (m_CurrentBackBufferIndex + 1) % SwapChainBufferCount;
    m_CurrentFrameResource->Fence = ++m_CurrentFence;
    m_CommandQueue->Signal(m_Fence.Get(), m_CurrentFence);
}

void LitWavesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const GameTimer& gt)
{
    for (auto& r : m_AllRitems)
    {
        d3dUtil::MeshGeometry* geo = r->Geo;
        D3D12_VERTEX_BUFFER_VIEW vView = geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW iView = geo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vView);
        cmdList->IASetIndexBuffer(&iView);
        cmdList->IASetPrimitiveTopology(r->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE Mathandle(m_CbvHeap->GetGPUDescriptorHandleForHeapStart());
        CD3DX12_GPU_DESCRIPTOR_HANDLE objHandle(m_CbvHeap->GetGPUDescriptorHandleForHeapStart());
        d3dUtil::Material* mat = r->mat;
        Mathandle.Offset(m_MaterialCbvBeginIndex + (m_CurrentFrameIndex * m_Materials.size()) + mat->MatCBIndex, m_CbvSrvUavDescriptorSize);
        objHandle.Offset(m_CurrentFrameIndex * m_AllRitems.size() + r->ObjCBIndex, m_CbvSrvUavDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(0, objHandle);
        cmdList->SetGraphicsRootDescriptorTable(1, Mathandle);
        cmdList->DrawIndexedInstanced(r->IndexCount, 1, r->StartIndexLocation, r->BaseVertexLocation, 0);
    }
}

void LitWavesApp::OnKeyboardInpput(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
    {
        m_SunTheta -= 1.0f * dt;
    }
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
    {
        m_SunTheta += 1.0f * dt;
    }
    if (GetAsyncKeyState(VK_UP) & 0x8000)
    {
        m_SunPhi -= 1.0f * dt;
    }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
    {
        m_SunPhi += 1.0f * dt;
    }
    m_SunPhi = MathHelper::Clamp(m_SunPhi, 0.1f, DirectX::XM_PIDIV2);
}

#endif