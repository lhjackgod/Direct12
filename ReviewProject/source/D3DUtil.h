#pragma once

#include <Windows.h>
#include <windowsx.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <iostream>
#include <DirectXMath.h>
#include "core/d3dx12.h"
#include <DirectXCollision.h>
#include <unordered_map>

extern const int gNumFrameResources;
namespace MathHelper
{
    template <typename T>
    inline T Clamp(T v, T min, T max)
    {
        return (v < min) ? min : ((v > max) ? max : v);
    }
    inline DirectX::XMFLOAT4X4 Identity4x4()
    {
        DirectX::XMFLOAT4X4 result = DirectX::XMFLOAT4X4();
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                if (i == j)
                {
                    result.m[i][j] = 1.0f;
                }
                else
                {
                    result.m[i][j] = 0.0f;
                }
            }
        }
        return result;
    }
    extern float Pi;
    inline DirectX::XMMATRIX InverseTanspose(DirectX::CXMMATRIX M)
    {
        DirectX::XMMATRIX A = M;
        A.r[3] = DirectX::XMVectorSet(0.f, 0.f, 0.f, 1.f);
        DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(A);
        return  DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, A));
    }
    inline float frac(float x)
    {
        return x - floorf(x);
    }
}

namespace d3dUtil
{
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
);
    UINT CalcConstBufferByteSize(UINT byteSize);
    Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target
    );
    Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

    struct SubmeshGeometry
    {
        UINT IndexCount = 0;
        UINT StartIndexLocation = 0;
        UINT BaseVertexLocation = 0;

        //通过子网络定义当前SubmeshGeometry结构体中所存几何体的包围盒（bounding box）。我们将在本书的后续章节中使用这个数据
        DirectX::BoundingBox Bounds;
    };

    struct MeshGeometry
    {
        std::string Name;

        Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

        UINT VertexByteStride = 0;
        UINT VertexByteSize = 0;
        DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
        UINT IndexBufferByteSize = 0;

        //一个MeshGeometry结构体能够存储一组顶点/索引缓冲区中的多个几何体
        //若利用下列容器来定义子网络几何体，我们就能单独地绘制出其中的子网络（单个几何体）
        std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

        D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
        {
            D3D12_VERTEX_BUFFER_VIEW vbv;
            vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
            vbv.SizeInBytes = VertexByteSize;
            vbv.StrideInBytes = VertexByteStride;

            return vbv;
        }

        D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
        {
            D3D12_INDEX_BUFFER_VIEW ibv;
            ibv.Format = IndexFormat;
            ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
            ibv.SizeInBytes = IndexBufferByteSize;

            return ibv;
        }

        //数据上传到GPU以后，就需要释放这些内存
        void DisposeUploaders()
        {
            VertexBufferUploader = nullptr;
            IndexBufferUploader = nullptr;
        }
    };
    struct Material
    {
        // 便于查找材质唯一对应名称
        std::string Name;
        //本材质的常量缓冲区
        int MatCBIndex = -1;
        //漫反射纹理在SRV堆中的索引。在第9章纹理贴图时会用到
        int DiffuseSrvHeapIndex = -1;

        // 已更新标志(dirty flag, 也作脏标志)表示本材质已有变动，而我们也就需要更新常量缓冲区了。
        // 由于每个帧资源FrameResource都有一个材质常量缓冲区。所以必须对每个FrameResource都进行更新
        // 因此当修改某个材质时，应当设置NumFrameDirty = gNumFrameResource，以使每个帧资源都能得到更新
        int NumFrameDirty = gNumFrameResources;
        //用于着色的材质常量缓冲区数据
        DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
        DirectX::XMFLOAT3 FresnelR0 = {0.01f, 0.01f, 0.01f};
        float Roughness = 0.25f;
        DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
    };
    struct Light
    {
        DirectX::XMFLOAT3 Strength = {0.5f, 0.5f, 0.5f}; //光源的颜色
        float FalloffStart = 1.0f; // 仅供点光源/聚光灯光源使用
        DirectX::XMFLOAT3 Direction = {0.0f, -1.0f, 0.0f}; // 仅供点光源/聚光灯光源使用
        float FalloffEnd = 10.0f; // 仅供点光源/聚光灯光源使用
        DirectX::XMFLOAT3 Position = {0.0f, 0.0f, 0.0f};// 仅供点光源/聚光灯光源使用
        float SpotPower = 64.0f; // 仅供聚光灯光源使用
    };
}
