#pragma once
#include "D3DApp.h"
#include "FrameResource.h"

class LitWavesApp : public D3DApp
{
public:
    LitWavesApp(HINSTANCE hInstance);
    bool Initialize() override;
    void BuildMaterial();
private:
    float GetHillHeight(float x, float z) const;
    DirectX::XMFLOAT3 GetHillsNormal(float x, float z) const;
    void CreateLandGeometry();
    void CreateLandRenderItems();
    void CreateSeaGeometry();
    void CreateSeaRenderItems();
    void CreateConstDescriptor();
    void CreateConstBufferView();
    void CreateConstBuffer();
    void CreateRootSignature();
    void BuildShadersAndInputLayout();
    void BuildsPSO();
    void OnResize() override;

    void Update(const GameTimer& gt) override;
    void UpdateMaterialCBs(const GameTimer& gt);
    void UpdateRenderItems(const GameTimer& gt);
    void UpdateFrameBuffer(const GameTimer& gt);
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
    void UpdateWaterHeight(const GameTimer& gt);

    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;

    void Draw(const GameTimer& gt) override;
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const GameTimer& gt);
    void OnKeyboardInpput(const GameTimer& gt);
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
    
    std::unordered_map<std::string, std::unique_ptr<d3dUtil::Material>> m_Materials;
    FrameResource* m_CurrentFrameResource;
    std::unordered_map<std::string, std::unique_ptr<d3dUtil::MeshGeometry>> m_Geometries;
    std::vector<std::unique_ptr<RenderItem>> m_AllRitems;
    std::vector<RenderItem*> m_OpaqueRitems;
    std::unique_ptr<UploadBuffer<Vertex>> m_SeaVertexBuffer;
    const int gNumFrameResources = 3;
    ComPtr<ID3D12DescriptorHeap> m_CbvHeap;
    UINT m_PassCbvBeginIndex;
    UINT m_MaterialCbvBeginIndex;
    std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
    DirectX::XMFLOAT4X4 m_View;
    DirectX::XMFLOAT3 m_Eyepos;
    DirectX::XMFLOAT4X4 m_Projection;

    ComPtr<ID3D12RootSignature> m_RootSignature;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputElementDescs;
    ComPtr<ID3DBlob> m_VSShader;
    ComPtr<ID3DBlob> m_PSShader;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PipelineStates;
    POINT m_MousePos;
    float m_Phi = DirectX::XM_PIDIV4; // pi / 4
    float m_Theta = 1.5f * DirectX::XM_PI;
    float m_Radius = 5.0f;
    UINT m_CurrentFrameIndex = 0;
    float m_SunTheta = 1.25f * DirectX::XM_PI;
    float m_SunPhi = DirectX::XM_PIDIV4;
};
