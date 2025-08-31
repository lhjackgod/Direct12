#pragma once
#include "UploadBuffer.h"
#define MaxLights 16
struct PassConstant
{
    DirectX::XMFLOAT4X4 View;
    DirectX::XMFLOAT4X4 InvView;
    DirectX::XMFLOAT4X4 Proj;
    DirectX::XMFLOAT4X4 InvProj;
    DirectX::XMFLOAT4X4 ViewProj;
    DirectX::XMFLOAT4X4 InvViewProj;
    DirectX::XMFLOAT3 EyePosW;
    float cbPerObjectPad1;
    DirectX::XMFLOAT2 RenderTargetSize;
    DirectX::XMFLOAT2 InvRenderTargetSize;
    float NearZ;
    float FarZ;
    float TotalTime;
    float DeltaTime;
    DirectX::XMFLOAT4 gAmbientLight;

    d3dUtil::Light gLights[MaxLights];
};
struct ObjectConsts
{
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4X4 TInvWorld;
};
struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
};

struct MaterialConstants
{
    DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 FresnelR0 = {0.01f, 0.01f, 0.01f};
    float Roughness = 0.25f;
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct RenderItem
{
    RenderItem() = default;
    //描述物体局部空间相对于世界空间的世界矩阵
    //它定义了物体位于世界空间中的位置、朝向以及大小
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

    //用已更行标记（dirty flag）来表示物体的相关数据已发生改变，这意味着我们此时需要更新常量缓冲区。
    //由于每个FrameResource中都有一个物体常量缓冲区，所以我们必须对每个FrameResource都进行更新
    //即，当我们修改物体数据的时候，应当按NumFramesDirty=gNumFrameResource进行设置
    //从而每个帧资源都得到更新
    int NumFrameDirty = gNumFrameResources; // 这里的意思就是物体primitive更新以后，那么所有的帧资源都应该更新
    //该索引指向的GPU常量缓冲区对应于当前渲染项中的物体常量缓冲区
    UINT ObjCBIndex  =-1;
    //此渲染项参与绘制的几何体。注意，绘制一个几何体可能会用到多个渲染项
    d3dUtil::MeshGeometry* Geo = nullptr;
    d3dUtil::Material* mat = nullptr;
    //DrawIndexedInstanced方法的参数
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType;
};

class FrameResource
{
public:
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
    FrameResource(const FrameResource&) = delete;
    FrameResource& operator=(const FrameResource) = delete;
    ~FrameResource();

    // 在GPU处理与此命令分配器相关的命令之前，我们不能对它进行重置。
    //所以每一帧都要有它们的命令分配器
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    //在GPU执行完引用此常量缓冲区的命令之前，我们不能对它进行更新。
    //因此每一帧都要有它们自己的常量缓冲区
    std::unique_ptr<UploadBuffer<PassConstant>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConsts>> ObjectCB = nullptr;

    // Material
    std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;

    //通过围栏值将命令标记到此围栏点，这使得我们可以检测GPU是否还在使用这些帧资源
    UINT Fence = 0;
};
