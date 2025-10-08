#include "D3DUtil.h"
#include "MaterialApp.h"
#include "DDSTexttureLoader.h"
#include <array>
#include "core/GeometryGenerator.h"
#include "core/DDSTexttureLoader.h"
#include "DirectXColors.h"
#include <fstream>
MaterialApp::MaterialApp(HINSTANCE hInstace)
    :D3DApp(hInstace)
{

}
bool MaterialApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;
    m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
    LoadShull();
    LoadRoom();
    LoadTextureResources();
    CreateSRVDescriptorHeap();
    CreateSRVView();
    CreateMaterial();
    CreateShullBoxRenderItems();
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

void MaterialApp::LoadShull()
{
    std::ifstream fin("modules/skull.txt");
    if (!fin.is_open())
    {
        MessageBox(mhMainWnd, L"Models/skull.txt not found.", L"Not Found", MB_OK);
    }
    std::string ignore;
    UINT vcount = 0;
    UINT tcount = 0;
    fin >> ignore >> vcount;
    fin >> ignore >> tcount;
    fin >> ignore >> ignore >> ignore >> ignore;
    std::vector<Vertex> vertices(vcount);
    for (UINT i = 0; i < vcount; ++i)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
        vertices[i].TexC = {0.0f, 0.0f};
    }
    fin >> ignore >> ignore >> ignore;
    std::vector<uint32_t> indices(tcount * 3);
    for (UINT i = 0; i < tcount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }
    fin >> ignore;
    fin.close();
    std::unique_ptr<d3dUtil::MeshGeometry> ShullGeom = std::make_unique<d3dUtil::MeshGeometry>();
    ShullGeom->Name = "Shull";
    UINT vbBytes = sizeof(Vertex) * vertices.size();
    UINT ibBytes = sizeof(uint32_t) * indices.size();
    
    ThrowIfFailed(D3DCreateBlob(vbBytes, ShullGeom->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibBytes, ShullGeom->IndexBufferCPU.GetAddressOf()));
    CopyMemory(ShullGeom->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbBytes);
    CopyMemory(ShullGeom->IndexBufferCPU->GetBufferPointer(), indices.data(), ibBytes);
    ShullGeom->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(),
    ShullGeom->VertexBufferCPU->GetBufferPointer(), vbBytes, ShullGeom->VertexBufferUploader);
    ShullGeom->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(),
        ShullGeom->IndexBufferCPU->GetBufferPointer(), ibBytes, ShullGeom->IndexBufferUploader);
    ShullGeom->VertexByteStride = sizeof(Vertex);
    ShullGeom->VertexByteSize = vbBytes;
    ShullGeom->IndexBufferByteSize = ibBytes;
    ShullGeom->IndexFormat = DXGI_FORMAT_R32_UINT;

    d3dUtil::SubmeshGeometry shullSub;
    shullSub.IndexCount = indices.size();
    shullSub.BaseVertexLocation = 0;
    shullSub.StartIndexLocation = 0;
    ShullGeom->DrawArgs["shull"] = std::move(shullSub);
    m_Geometries[ShullGeom->Name] = std::move(ShullGeom);
}

void MaterialApp::LoadRoom()
{
    // Create and specify geometry.  For this sample we draw a floor
    // and a wall with a mirror on it.  We put the floor, wall, and
    // mirror geometry in one vertex buffer.
    //
    //   |--------------|
    //   |              |
    //   |----|----|----|
    //   |Wall|Mirr|Wall|
    //   |    | or |    |
    //   /--------------/
    //  /   Floor      /
    // /--------------/
    std::array<Vertex, 20> vertices =
    {
        // Floor: Observe we tile texture coordinates.
        Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
        Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
        Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
        Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

        // Wall: Observe we tile texture coordinates, and that we
        // leave a gap in the middle for the mirror.
        Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
        Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
        Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

        Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
        Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
        Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

        Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
        Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
        Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

        // Mirror
        Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
        Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
        Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
    };
    std::array<std::int16_t, 30> indices =
    {
        // Floor
        0, 1, 2,
        0, 2, 3,

        // Walls
        4, 5, 6,
        4, 6, 7,

        8, 9, 10,
        8, 10, 11,

        12, 13, 14,
        12, 14, 15,

        // Mirror
        16, 17, 18,
        16, 18, 19
    };
    std::unique_ptr<d3dUtil::MeshGeometry> roomGeo = std::make_unique<d3dUtil::MeshGeometry>();
    roomGeo->Name = "room";
    UINT vbBytes = sizeof(Vertex) * vertices.size();
    UINT ibBytes = sizeof(uint16_t) * indices.size();
    ThrowIfFailed(D3DCreateBlob(vbBytes, roomGeo->VertexBufferCPU.GetAddressOf()));
    ThrowIfFailed(D3DCreateBlob(ibBytes, roomGeo->IndexBufferCPU.GetAddressOf()));
    CopyMemory(roomGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbBytes);
    CopyMemory(roomGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibBytes);
    roomGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(),
        m_CommandList.Get(), roomGeo->VertexBufferCPU->GetBufferPointer(),
        vbBytes, roomGeo->VertexBufferUploader);
    roomGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(),
        m_CommandList.Get(), roomGeo->IndexBufferCPU->GetBufferPointer(),
        ibBytes, roomGeo->IndexBufferUploader);
    roomGeo->VertexByteStride = sizeof(Vertex);
    roomGeo->VertexByteSize = vbBytes;
    roomGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    roomGeo->IndexBufferByteSize = ibBytes;

    d3dUtil::SubmeshGeometry wall;
    wall.IndexCount = 18;
    wall.StartIndexLocation = 6;
    wall.BaseVertexLocation = 0;

    d3dUtil::SubmeshGeometry floor;
    floor.IndexCount = 6;
    floor.StartIndexLocation = 0;
    floor.BaseVertexLocation = 0;

    d3dUtil::SubmeshGeometry mirror;
    mirror.IndexCount = 6;
    mirror.StartIndexLocation = 24;
    mirror.BaseVertexLocation = 0;

    roomGeo->DrawArgs["wall"] = wall;
    roomGeo->DrawArgs["floor"] = floor;
    roomGeo->DrawArgs["mirror"] = mirror;
    m_Geometries[roomGeo->Name] = std::move(roomGeo);
}

void MaterialApp::CreateMaterial()
{
    std::unique_ptr<d3dUtil::Material> bricks = std::make_unique<d3dUtil::Material>();
    bricks->Name = "bricks";
    bricks->MatCBIndex = 0;
    bricks->DiffuseSrvHeapIndex = 0;
    bricks->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bricks->FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
    bricks->Roughness = 0.25f;

    std::unique_ptr<d3dUtil::Material> checkertile = std::make_unique<d3dUtil::Material>();
    checkertile->Name = "checkertile";
    checkertile->MatCBIndex = 1;
    checkertile->DiffuseSrvHeapIndex = 1;
    checkertile->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    checkertile->FresnelR0 = DirectX::XMFLOAT3(0.07f, 0.07f, 0.07f);
    checkertile->Roughness = 0.3f;

    std::unique_ptr<d3dUtil::Material> icemirror = std::make_unique<d3dUtil::Material>();
    icemirror->Name = "icemirror";
    icemirror->MatCBIndex = 2;
    icemirror->DiffuseSrvHeapIndex = 2;
    icemirror->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
    icemirror->FresnelR0 = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
    icemirror->Roughness = 0.5f;

    std::unique_ptr<d3dUtil::Material> skullMat = std::make_unique<d3dUtil::Material>();
    skullMat->Name = "skull";
    skullMat->MatCBIndex = 3;
    skullMat->DiffuseSrvHeapIndex = 3;
    skullMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    skullMat->FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
    skullMat->Roughness = 0.3f;

    std::unique_ptr<d3dUtil::Material> defaultMat = std::make_unique<d3dUtil::Material>();
    defaultMat->Name = "default";
    defaultMat->MatCBIndex = 4;
    defaultMat->DiffuseSrvHeapIndex = 3;
    defaultMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
    defaultMat->FresnelR0 = DirectX::XMFLOAT3(0.001f, 0.001f, 0.001f);
    defaultMat->Roughness = 0.0f;

    m_Materials[bricks->Name] = std::move(bricks);
    m_Materials[checkertile->Name] = std::move(checkertile);
    m_Materials[icemirror->Name] = std::move(icemirror);
    m_Materials[skullMat->Name] = std::move(skullMat);
    m_Materials[defaultMat->Name] = std::move(defaultMat);
}

void MaterialApp::CreateFrameResources()
{
    for (UINT i = 0; i < gNumFrameResources; i++)
    {
        m_FrameResources.push_back(std::make_unique<FrameResource>(m_Device.Get(), 2, m_RenderItems.size(), m_Materials.size()));
    }
}

void MaterialApp::CreateShullBoxRenderItems()
{
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    std::unique_ptr<RenderItem> skullRit = std::make_unique<RenderItem>();
    skullRit->ObjCBIndex = 0;
    DirectX::XMMATRIX SkullWorld = DirectX::XMLoadFloat4x4(&skullRit->World);
    SkullWorld = DirectX::XMMatrixMultiply(SkullWorld, DirectX::XMMatrixRotationAxis(up, DirectX::XMConvertToRadians(90.0f)));
    SkullWorld = DirectX::XMMatrixMultiply(SkullWorld, DirectX::XMMatrixScaling(0.5f, 0.5f, 0.5f));
    SkullWorld = DirectX::XMMatrixMultiply(SkullWorld, DirectX::XMMatrixTranslation(-3.0f, 1.0f, -5.6f));
    DirectX::XMStoreFloat4x4(&skullRit->World, SkullWorld);
    skullRit->Geo = m_Geometries["Shull"].get();
    skullRit->mat = m_Materials["skull"].get();
    skullRit->IndexCount = skullRit->Geo->DrawArgs["shull"].IndexCount;
    skullRit->StartIndexLocation = skullRit->Geo->DrawArgs["shull"].StartIndexLocation;
    skullRit->BaseVertexLocation = skullRit->Geo->DrawArgs["shull"].BaseVertexLocation;
    skullRit->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_PassRenderItems[(int)RenderLayer::Opaque].push_back(skullRit.get());
    m_RenderItems.push_back(std::move(skullRit));
    
    
    std::unique_ptr<RenderItem> wallRit = std::make_unique<RenderItem>();
    wallRit->ObjCBIndex = 1;
    wallRit->Geo = m_Geometries["room"].get();
    wallRit->mat = m_Materials["bricks"].get();
    wallRit->IndexCount = wallRit->Geo->DrawArgs["wall"].IndexCount;
    wallRit->StartIndexLocation = wallRit->Geo->DrawArgs["wall"].StartIndexLocation;
    wallRit->BaseVertexLocation = wallRit->Geo->DrawArgs["wall"].BaseVertexLocation;
    wallRit->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_PassRenderItems[(int)RenderLayer::Opaque].push_back(wallRit.get());
    m_RenderItems.push_back(std::move(wallRit));

    
    std::unique_ptr<RenderItem> floorRit = std::make_unique<RenderItem>();
    floorRit->ObjCBIndex = 2;
    floorRit->Geo = m_Geometries["room"].get();
    floorRit->mat = m_Materials["checkertile"].get();
    floorRit->IndexCount = floorRit->Geo->DrawArgs["floor"].IndexCount;
    floorRit->StartIndexLocation = floorRit->Geo->DrawArgs["floor"].StartIndexLocation;
    floorRit->BaseVertexLocation = floorRit->Geo->DrawArgs["floor"].BaseVertexLocation;
    floorRit->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_PassRenderItems[(int)RenderLayer::Opaque].push_back(floorRit.get());
    m_RenderItems.push_back(std::move(floorRit));
    
    //Mirror pass
    std::unique_ptr<RenderItem> mirrorRit = std::make_unique<RenderItem>();
    mirrorRit->ObjCBIndex = 3;
    mirrorRit->Geo = m_Geometries["room"].get();
    mirrorRit->mat = m_Materials["icemirror"].get();
    mirrorRit->IndexCount = mirrorRit->Geo->DrawArgs["mirror"].IndexCount;
    mirrorRit->StartIndexLocation = mirrorRit->Geo->DrawArgs["mirror"].StartIndexLocation;
    mirrorRit->BaseVertexLocation = mirrorRit->Geo->DrawArgs["mirror"].BaseVertexLocation;
    mirrorRit->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_PassRenderItems[(int)RenderLayer::Mirrors].push_back(mirrorRit.get());
    m_RenderItems.push_back(std::move(mirrorRit));

    //ReflectionPass
    DirectX::XMVECTOR ReflectFaceNormal = DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
    DirectX::XMMATRIX ReflectionMatrix = DirectX::XMMatrixReflect(ReflectFaceNormal);
    std::unique_ptr<RenderItem> skullReflectRit = std::make_unique<RenderItem>();
    skullReflectRit->ObjCBIndex = 4;
    DirectX::XMMATRIX SkullReflectWorld = DirectX::XMLoadFloat4x4(&skullReflectRit->World);
    SkullReflectWorld = DirectX::XMMatrixMultiply(SkullWorld, DirectX::XMMatrixTranslation(0.0f, 0.0f, 11.2f));
    //SkullReflectWorld = SkullWorld * ReflectionMatrix;
    DirectX::XMStoreFloat4x4(&skullReflectRit->World, SkullReflectWorld);
    skullReflectRit->Geo = m_Geometries["Shull"].get();
    skullReflectRit->mat = m_Materials["skull"].get();
    skullReflectRit->IndexCount = skullReflectRit->Geo->DrawArgs["shull"].IndexCount;
    skullReflectRit->StartIndexLocation = skullReflectRit->Geo->DrawArgs["shull"].StartIndexLocation;
    skullReflectRit->BaseVertexLocation = skullReflectRit->Geo->DrawArgs["shull"].BaseVertexLocation;
    skullReflectRit->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_PassRenderItems[(int)RenderLayer::Reflected].push_back(skullReflectRit.get());
    m_RenderItems.push_back(std::move(skullReflectRit));

    // transparent
    std::unique_ptr<RenderItem> transparentMirror = std::make_unique<RenderItem>();
    transparentMirror->ObjCBIndex = 5;
    transparentMirror->Geo = m_Geometries["room"].get();
    transparentMirror->mat = m_Materials["icemirror"].get();
    transparentMirror->IndexCount = transparentMirror->Geo->DrawArgs["mirror"].IndexCount;
    transparentMirror->BaseVertexLocation = transparentMirror->Geo->DrawArgs["mirror"].BaseVertexLocation;
    transparentMirror->StartIndexLocation = transparentMirror->Geo->DrawArgs["mirror"].StartIndexLocation;
    transparentMirror->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_PassRenderItems[(int)RenderLayer::Transparent].push_back(transparentMirror.get());
    m_RenderItems.push_back(std::move(transparentMirror));

    // shadow
                                                        DirectX::XMVECTOR Direction = DirectX::XMVectorSet(0.57735f, -0.57735f, 0.57735f, 0.0f);
                                                        Direction = DirectX::XMVectorMultiply(Direction, DirectX::XMVectorSet(-1.0f, -1.0f, -1.0f, 0.0f));
                                                        DirectX::XMVECTOR shadowPlane = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
                                                        DirectX::XMMATRIX shadowMatrix = DirectX::XMMatrixShadow(shadowPlane, Direction);
                                                    
                                                        DirectX::XMMATRIX shadowWorld = SkullWorld * shadowMatrix;
                                                        shadowWorld = shadowWorld * DirectX::XMMatrixTranslation(0.0f, 0.001f, 0.0f);
                                                        
                                                        std::unique_ptr<RenderItem> shadowSkull = std::make_unique<RenderItem>();
                                                        shadowSkull->ObjCBIndex = 6;
                                                        DirectX::XMStoreFloat4x4(&shadowSkull->World, shadowWorld);
                                                        shadowSkull->Geo = m_Geometries["Shull"].get();
                                                        shadowSkull->mat = m_Materials["default"].get();
                                                        shadowSkull->IndexCount = shadowSkull->Geo->DrawArgs["shull"].IndexCount;
                                                        shadowSkull->StartIndexLocation = shadowSkull->Geo->DrawArgs["shull"].StartIndexLocation;
                                                        shadowSkull->BaseVertexLocation = shadowSkull->Geo->DrawArgs["shull"].BaseVertexLocation;
                                                        shadowSkull->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                                                        m_PassRenderItems[(int)RenderLayer::Shadow].push_back(shadowSkull.get());
                                                        m_RenderItems.push_back(std::move(shadowSkull));
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
    const D3D_SHADER_MACRO defines[] = {
        {"FOG", "1"},
        {"ALPHA_TEST", "1"},
        {"NUM_DIR_LIGHTS", "3"},
        {nullptr, nullptr}
    };
    m_VSShader = d3dUtil::CompileShader(L"source/shader/Texture.usf", nullptr, "VS", "vs_5_0");
    m_PSShader = d3dUtil::CompileShader(L"source/shader/Texture.usf", defines, "PS", "ps_5_0");
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
    srvdesc.NumDescriptors = 4;
    srvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvdesc.NodeMask = 0;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&srvdesc, IID_PPV_ARGS(m_SRVDescriptorHeap.GetAddressOf())));
}

void MaterialApp::CreateSRVView()
{
    // create brick view 
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_SRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    ID3D12Resource* brickResource = m_Textures["bricksTex"]->Resource.Get();
    D3D12_SHADER_RESOURCE_VIEW_DESC pShaderResourceViewDesc{};
    pShaderResourceViewDesc.Format = brickResource->GetDesc().Format;
    pShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    pShaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    pShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
    pShaderResourceViewDesc.Texture2D.MipLevels = brickResource->GetDesc().MipLevels;
    pShaderResourceViewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    m_Device->CreateShaderResourceView(brickResource, &pShaderResourceViewDesc, handle);

    //create checkboard
    ID3D12Resource* checkboardResource = m_Textures["checkboardTex"]->Resource.Get();
    pShaderResourceViewDesc.Format = checkboardResource->GetDesc().Format;
    pShaderResourceViewDesc.Texture2D.MipLevels = checkboardResource->GetDesc().MipLevels;
    handle.Offset(1, m_CbvSrvUavDescriptorSize);
    m_Device->CreateShaderResourceView(checkboardResource, &pShaderResourceViewDesc, handle);

    //create ice
    ID3D12Resource* iceResource = m_Textures["iceTex"]->Resource.Get();
    pShaderResourceViewDesc.Format = iceResource->GetDesc().Format;
    pShaderResourceViewDesc.Texture2D.MipLevels = iceResource->GetDesc().MipLevels;
    handle.Offset(1, m_CbvSrvUavDescriptorSize);
    m_Device->CreateShaderResourceView(iceResource, &pShaderResourceViewDesc, handle);

    // create default
    ID3D12Resource* defaultResource = m_Textures["defaultWhiteTex"]->Resource.Get();
    pShaderResourceViewDesc.Format = defaultResource->GetDesc().Format;
    pShaderResourceViewDesc.Texture2D.MipLevels = defaultResource->GetDesc().MipLevels;
    handle.Offset(1, m_CbvSrvUavDescriptorSize);
    m_Device->CreateShaderResourceView(defaultResource, &pShaderResourceViewDesc, handle);
}


void MaterialApp::LoadTextureResources()
{
    CD3DX12_RESOURCE_BARRIER pBarrier;
    std::unique_ptr<Texture> bricksTex = std::make_unique<Texture>();
    bricksTex->Name = "bricksTex";
    bricksTex->FileName = L"Textures/bricks.dds";
    bricksTex->AlphaMode = DirectX::DDS_ALPHA_MODE_OPAQUE;
    bricksTex->is_cube = false;
    ThrowIfFailed(DirectX::LoadDDSTextureFromFile(
        m_Device.Get(),
        bricksTex->FileName.c_str(),
        bricksTex->Resource.GetAddressOf(),
        bricksTex->Data,
        bricksTex->SubResourceData,
        0, &bricksTex->AlphaMode,
        &bricksTex->is_cube
    ));
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(bricksTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    CD3DX12_HEAP_PROPERTIES brickP(D3D12_HEAP_TYPE_UPLOAD);
    UINT64 brickDescSize = GetRequiredIntermediateSize(bricksTex->Resource.Get(),
        0, bricksTex->SubResourceData.size());
    CD3DX12_RESOURCE_DESC brickDesc = CD3DX12_RESOURCE_DESC::Buffer(brickDescSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &brickP,
        D3D12_HEAP_FLAG_NONE,
        &brickDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(bricksTex->UploadHeap.GetAddressOf())
    ));
    UpdateSubresources(m_CommandList.Get(),
        bricksTex->Resource.Get(), bricksTex->UploadHeap.Get(),
        0, 0, bricksTex->SubResourceData.size(), bricksTex->SubResourceData.data());
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        bricksTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COMMON
    );
    m_CommandList->ResourceBarrier(1, &pBarrier);
    m_Textures[bricksTex->Name] = std::move(bricksTex);
    
    std::unique_ptr<Texture> checkboardTex = std::make_unique<Texture>();
    checkboardTex->Name = "checkboardTex";
    checkboardTex->FileName = L"Textures/checkboard.dds";
    checkboardTex->AlphaMode = DirectX::DDS_ALPHA_MODE_OPAQUE;
    checkboardTex->is_cube = false;
    ThrowIfFailed(DirectX::LoadDDSTextureFromFile(
        m_Device.Get(),
        checkboardTex->FileName.c_str(),
        checkboardTex->Resource.GetAddressOf(),
        checkboardTex->Data,
        checkboardTex->SubResourceData,
        0, &checkboardTex->AlphaMode,
        &checkboardTex->is_cube
    ));
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(checkboardTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    CD3DX12_HEAP_PROPERTIES checkboardP(D3D12_HEAP_TYPE_UPLOAD);
    UINT64 checkboardDescSize = GetRequiredIntermediateSize(checkboardTex->Resource.Get(),
        0, checkboardTex->SubResourceData.size());
    CD3DX12_RESOURCE_DESC checkboardDesc = CD3DX12_RESOURCE_DESC::Buffer(checkboardDescSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &checkboardP,
        D3D12_HEAP_FLAG_NONE,
        &checkboardDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(checkboardTex->UploadHeap.GetAddressOf())
    ));
    UpdateSubresources(m_CommandList.Get(),
        checkboardTex->Resource.Get(), checkboardTex->UploadHeap.Get(),
        0, 0, checkboardTex->SubResourceData.size(), checkboardTex->SubResourceData.data());
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        checkboardTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COMMON
    );
    m_CommandList->ResourceBarrier(1, &pBarrier);
    m_Textures[checkboardTex->Name] = std::move(checkboardTex);

    std::unique_ptr<Texture> iceTex = std::make_unique<Texture>();
    iceTex->Name = "iceTex";
    iceTex->FileName = L"Textures/ice.dds";
    iceTex->AlphaMode = DirectX::DDS_ALPHA_MODE_STRAIGHT;
    iceTex->is_cube = false;
    ThrowIfFailed(DirectX::LoadDDSTextureFromFile(
        m_Device.Get(),
        iceTex->FileName.c_str(),
        iceTex->Resource.GetAddressOf(),
        iceTex->Data,
        iceTex->SubResourceData,
        0, &iceTex->AlphaMode,
        &iceTex->is_cube
    ));
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(iceTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    CD3DX12_HEAP_PROPERTIES iceP(D3D12_HEAP_TYPE_UPLOAD);
    UINT64 iceDescSize = GetRequiredIntermediateSize(iceTex->Resource.Get(),
        0, iceTex->SubResourceData.size());
    CD3DX12_RESOURCE_DESC iceDesc = CD3DX12_RESOURCE_DESC::Buffer(iceDescSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &iceP,
        D3D12_HEAP_FLAG_NONE,
        &iceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(iceTex->UploadHeap.GetAddressOf())
    ));
    UpdateSubresources(m_CommandList.Get(),
        iceTex->Resource.Get(), iceTex->UploadHeap.Get(),
        0, 0, iceTex->SubResourceData.size(), iceTex->SubResourceData.data());
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        iceTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COMMON
    );
    m_CommandList->ResourceBarrier(1, &pBarrier);
    m_Textures[iceTex->Name] = std::move(iceTex);

    std::unique_ptr<Texture> defaultWhiteTex = std::make_unique<Texture>();
    defaultWhiteTex->Name = "defaultWhiteTex";
    defaultWhiteTex->FileName = L"";
    defaultWhiteTex->AlphaMode = DirectX::DDS_ALPHA_MODE_OPAQUE;
    defaultWhiteTex->is_cube = false;

    UINT width = 1;
    UINT height = 1;
    const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    const UINT mipmapLevels = 1;
    CD3DX12_HEAP_PROPERTIES whiteP(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC whiteDesc = CD3DX12_RESOURCE_DESC::Tex2D(format,
        width, height, 1, mipmapLevels,
        1, 0);
    ThrowIfFailed(m_Device->CreateCommittedResource(&whiteP,
        D3D12_HEAP_FLAG_NONE, &whiteDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(defaultWhiteTex->Resource.GetAddressOf())));
    const UINT pixelSize = 4;
    const UINT rowPitch = width * pixelSize;
    const UINT slicPitch = rowPitch * height;
    defaultWhiteTex->Data = std::make_unique<uint8_t[]>(slicPitch);
    memset(defaultWhiteTex->Data.get(), 0xFF, slicPitch);
    D3D12_SUBRESOURCE_DATA subResourceData{};
    subResourceData.pData = defaultWhiteTex->Data.get();
    subResourceData.RowPitch = rowPitch;
    subResourceData.SlicePitch = slicPitch;
    defaultWhiteTex->SubResourceData.push_back(subResourceData);

    CD3DX12_HEAP_PROPERTIES defaultP = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    UINT defaultSize = GetRequiredIntermediateSize(defaultWhiteTex->Resource.Get(),
        0, defaultWhiteTex->SubResourceData.size());
    CD3DX12_RESOURCE_DESC defaultDesc = CD3DX12_RESOURCE_DESC::Buffer(defaultSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(&defaultP,
        D3D12_HEAP_FLAG_NONE,
        &defaultDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(defaultWhiteTex->UploadHeap.GetAddressOf())));
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultWhiteTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    m_CommandList->ResourceBarrier(1, &pBarrier);
    UpdateSubresources(m_CommandList.Get(),
        defaultWhiteTex->Resource.Get(),
        defaultWhiteTex->UploadHeap.Get(),
        0, 0, defaultWhiteTex->SubResourceData.size(),
        defaultWhiteTex->SubResourceData.data());
    pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultWhiteTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
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

    D3D12_DEPTH_STENCIL_DESC DepthStencilDesc{};
    DepthStencilDesc.DepthEnable = true;
    DepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    DepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    DepthStencilDesc.StencilEnable = true;
    DepthStencilDesc.StencilReadMask = 0xff;
    DepthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK; //0xff
    DepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    DepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    //背面测试无所谓因为我们不渲染背面的三角形
    DepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    DepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DepthStencilState = DepthStencilDesc;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0; //禁止颜色写入
    ComPtr<ID3D12PipelineState> stencilRenderPso;
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(stencilRenderPso.GetAddressOf())));
    m_PSOs["stencilPass"] = std::move(stencilRenderPso);
    
    DepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    DepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
    DepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
    psoDesc.DepthStencilState = DepthStencilDesc;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //开启颜色的写入
    ComPtr<ID3D12PipelineState> ReflectPass;
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(ReflectPass.GetAddressOf())));
    m_PSOs["reflectionPass"] = std::move(ReflectPass);

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    D3D12_BLEND_DESC blendStateDesc{};
    blendStateDesc.AlphaToCoverageEnable = false;
    blendStateDesc.IndependentBlendEnable = false;
    D3D12_RENDER_TARGET_BLEND_DESC rendeTargetBlendDesc{};
    rendeTargetBlendDesc.BlendEnable = true;
    rendeTargetBlendDesc.LogicOpEnable = false;
    rendeTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rendeTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rendeTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    rendeTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    rendeTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    rendeTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rendeTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    rendeTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendStateDesc.RenderTarget[0] = rendeTargetBlendDesc;

    psoDesc.BlendState = blendStateDesc;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ComPtr<ID3D12PipelineState> transparentPSO;
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(transparentPSO.GetAddressOf())));
    m_PSOs["transparent"] = std::move(transparentPSO);

    DepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    DepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR_SAT;
    DepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_GREATER;
    //背面测试无所谓因为我们不渲染背面的三角形
    DepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    DepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR_SAT;
    DepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_GREATER;
    psoDesc.DepthStencilState = DepthStencilDesc;
    ComPtr<ID3D12PipelineState> shadowPso;
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(shadowPso.GetAddressOf())));
    m_PSOs["Shadow"] = std::move(shadowPso);
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
    UpdateReflectFrameResources(gt);
    UpdateObj(gt);
    UpdateMaterial(gt);
}

void MaterialApp::UpdateObj(const GameTimer& gt)
{
    for (auto& r : m_RenderItems)
    {
        if (r->NumFrameDirty > 0)
        {
            r->NumFrameDirty--;
            ObjectConsts objConstData;
            
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&r->World);
            DirectX::XMMATRIX worldT = DirectX::XMMatrixTranspose(world);
            DirectX::XMFLOAT4X4 p;
            DirectX::XMStoreFloat4x4(&p, worldT);
            DirectX::XMMATRIX invtworld = MathHelper::InverseTanspose(world);
            DirectX::XMFLOAT4X4 w;
            DirectX::XMStoreFloat4x4(&w, DirectX::XMMatrixTranspose(invtworld));
            objConstData.TInvWorld = w;
            objConstData.World = p;
            objConstData.TexTransform = r->TexTransform;
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

    passCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    passCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
    passCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    passCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    passCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    passCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
    passCB.FogColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    passCB.FogRange = 100.0f;
    passCB.FogStart = 40.0f;
    passCB.Padding = DirectX::XMFLOAT2(0.0f, 0.0f);
    m_CurrentFrameResource->PassCB->CopyData(0, passCB);
    m_MainPassCB = passCB;
}

void MaterialApp::UpdateReflectFrameResources(const GameTimer& gt)
{
    m_ReflectedPassCB = m_MainPassCB;
    DirectX::XMVECTOR ReflectFaceNormal = DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
    DirectX::XMMATRIX ReflectionMatrix = DirectX::XMMatrixReflect(ReflectFaceNormal);
    for (int i = 0; i < 3; ++i)
    {
        DirectX::XMVECTOR lightDir = DirectX::XMLoadFloat3(&m_MainPassCB.Lights[i].Direction);
        lightDir = DirectX::XMVector3Transform(lightDir, ReflectionMatrix);
        DirectX::XMStoreFloat3(&m_ReflectedPassCB.Lights[i].Direction, lightDir);
    }
    m_CurrentFrameResource->PassCB->CopyData(1, m_ReflectedPassCB);
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
            DirectX::XMMATRIX pM = DirectX::XMLoadFloat4x4(&pMat->MatTransform);
            DirectX::XMStoreFloat4x4(&matC.MatTransform, DirectX::XMMatrixTranspose(pM));
            m_CurrentFrameResource->MaterialCB->CopyData(pMat->MatCBIndex, matC);
        }
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
    m_CommandList->SetPipelineState(m_PSOs["material"].Get());
    DrawRenderItems(m_CommandList.Get(), m_PassRenderItems[(int)RenderLayer::Opaque]);

    //stencilPass
    m_CommandList->OMSetStencilRef(1);
    m_CommandList->SetPipelineState(m_PSOs["stencilPass"].Get());
    DrawRenderItems(m_CommandList.Get(), m_PassRenderItems[(int)RenderLayer::Mirrors]);
    
    // ReflectionPass
    m_CommandList->SetPipelineState(m_PSOs["reflectionPass"].Get());
    pFrameResource = FrameConstResource->GetGPUVirtualAddress() + d3dUtil::CalcConstBufferByteSize(sizeof(PassConstant));
    m_CommandList->SetGraphicsRootConstantBufferView(3, pFrameResource);
    DrawRenderItems(m_CommandList.Get(), m_PassRenderItems[(int)RenderLayer::Reflected]);

    //finalpass
    m_CommandList->SetPipelineState(m_PSOs["transparent"].Get());
    pFrameResource = FrameConstResource->GetGPUVirtualAddress();
    m_CommandList->SetGraphicsRootConstantBufferView(3, pFrameResource);
    DrawRenderItems(m_CommandList.Get(), m_PassRenderItems[(int)RenderLayer::Transparent]);

    //shadow
    m_CommandList->ClearDepthStencilView(DepthHandle, D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    m_CommandList->SetPipelineState(m_PSOs["Shadow"].Get());
    DrawRenderItems(m_CommandList.Get(), m_PassRenderItems[(int)RenderLayer::Shadow]);
    
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
