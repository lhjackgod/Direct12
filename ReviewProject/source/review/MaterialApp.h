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

    void Update(const GameTimer& gt) override;
    void UpdateMaterial();
    void UpdateObj();
    void UpdateFramresouce();
    
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*> &ritems);
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
    
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
    ComPtr<ID3D12DescriptorHeap> m_SRVDescriptorHeap;
    ComPtr<ID3D12RootSignature> m_RootSignature;
    std::unordered_map<std::string, std::unique_ptr<d3dUtil::MeshGeometry>> m_Geometries;
    std::unique_ptr<UploadBuffer<Vertex>> m_SeaUploaderBuffer;
    std::vector<std::unique_ptr<RenderItem>> m_RenderItems;
    std::unordered_map<std::string, std::unique_ptr<d3dUtil::Material>> m_Materials;
    std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
    ComPtr<ID3DBlob> m_VSShader;
    ComPtr<ID3DBlob> m_PSShader;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;
};
