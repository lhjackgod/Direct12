#pragma once
#include <map>

#include "D3DApp.h"
#include "FrameResource.h"

class MaterialApp : public D3DApp
{
public:
    MaterialApp(HINSTANCE hInstance);
    bool Initialize() override;
private:
    struct WaveParams
    {
        float A_min = 0.1f;
        float A_max = 0.5f;
        float WaveLength_min = 2.0f;
        float WaveLength_max = 8.0f;
        float Speed_min = 1.0f;
        float Speed_max = 3.0f;
    };
    float GetHillHeight(float x, float z) const;
    DirectX::XMFLOAT3 GetHillsNormal(float x, float z) const;
    void CreateGeometries();
    void CreateFrameResources();
    void CreateRenderItems();
    void CreateSamplerDescriptor();
    void CreateShaderAndInputLayout();
    void createRootSignature();
    void LoadTextureResources();
    void LoadDefaultWhiteTexture();
    void CreateSRVDescriptorHeap();
    void CreateSRVView();
    void CreateMaterial();
    void CreatePSO();
    void OnResize() override;

    void Update(const GameTimer& gt) override;
    void UpdateMaterial(const GameTimer& gt);
    void UpdateObj(const GameTimer& gt);
    void UpdateFramresouce(const GameTimer& gt);
    void UpdateSea(const GameTimer& gt);
    float CalculateWaveHeight(float amplitude,
        float waveSpeed,
        float speed,
        float time,
        DirectX::XMFLOAT2 direction,
        DirectX::XMFLOAT2 pos_xy);
    DirectX::XMFLOAT3 calculateNormal(float amplitude,
        float waveSpeed,
        float speed,
        float time,
        DirectX::XMFLOAT2 direction,
        DirectX::XMFLOAT2 pos_xy);
    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;

    void Draw(const GameTimer& gt) override;
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*> &ritems);
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
    
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
    ComPtr<ID3D12DescriptorHeap> m_SRVDescriptorHeap;
    ComPtr<ID3D12RootSignature> m_RootSignature;
    std::unordered_map<std::string, std::unique_ptr<d3dUtil::MeshGeometry>> m_Geometries;
    std::unique_ptr<UploadBuffer<Vertex>> m_SeaUploaderBuffer;
    std::vector<std::unique_ptr<RenderItem>> m_RenderItems;
    std::vector<RenderItem*> m_Opaques;
    std::unordered_map<std::string, std::unique_ptr<d3dUtil::Material>> m_Materials;
    std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
    FrameResource* m_CurrentFrameResource;
    ComPtr<ID3DBlob> m_VSShader;
    ComPtr<ID3DBlob> m_PSShader;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;
    UINT m_CurrentFrameIndex = 0;

    POINT m_MousePos;
    float m_Phi = DirectX::XM_PIDIV4; // pi / 4
    float m_Theta = 1.5f * DirectX::XM_PI;
    float m_Radius = 5.0f;
    float m_SunTheta = 1.25f * DirectX::XM_PI;
    float m_SunPhi = DirectX::XM_PIDIV4;
    DirectX::XMFLOAT4X4 m_View;
    DirectX::XMFLOAT4X4 m_Projection;
    DirectX::XMFLOAT3 m_Eyepos;
};
