#include "D3DUtil.h"
#include "BoxApp.h"

#include <DirectXColors.h>
#include <array>
#include <sstream>
BoxApp::BoxApp(HINSTANCE hInstance)
    :D3DApp(hInstance)
{
    
}

BoxApp::~BoxApp()
{
}

void BoxApp::OnResize()
{
    D3DApp::OnResize();

    DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    DirectX::XMStoreFloat4x4(&m_Proj, p);
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - m_LastMousePos.x));
        float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - m_LastMousePos.y));

        m_Phi += dy;
        m_Theta += dx;

        m_Phi = MathHelper::Clamp(m_Phi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.005f * static_cast<float>(x - m_LastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - m_LastMousePos.y);

        m_Radius += (dx - dy);

        m_Radius = MathHelper::Clamp(m_Radius, 3.0f, 15.0f);
    }
    m_LastMousePos = { x, y };
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    m_LastMousePos = { x, y };
    SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void BoxApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc{};
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(m_ConstBufferHeap.GetAddressOf())));
}

void BoxApp::BuildConstBuffer()
{
    m_ObjectCB = std::make_unique<UploadBuffer<ObjectConsts>>(m_Device.Get(), 1, true);

    UINT objCBByteSize = d3dUtil::CalcConstBufferByteSize(sizeof(ObjectConsts));

    D3D12_GPU_VIRTUAL_ADDRESS cbAdress = static_cast<ID3D12Resource*>(*m_ObjectCB)->GetGPUVirtualAddress();

    // 偏移到常量缓冲区中第i个物体所对应的常量数据
    //这里取i = 0
    int boxCBufferIndex = 0;
    cbAdress += boxCBufferIndex * objCBByteSize;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = cbAdress;
    cbvDesc.SizeInBytes = objCBByteSize;
    
    m_Device->CreateConstantBufferView(&cbvDesc, m_ConstBufferHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    ComPtr<ID3DBlob> rootSignature;
    ComPtr<ID3DBlob> error;
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, &slotRootParameter[0], 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, rootSignature.GetAddressOf(), error.GetAddressOf());

    if (error != nullptr)
    {
        OutputDebugStringA((char*)error->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    ThrowIfFailed(m_Device->CreateRootSignature(0,
        rootSignature->GetBufferPointer(),
        rootSignature->GetBufferSize(),
        IID_PPV_ARGS(m_RootSignature.GetAddressOf())));
}

void BoxApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;
    m_VSByteCode = d3dUtil::CompileShader(L"source/shader/color.usf", nullptr, "VS", "vs_5_0");
    m_PSByteCode = d3dUtil::CompileShader(L"source/shader/color.usf", nullptr, "PS", "ps_5_0");

    m_InputLayoutDesc = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
}

void BoxApp::BuildBoxGeometry()
{
    std::array<Vertex, 8> vertices = {
        Vertex({DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::White)}),
        Vertex({DirectX::XMFLOAT3(-1.0f, 1.0f,  -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Black)}),
        Vertex({DirectX::XMFLOAT3(1.0f, 1.0f,   -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Red)}),
        Vertex({DirectX::XMFLOAT3(1.0f, -1.0f,  -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Green)}),

        Vertex({DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f), DirectX::XMFLOAT4(DirectX::Colors::Blue)}),
        Vertex({DirectX::XMFLOAT3(-1.0f, 1.0f,  1.0f), DirectX::XMFLOAT4(DirectX::Colors::Yellow)}),
        Vertex({DirectX::XMFLOAT3(1.0f, 1.0f,   1.0f), DirectX::XMFLOAT4(DirectX::Colors::Cyan)}),
        Vertex({DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f), DirectX::XMFLOAT4(DirectX::Colors::Magenta)})
    };

    std::array<uint16_t, 36> indices =
        {
        0, 1, 2,
        0, 2, 3,

        4, 6, 5,
        4, 7, 6,

        4, 5, 1,
        4, 1, 0,

        3, 2, 6,
        3, 6, 7,

        1, 5, 6,
        1, 6, 2,

        4, 0, 3,
        4, 3, 7
        };

    UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);
    m_BoxGeometry = std::make_unique<d3dUtil::MeshGeometry>();
    m_BoxGeometry->Name = "boxGeo";
    ThrowIfFailed(D3DCreateBlob(vbByteSize, m_BoxGeometry->VertexBufferCPU.GetAddressOf()));
    CopyMemory(m_BoxGeometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    ThrowIfFailed(D3DCreateBlob(ibByteSize, m_BoxGeometry->IndexBufferCPU.GetAddressOf()));
    CopyMemory(m_BoxGeometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
    m_BoxGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(),
        m_BoxGeometry->VertexBufferCPU->GetBufferPointer(), vbByteSize, m_BoxGeometry->VertexBufferUploader);
    m_BoxGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_Device.Get(), m_CommandList.Get(),
        m_BoxGeometry->IndexBufferCPU->GetBufferPointer(), ibByteSize, m_BoxGeometry->IndexBufferUploader);
    m_BoxGeometry->VertexByteStride = sizeof(Vertex);
    m_BoxGeometry->VertexByteSize = vbByteSize;
    m_BoxGeometry->IndexBufferByteSize = ibByteSize;
    m_BoxGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;

    d3dUtil::SubmeshGeometry subMesh;
    subMesh.IndexCount = indices.size();
    subMesh.BaseVertexLocation = 0;
    subMesh.StartIndexLocation = 0;
    m_BoxGeometry->DrawArgs["box"] = subMesh;
}

void BoxApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.pRootSignature = m_RootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCode.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCode.Get());
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = { m_InputLayoutDesc.data(), (UINT)m_InputLayoutDesc.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_BackBufferFormat;
    psoDesc.DSVFormat = m_DepthStencilFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? m4xMsaaQuality - 1 : 0;

    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_PSO.GetAddressOf())));
}

bool BoxApp::Initialize()
{
    if (!D3DApp::Initialize())
    {
        return false;
    }
    ThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), m_PSO.Get()));
    BuildDescriptorHeaps();
    BuildConstBuffer();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* cmdLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
    FlushCommandQueue();
    
    return true;
}

void BoxApp::Update(const GameTimer& gt)
{
    float x = m_Radius * sinf(m_Phi) * cosf(m_Theta);
    float y = m_Radius * cos(m_Phi);
    float z = m_Radius * sinf(m_Phi) * sinf(m_Theta);

    DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
    DirectX::XMVECTOR target = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    DirectX::XMStoreFloat4x4(&m_View, view);

    DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&m_World);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&m_Proj);
    DirectX::XMMATRIX worldViewProj = world * view * proj;

    ObjectConsts objConstants;
    DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(worldViewProj));
    //objConstants.WorldViewProj = MathHelper::Identity4x4();
    m_ObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt)
{
    ThrowIfFailed(m_CommandAllocator->Reset());
    ThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), m_PSO.Get()));

    m_CommandList->RSSetViewports(1, &m_ViewPort);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRenderView = CurrentBackBufferHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentDepthStencil = DepthStencilView();

    D3D12_RESOURCE_BARRIER p_rb = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &p_rb);
    m_CommandList->ClearRenderTargetView(CurrentRenderView, DirectX::Colors::LightSteelBlue, 0, nullptr);
    m_CommandList->ClearDepthStencilView(CurrentDepthStencil, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    m_CommandList->OMSetRenderTargets(1, &CurrentRenderView, true, &CurrentDepthStencil);
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_ConstBufferHeap.Get()};
    m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = m_BoxGeometry->VertexBufferView();
    D3D12_INDEX_BUFFER_VIEW indexBufferView = m_BoxGeometry->IndexBufferView();
    m_CommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    m_CommandList->IASetIndexBuffer(&indexBufferView);
    m_CommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_CommandList->SetGraphicsRootDescriptorTable(0, m_ConstBufferHeap->GetGPUDescriptorHandleForHeapStart());
    m_CommandList->DrawIndexedInstanced(m_BoxGeometry->DrawArgs["box"].IndexCount, 1, 0, 0, 0);

    p_rb = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &p_rb);
    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* cmdLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(m_SwapChain->Present(0, 0));
    m_CurrentBackBufferIndex = (m_CurrentBackBufferIndex + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}
