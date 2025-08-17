#pragma once
#include "D3DUtil.h"
#include "core/D3DApp.h"
#include "core/FrameResource.h"
class ShapesApp : public D3DApp
{
public:

    ShapesApp(HINSTANCE hInstance);
    
    bool Initialize() override;
    void OnResize() override;
    

    void BuildFrameResources();
    void BuildRenderItems();
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void BuildShapeGeometry();

    void BuildConstHeadDescriptor();
    void BuildConstantBufferViews();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSO();

    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
private:

    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
    DirectX::XMFLOAT4X4 m_View;
    DirectX::XMFLOAT4X4 m_Proj;
    PassConstant m_MainPassCB;
    DirectX::XMFLOAT3 m_EyePos;
    std::unordered_map<std::string, std::unique_ptr<d3dUtil::MeshGeometry>> m_Geometries;
    
    const int gNumFrameResources = 3;
    ComPtr<ID3D12DescriptorHeap> m_CbvDescriptorHeap;
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3DBlob> m_VSShader;
    ComPtr<ID3DBlob> m_PSShader;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputElementDesc;
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    std::vector<RenderItem*> mOpaqueRitems;
    std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

    FrameResource* m_CurrentFrameResource;
    int m_CurrentFrameResourceIndex = 0;
    UINT m_PassCbvOffset = 0;
    float m_Theta = 1.5f * DirectX::XM_PI;
    float m_Phi = DirectX::XM_PIDIV4; // pi / 4
    float m_Radius = 5.0f;
    POINT m_MousePos;
};
