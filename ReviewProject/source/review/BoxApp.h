#pragma once
#include "D3DUtil.h"
#include "D3DApp.h"
#include "UploadBuffer.h"

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

struct ObjectConsts
{
    DirectX::XMFLOAT4X4 WorldViewProj;
};

class BoxApp : public D3DApp
{
public:
    BoxApp(HINSTANCE hInstance);
    BoxApp(const BoxApp& rhs) = delete;
    BoxApp& operator=(const BoxApp& ths) = delete;
    ~BoxApp();
    bool Initialize() override;
    
private:
    void OnResize() override;
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;
    
    void OnMouseMove(WPARAM btnState, int x, int y) override;
    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    
    void BuildDescriptorHeaps();
    void BuildConstBuffer();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

private:
    ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_ConstBufferHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConsts>> m_ObjectCB = nullptr;
    std::unique_ptr<d3dUtil::MeshGeometry> m_BoxGeometry = nullptr;

    ComPtr<ID3DBlob> m_VSByteCode = nullptr;
    ComPtr<ID3DBlob> m_PSByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayoutDesc;
    ComPtr<ID3D12PipelineState> m_PSO = nullptr;

    DirectX::XMFLOAT4X4 m_World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 m_View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 m_Proj = MathHelper::Identity4x4();

    float m_Theta = 1.5f * DirectX::XM_PI;
    float m_Phi = DirectX::XM_PIDIV4; // pi / 4
    float m_Radius = 5.0f;
    POINT m_LastMousePos;
};
