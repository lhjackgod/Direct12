#include "D3DUtil.h"
#include "MaterialApp.h"
#include "DDSTexttureLoader.h"
#include <array>
#include "core/GeometryGenerator.h"
#include "core/DDSTexttureLoader.h"
#include "DirectXColors.h"
MaterialApp::MaterialApp(HINSTANCE hInstace)
    :D3DApp(hInstace)
{

}
bool MaterialApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;
    m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
    CreateGeometries();
    LoadDefaultWhiteTexture();
    LoadTextureResources();
    CreateSRVDescriptorHeap();
    CreateSRVView();
    CreateMaterial();
    CreateRenderItems();
    CreateFrameResources();
    CreateShaderAndInputLayout();
    createRootSignature();
    CreatePSO();

    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* cmdLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
    FlushCommandQueue();
}

void MaterialApp::OnResize()
{
    D3DApp::OnResize();
    DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(0.5f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    DirectX::XMStoreFloat4x4(&m_Projection, proj);
}


float MaterialApp::GetHillHeight(float x, float z) const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

DirectX::XMFLOAT3 MaterialApp::GetHillsNormal(float x, float z) const
{
    DirectX::XMFLOAT3 n(-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z), 1.0f,
             -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));
    DirectX::XMVECTOR unitNormal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&n));
    DirectX::XMStoreFloat3(&n, unitNormal);
    return n;
}

void MaterialApp::CreateGeometries()
{
    //first get data
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData land = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);
    GeometryGenerator::MeshData box = geoGen.CreateBox(10, 10, 10);
    // land and box
    std::vector<Vertex> landVertex(land.Vertices.size());
    for (UINT i = 0; i < land.Vertices.size(); ++i)
    {
        Vertex v;
        v.Pos = land.Vertices[i].Position;
        v.Pos.y = GetHillHeight(v.Pos.x, v.Pos.z);
        v.Normal = GetHillsNormal(v.Pos.x, v.Pos.z);
        v.TexC = land.Vertices[i].TexC;
        landVertex[i] = std::move(v);
    }
    //land geometry
    std::unique_ptr<d3dUtil::MeshGeometry> landGeo = std::make_unique<d3dUtil::MeshGeometry>();
    UINT vbByteSize = sizeof(Vertex) * landVertex.size();
    UINT ibByteSize = sizeof(uint16_t) * land.GetIndices16().size();
    ThrowIfFailed(D3DCreateBlob(vbByteSize, landGeo->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibByteSize, landGeo->IndexBufferCPU.GetAddressOf()));
    CopyMemory(landGeo->VertexBufferCPU->GetBufferPointer(), landVertex.data(), vbByteSize);
    CopyMemory(landGeo->IndexBufferCPU->GetBufferPointer(), land.GetIndices16().data(), ibByteSize);
    landGeo->Name = "land";
    landGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), landGeo->VertexBufferCPU->GetBufferPointer(),
        vbByteSize, landGeo->VertexBufferUploader);
    landGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), landGeo->IndexBufferCPU->GetBufferPointer(),
        ibByteSize, landGeo->IndexBufferUploader);
    landGeo->VertexByteStride = sizeof(Vertex);
    landGeo->VertexByteSize = vbByteSize;
    landGeo->IndexBufferByteSize = ibByteSize;
    landGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    d3dUtil::SubmeshGeometry landSub;
    landSub.IndexCount = land.GetIndices16().size();
    landSub.StartIndexLocation = 0;
    landSub.BaseVertexLocation = 0;
    landGeo->DrawArgs["land"] = std::move(landSub);

    m_Geometries[landGeo->Name] = std::move(landGeo);
    
    std::vector<Vertex> boxVertex(box.Vertices.size());
    for (UINT i = 0; i < box.Vertices.size(); ++i)
    {
        Vertex v;
        v.Pos = box.Vertices[i].Position;
        v.Normal = box.Vertices[i].Normal;
        v.TexC = box.Vertices[i].TexC;
        boxVertex[i] = std::move(v);
    }
    
    // box geometry
    std::unique_ptr<d3dUtil::MeshGeometry> boxGeom = std::make_unique<d3dUtil::MeshGeometry>();
    vbByteSize = sizeof(Vertex) * boxVertex.size();
    ibByteSize = sizeof(uint16_t) * box.GetIndices16().size();
    ThrowIfFailed(D3DCreateBlob(vbByteSize, boxGeom->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibByteSize, boxGeom->IndexBufferCPU.GetAddressOf()));
    CopyMemory(boxGeom->VertexBufferCPU->GetBufferPointer(), boxVertex.data(), vbByteSize);
    CopyMemory(boxGeom->IndexBufferCPU->GetBufferPointer(), box.GetIndices16().data(), ibByteSize);

    boxGeom->Name = "box";
    boxGeom->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(),
        boxGeom->VertexBufferCPU->GetBufferPointer(), vbByteSize, boxGeom->VertexBufferUploader);
    boxGeom->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(),
        boxGeom->IndexBufferCPU->GetBufferPointer(), ibByteSize, boxGeom->IndexBufferUploader);
    boxGeom->VertexByteStride = sizeof(Vertex);
    boxGeom->VertexByteSize = vbByteSize;
    boxGeom->IndexBufferByteSize = ibByteSize;
    boxGeom->IndexFormat = DXGI_FORMAT_R16_UINT;
    d3dUtil::SubmeshGeometry boxSub;
    boxSub.IndexCount = box.GetIndices16().size();
    boxSub.StartIndexLocation = 0;
    boxSub.BaseVertexLocation = 0;
    boxGeom->DrawArgs["box"] = std::move(boxSub);
    m_Geometries[boxGeom->Name] = std::move(boxGeom);

    // sea Geometry
    m_SeaUploaderBuffer = std::make_unique<UploadBuffer<Vertex>>(m_Device.Get(), land.Vertices.size(), false);
    std::unique_ptr<d3dUtil::MeshGeometry> seaGeo = std::make_unique<d3dUtil::MeshGeometry>();
    vbByteSize = sizeof(Vertex) * land.Vertices.size();
    ibByteSize = sizeof(uint16_t) * land.GetIndices16().size();
    ThrowIfFailed(D3DCreateBlob(vbByteSize, seaGeo->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibByteSize, seaGeo->IndexBufferCPU.GetAddressOf()));
    CopyMemory(seaGeo->VertexBufferCPU->GetBufferPointer(), landVertex.data(), vbByteSize);
    CopyMemory(seaGeo->IndexBufferCPU->GetBufferPointer(), land.GetIndices16().data(), ibByteSize);
    seaGeo->Name = "sea";
    seaGeo->VertexBufferGPU = (ID3D12Resource*)*m_SeaUploaderBuffer;
    seaGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), land.GetIndices16().data(),
        ibByteSize, seaGeo->IndexBufferUploader);
    d3dUtil::SubmeshGeometry seaSub;
    seaSub.IndexCount = land.GetIndices16().size();
    seaSub.StartIndexLocation = 0;
    seaSub.BaseVertexLocation = 0;
    seaGeo->DrawArgs["sea"] = std::move(seaSub);
    m_Geometries[seaGeo->Name] = std::move(seaGeo);
}

void MaterialApp::CreateMaterial()
{
    std::unique_ptr<d3dUtil::Material> grass = std::make_unique<d3dUtil::Material>();
    grass->Name = "land";
    grass->MatCBIndex = 0;
    grass->DiffuseSrvHeapIndex = 0;
    grass->DiffuseAlbedo = DirectX::XMFLOAT4(0.2f, 0.6f, 0.2f, 1.0f);
    grass->FresnelR0 = DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f);
    grass->Roughness = 0.125f;

    //当前这种水的材质定义得并不是很好，但是由于我们还未学会所需的全部渲染工具（如透明度、环境反射等），因此暂时先用这些数据解决
    auto water = std::make_unique<d3dUtil::Material>();
    water->Name = "sea";
    water->MatCBIndex = 1;
    water->DiffuseSrvHeapIndex = 0;
    water->DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.2f, 0.6f, 1.0f);
    water->FresnelR0 = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
    water->Roughness = 0.0f;
    
    auto box = std::make_unique<d3dUtil::Material>();
    box->Name = "box";
    box->MatCBIndex = 2;
    box->DiffuseSrvHeapIndex = 0;
    box->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    box->FresnelR0 = DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f);
    box->Roughness = 0.125f;
    
    m_Materials[grass->Name] = std::move(grass);
    m_Materials[water->Name] = std::move(water);
    m_Materials[box->Name] = std::move(box);
}

void MaterialApp::CreateFrameResources()
{
    for (UINT i = 0; i < gNumFrameResources; i++)
    {
        m_FrameResources.push_back(std::make_unique<FrameResource>(m_Device.Get(), 1, m_RenderItems.size(), m_Materials.size()));
    }
}


void MaterialApp::CreateRenderItems()
{
    std::unique_ptr<RenderItem> landRitem = std::make_unique<RenderItem>();
    landRitem->ObjCBIndex = 0;
    landRitem->Geo = m_Geometries["land"].get();
    landRitem->mat = m_Materials["land"].get();
    landRitem->Geo = m_Geometries["land"].get();
    landRitem->IndexCount = landRitem->Geo->DrawArgs["land"].IndexCount;
    landRitem->StartIndexLocation = landRitem->Geo->DrawArgs["land"].StartIndexLocation;
    landRitem->BaseVertexLocation = landRitem->Geo->DrawArgs["land"].BaseVertexLocation;
    landRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    std::unique_ptr<RenderItem> seaRitem = std::make_unique<RenderItem>();
    seaRitem->ObjCBIndex = 1;
    seaRitem->Geo = m_Geometries["sea"].get();
    seaRitem->mat = m_Materials["sea"].get();
    seaRitem->IndexCount = seaRitem->Geo->DrawArgs["sea"].IndexCount;
    seaRitem->BaseVertexLocation = seaRitem->Geo->DrawArgs["sea"].BaseVertexLocation;
    seaRitem->StartIndexLocation = seaRitem->Geo->DrawArgs["sea"].StartIndexLocation;
    seaRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    std::unique_ptr<RenderItem> boxRitem = std::make_unique<RenderItem>();
    boxRitem->ObjCBIndex = 2;
    boxRitem->Geo = m_Geometries["box"].get();
    boxRitem->mat = m_Materials["box"].get();
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
    boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    
    m_RenderItems.push_back(std::move(landRitem));
    m_RenderItems.push_back(std::move(seaRitem));
    m_RenderItems.push_back(std::move(boxRitem));
}


void MaterialApp::CreateSamplerDescriptor()
{
    D3D12_DESCRIPTOR_HEAP_DESC samplerDesc = {};
    samplerDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    samplerDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    samplerDesc.NodeMask = 0;
    samplerDesc.NumDescriptors = 1;

    ComPtr<ID3D12DescriptorHeap> samplerDescHeap;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(samplerDescHeap.GetAddressOf())));

    D3D12_SAMPLER_DESC sd{};
    sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sd.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D12_FLOAT32_MAX;
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(samplerDescHeap->GetCPUDescriptorHandleForHeapStart());
    m_Device->CreateSampler(&sd, handle);
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> MaterialApp::GetStaticSamplers()
{
    // 应用程序一般只会用到这些采样器中的一部分
    //所以将它们全部提前定义好，并作为根签名的一部分保留下来
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap{
        0,
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP
    };

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp{
        1,
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP
    };
    
    const CD3DX12_STATIC_SAMPLER_DESC linearWrap{
        2,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP
    };

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp{
        3,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP
    };

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap{
        4,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f,
        8
    };

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp{
        5,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP
    };
    return std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>{pointWrap, pointClamp,
    linearWrap, linearClamp,
    anisotropicWrap, anisotropicClamp};
}


void MaterialApp::CreateShaderAndInputLayout()
{
    m_InputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    m_VSShader = d3dUtil::CompileShader(L"source/shader/Texture.usf", nullptr, "VS", "vs_5_0");
    m_PSShader = d3dUtil::CompileShader(L"source/shader/Texture.usf", nullptr, "PS", "ps_5_0");
}

void MaterialApp::createRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[4];
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> staticSamplers = GetStaticSamplers();
    CD3DX12_ROOT_SIGNATURE_DESC rootSignature(4, slotRootParameter, (UINT)staticSamplers.size(),
        staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> error;
    ComPtr<ID3DBlob> sig;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignature, D3D_ROOT_SIGNATURE_VERSION_1_0, sig.GetAddressOf(), error.GetAddressOf());
    if (error != nullptr)
    {
        ::OutputDebugStringA((char*)error->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    ThrowIfFailed(m_Device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(m_RootSignature.GetAddressOf())));
}


void MaterialApp::CreateSRVDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvdesc{};
    srvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvdesc.NumDescriptors = 2;
    srvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvdesc.NodeMask = 0;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&srvdesc, IID_PPV_ARGS(m_SRVDescriptorHeap.GetAddressOf())));
}

void MaterialApp::CreateSRVView()
{
    //1. create vie
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    ID3D12Resource* woodTex = m_Textures["woodCrateTex"]->Resource.Get();
    srvDesc.Format = woodTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = woodTex->GetDesc().MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_SRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    
    m_Device->CreateShaderResourceView(m_Textures["woodCrateTex"]->Resource.Get(),
       &srvDesc, handle);

    ID3D12Resource* defaultTex = m_Textures["default"]->Resource.Get();
    srvDesc.Format = defaultTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = defaultTex->GetDesc().MipLevels;
    handle.Offset(1, m_CbvSrvUavDescriptorSize);
    m_Device->CreateShaderResourceView(m_Textures["default"]->Resource.Get(),
        &srvDesc, handle);
}


void MaterialApp::LoadTextureResources()
{
    std::unique_ptr<Texture> woodCrateTex = std::make_unique<Texture>();
    woodCrateTex->Name = "woodCrateTex";
    woodCrateTex->FileName = L"resources/wood.dds";
    woodCrateTex->AlphaMode = DirectX::DDS_ALPHA_MODE_OPAQUE;
    woodCrateTex->is_cube = false;
    ThrowIfFailed(DirectX::LoadDDSTextureFromFile(m_Device.Get(),
        woodCrateTex->FileName.c_str(),
        woodCrateTex->Resource.GetAddressOf(),
        woodCrateTex->Data,
        woodCrateTex->SubResourceData,
        0,
        &woodCrateTex->AlphaMode,
        &woodCrateTex->is_cube
        ));
    CD3DX12_RESOURCE_BARRIER pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(woodCrateTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    
    // subresource has data
    CD3DX12_HEAP_PROPERTIES pP(D3D12_HEAP_TYPE_UPLOAD);
    UINT byteSize = GetRequiredIntermediateSize(woodCrateTex->Resource.Get(), 0,
        woodCrateTex->SubResourceData.size());
    CD3DX12_RESOURCE_DESC pD = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(&pP,
        D3D12_HEAP_FLAG_NONE,
        &pD, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(woodCrateTex->UploadHeap.GetAddressOf())
        ));

    UpdateSubresources(m_CommandList.Get(),
        woodCrateTex->Resource.Get(),
        woodCrateTex->UploadHeap.Get(),
        0, 0, woodCrateTex->SubResourceData.size(),
        woodCrateTex->SubResourceData.data());

    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(woodCrateTex->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    
    m_CommandList->ResourceBarrier(1, &pBarrier);
    m_Textures[woodCrateTex->Name] = std::move(woodCrateTex);
}

void MaterialApp::LoadDefaultWhiteTexture()
{
    std::unique_ptr<Texture> defaultWhiteTex = std::make_unique<Texture>();
    defaultWhiteTex->Name = "default";
    defaultWhiteTex->FileName = L"";
    defaultWhiteTex->AlphaMode = DirectX::DDS_ALPHA_MODE_OPAQUE;
    defaultWhiteTex->is_cube = false;

    const UINT width = 1;
    const UINT height = 1;
    const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    const UINT mipLevels = 1;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        format,
        width,
        height,
        1,
        mipLevels,
        1,
        0,
        D3D12_RESOURCE_FLAG_NONE);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultWhiteTex->Resource.GetAddressOf())));
    
    const UINT pixelSize = 4; //R8G8B8A8 4个字节
    const UINT rowPitch = width * pixelSize;
    const UINT slicePitch = rowPitch * height;
    defaultWhiteTex->Data = std::make_unique<uint8_t[]>(slicePitch); //数组的首地址
    memset(defaultWhiteTex->Data.get(), 0xFF, slicePitch);//数组的首地址开始全部赋值为1
    D3D12_SUBRESOURCE_DATA subResourceData{};
    subResourceData.pData = defaultWhiteTex->Data.get();
    subResourceData.RowPitch = rowPitch;
    subResourceData.SlicePitch = slicePitch;
    defaultWhiteTex->SubResourceData.push_back(subResourceData);

    CD3DX12_HEAP_PROPERTIES uploaderHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    UINT byteSize = GetRequiredIntermediateSize(defaultWhiteTex->Resource.Get(), 0, defaultWhiteTex->SubResourceData.size());

    CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    m_Device->CreateCommittedResource(&uploaderHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(defaultWhiteTex->UploadHeap.GetAddressOf()));
    CD3DX12_RESOURCE_BARRIER pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        defaultWhiteTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    
    m_CommandList->ResourceBarrier(1, &pBarrier);
    UpdateSubresources(m_CommandList.Get(), defaultWhiteTex->Resource.Get(),
        defaultWhiteTex->UploadHeap.Get(),
    0, 0,
        defaultWhiteTex->SubResourceData.size(),
        defaultWhiteTex->SubResourceData.data());
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultWhiteTex->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    m_Textures[defaultWhiteTex->Name] = std::move(defaultWhiteTex);
}

void MaterialApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    for (size_t i = 0; i < ritems.size(); i++)
    {
        auto ri = ritems[i];
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView = ri->Geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW indexBufferView = ri->Geo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
        cmdList->IASetIndexBuffer(&indexBufferView);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(m_SRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(ri->mat->DiffuseSrvHeapIndex, m_CbvSrvUavDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(0, tex);
        ID3D12Resource* objResource = (ID3D12Resource*)*m_CurrentFrameResource->ObjectCB.get();
        ID3D12Resource* matResource = (ID3D12Resource*)*m_CurrentFrameResource->MaterialCB.get();
        D3D12_GPU_VIRTUAL_ADDRESS objGPU = objResource->GetGPUVirtualAddress() + ri->ObjCBIndex * d3dUtil::CalcConstBufferByteSize(sizeof(ObjectConsts));
        D3D12_GPU_VIRTUAL_ADDRESS matGPU = matResource->GetGPUVirtualAddress() + ri->mat->MatCBIndex * d3dUtil::CalcConstBufferByteSize(sizeof(MaterialConstants));
        cmdList->SetGraphicsRootConstantBufferView(1, objGPU);
        cmdList->SetGraphicsRootConstantBufferView(2, matGPU);
        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void MaterialApp::CreatePSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = m_RootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSShader.Get());
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = {m_InputLayout.data(), (UINT)m_InputLayout.size()};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_BackBufferFormat;
    psoDesc.DSVFormat = m_DepthStencilFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? m4xMsaaQuality - 1 : 0;
    psoDesc.NodeMask = 0;
    ComPtr<ID3D12PipelineState> pso;
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pso.GetAddressOf())));
    m_PSOs["material"] = std::move(pso);
}

void MaterialApp::Update(const GameTimer& gt)
{
    m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % gNumFrameResources;
    m_CurrentFrameResource = m_FrameResources[m_CurrentFrameIndex].get();
    if (m_CurrentFrameResource->Fence != 0 && m_Fence->GetCompletedValue() < m_CurrentFrameResource->Fence)
    {
        HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        m_Fence->SetEventOnCompletion(m_CurrentFrameResource->Fence, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }
    float x = m_Radius * sinf(m_Phi) * cosf(m_Theta);
    float y = m_Radius * cosf(m_Phi);
    float z = m_Radius * sinf(m_Phi) * sinf(m_Theta);
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
    DirectX::XMVECTOR target = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    DirectX::XMStoreFloat4x4(&m_View, view);
    DirectX::XMStoreFloat3(&m_Eyepos, pos);

    UpdateFramresouce(gt);
    UpdateObj(gt);
    UpdateMaterial(gt);
    UpdateSea(gt);
}

void MaterialApp::UpdateObj(const GameTimer& gt)
{
    for (auto& r : m_RenderItems)
    {
        if (r->NumFrameDirty > 0)
        {
            r->NumFrameDirty--;
            ObjectConsts objConstData;
            objConstData.World = r->World;
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&r->World);
            DirectX::XMMATRIX invtworld = MathHelper::InverseTanspose(world);
            DirectX::XMFLOAT4X4 w;
            DirectX::XMStoreFloat4x4(&w, invtworld);
            objConstData.TInvWorld = w;
            
            DirectX::XMStoreFloat4x4(&w, invtworld);
            m_CurrentFrameResource->ObjectCB->CopyData(r->ObjCBIndex, objConstData);
        }
    }
}

void MaterialApp::UpdateFramresouce(const GameTimer& gt)
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

void MaterialApp::UpdateMaterial(const GameTimer& gt)
{
    for (auto& mat : m_Materials)
    {
        d3dUtil::Material* pMat = mat.second.get();
        if (pMat->NumFrameDirty > 0)
        {
            pMat->NumFrameDirty --;
            MaterialConstants matC;
            matC.DiffuseAlbedo = pMat->DiffuseAlbedo;
            matC.Roughness = pMat->Roughness;
            matC.FresnelR0 = pMat->FresnelR0;
            matC.MatTransform = pMat->MatTransform;
            m_CurrentFrameResource->MaterialCB->CopyData(pMat->MatCBIndex, matC);
        }
    }
}

DirectX::XMFLOAT3 MaterialApp::calculateNormal(float amplitude, float waveSpeed, float speed, float time, DirectX::XMFLOAT2 direction, DirectX::XMFLOAT2 pos_xy)
{
    DirectX::XMFLOAT3 normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
    float w = 2.0f / waveSpeed;
    float phi = speed * w;
    float k = cosf((direction.x * pos_xy.x + direction.y * pos_xy.y) * w + time * phi);
        
    float x = -1.0f * amplitude * w * direction.x * k;
    float z = -1.0f * amplitude * w * direction.y * k;
    return {x, 1.0f, z};
}

float MaterialApp::CalculateWaveHeight(float amplitude, float waveSpeed, float speed, float time, DirectX::XMFLOAT2 direction, DirectX::XMFLOAT2 pos_xy)
{
    float w = 2.0f / waveSpeed;
    float phi = speed * w;
    float dDotPos = direction.x * pos_xy.x + direction.y * pos_xy.y;
    float y = amplitude * sinf(w * dDotPos + time * phi);
    return y;
}



void MaterialApp::UpdateSea(const GameTimer& gt)
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
        m_SeaUploaderBuffer->CopyData(vIdx, v);
        ++vIdx;
    }
}


void MaterialApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    m_MousePos = {x, y};
    SetCapture(mhMainWnd);
}
void MaterialApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if (btnState & MK_LBUTTON)
    {
        float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - m_MousePos.x));
        float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - m_MousePos.y));

        m_Phi += dy;
        m_Theta += dx;
        m_Phi = MathHelper::Clamp(m_Phi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if (btnState & MK_RBUTTON)
    {
        float dx = 0.005f * static_cast<float>(x - m_MousePos.x);
        float dy = 0.005f * static_cast<float>(y - m_MousePos.y);
        m_Radius += (dx - dy);
        m_Radius = MathHelper::Clamp(m_Radius, 3.0f, 100.0f);
    }
    m_MousePos = {x, y};
}
void MaterialApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

 


void MaterialApp::Draw(const GameTimer& gt)
{
    ID3D12CommandAllocator* alloc = m_CurrentFrameResource->CmdListAlloc.Get();
    ThrowIfFailed(alloc->Reset());
    ID3D12PipelineState* pso = m_PSOs["material"].Get();
    ThrowIfFailed(m_CommandList->Reset(alloc, pso));

    ID3D12Resource* CurrentFrame = CurrentBackBuffer();
    D3D12_CPU_DESCRIPTOR_HANDLE FrameHandle = CurrentBackBufferHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE DepthHandle = DepthStencilView();
    CD3DX12_RESOURCE_BARRIER pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentFrame, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    m_CommandList->OMSetRenderTargets(1, &FrameHandle, true, &DepthHandle);
    m_CommandList->RSSetViewports(1, &m_ViewPort);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
    
    m_CommandList->ClearRenderTargetView(FrameHandle, DirectX::Colors::LightSteelBlue, 0, nullptr);
    m_CommandList->ClearDepthStencilView(DepthHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    ID3D12DescriptorHeap* heaps[] = { m_SRVDescriptorHeap.Get() };
    m_CommandList->SetDescriptorHeaps(1, heaps);
    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
    ID3D12Resource* FrameConstResource = (ID3D12Resource*)*m_CurrentFrameResource->PassCB.get();
    D3D12_GPU_VIRTUAL_ADDRESS pFrameResource = FrameConstResource->GetGPUVirtualAddress();
    m_CommandList->SetGraphicsRootConstantBufferView(3, pFrameResource);
    m_Opaques.clear();
    for (int i = 0; i < m_RenderItems.size(); ++i)
    {
        m_Opaques.push_back(m_RenderItems[i].get());
    }
    DrawRenderItems(m_CommandList.Get(), m_Opaques);

    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentFrame, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    ThrowIfFailed(m_CommandList->Close());

    ID3D12CommandList* cmdLists[] = {m_CommandList.Get()};
    m_CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
    ThrowIfFailed(m_SwapChain->Present(1, 0));
    m_CurrentBackBufferIndex = (m_CurrentBackBufferIndex + 1) % SwapChainBufferCount;
    m_CurrentFrameResource->Fence = ++m_CurrentFence;
    m_CommandQueue->Signal(m_Fence.Get(), m_CurrentFence);
}
