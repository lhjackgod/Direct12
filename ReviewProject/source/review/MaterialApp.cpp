#include "D3DUtil.h"
#include "MaterialApp.h"
#include "DDSTexttureLoader.h"
#include <array>
#include "core/GeometryGenerator.h"
#include "core/DDSTexttureLoader.h"
bool MaterialApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;
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
    std::unique_ptr<d3dUtil::MeshGeometry> landGeo;
    UINT vbByteSize = sizeof(Vertex) * landVertex.size();
    UINT ibByteSize = sizeof(uint16_t) * land.GetIndices16().size();
    ThrowIfFailed(D3DCreateBlob(vbByteSize, landGeo->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibByteSize, landGeo->IndexBufferCPU.GetAddressOf));
    CopyMemory(landGeo->VertexBufferCPU->GetBufferPointer(), landVertex.data(), vbByteSize);
    CopyMemory(landGeo->IndexBufferCPU->GetBufferPointer(), land.GetIndices16().data(), ibByteSize);
    landGeo->Name = "land";
    landGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), landGeo->VertexBufferCPU->GetBufferPointer(),
        vbByteSize, landGeo->VertexBufferUploader);
    landGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), landGeo->IndexBufferCPU->GetBufferPointer(),
        ibByteSize, landGeo->IndexBufferUploader);
    landGeo->DisposeUploaders();
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
    std::unique_ptr<d3dUtil::MeshGeometry> boxGeom;
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
    boxGeom->DisposeUploaders();
    d3dUtil::SubmeshGeometry boxSub;
    boxSub.IndexCount = box.GetIndices16().size();
    boxSub.StartIndexLocation = 0;
    boxSub.BaseVertexLocation = 0;
    boxGeom->DrawArgs["box"] = std::move(boxSub);
    m_Geometries[boxGeom->Name] = std::move(boxGeom);

    // sea Geometry
    m_SeaUploaderBuffer = std::make_unique<UploadBuffer<Vertex>>(m_Device.Get(), land.Vertices.size(), false);
    std::unique_ptr<d3dUtil::MeshGeometry> seaGeo;
    vbByteSize = sizeof(Vertex) * land.Vertices.size();
    ibByteSize = sizeof(uint16_t) * land.GetIndices16().size();
    ThrowIfFailed(D3DCreateBlob(vbByteSize, seaGeo->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibByteSize, seaGeo->IndexBufferCPU.GetAddressOf()));
    CopyMemory(seaGeo->VertexBufferCPU->GetBufferPointer(), landVertex.data(), vbByteSize);
    CopyMemory(seaGeo->IndexBufferCPU->GetBufferPointer(), land.GetIndices16().data(), ibByteSize);
    seaGeo->Name = "sea";
    seaGeo->VertexBufferGPU = (ID3D12Resource*)*m_SeaUploaderBuffer;
    seaGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(), land.GetIndices16().data(),
        ibByteSize, landGeo->IndexBufferUploader);
    seaGeo->DisposeUploaders();
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
    grass->DiffuseSrvHeapIndex = 1;
    grass->DiffuseAlbedo = DirectX::XMFLOAT4(0.2f, 0.6f, 0.2f, 1.0f);
    grass->FresnelR0 = DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f);
    grass->Roughness = 0.125f;

    //当前这种水的材质定义得并不是很好，但是由于我们还未学会所需的全部渲染工具（如透明度、环境反射等），因此暂时先用这些数据解决
    auto water = std::make_unique<d3dUtil::Material>();
    water->Name = "sea";
    water->MatCBIndex = 1;
    water->DiffuseSrvHeapIndex = 1;
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
        D3D12_RESOURCE_STATE_COMMON);
    
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
        D3D12_RESOURCE_STATE_COMMON);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    m_Textures[defaultWhiteTex->Name] = std::move(defaultWhiteTex);
}

void MaterialApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    for (size_t i = 0; i < ritems.size(); i++)
    {
        auto ri = ritems[i];
        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(m_SRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(ri->mat->DiffuseSrvHeapIndex, m_CbvSrvUavDescriptorSize);
        
        cmdList->SetGraphicsRootDescriptorTable(0, tex);
    }
}
