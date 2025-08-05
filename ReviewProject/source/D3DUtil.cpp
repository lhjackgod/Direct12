#include "D3DUtil.h"
#include "core/d3dx12.h"
#include <fstream>
namespace d3dUtil
{
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
)
    {
        using Microsoft::WRL::ComPtr;
        ComPtr<ID3D12Resource> defaultBuffer;
        D3D12_HEAP_PROPERTIES pHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC pBuffer = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
        ThrowIfFailed(device->CreateCommittedResource(
            &pHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &pBuffer,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(defaultBuffer.GetAddressOf())
        ));

        D3D12_HEAP_PROPERTIES p2HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC p2Buffer = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
        ThrowIfFailed(device->CreateCommittedResource(&p2HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &p2Buffer,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(uploadBuffer.GetAddressOf())
        ));

        D3D12_SUBRESOURCE_DATA subResourceData;
        subResourceData.pData = initData;
        subResourceData.RowPitch = byteSize;
        subResourceData.SlicePitch = byteSize;

        D3D12_RESOURCE_BARRIER p_rb =  CD3DX12_RESOURCE_BARRIER::Transition(
            defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        cmdList->ResourceBarrier(1, &p_rb);
        UpdateSubresources(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

        p_rb = CD3DX12_RESOURCE_BARRIER::Transition(
            defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_COMMON
        );
        cmdList->ResourceBarrier(1, &p_rb);
        return defaultBuffer;
    }
    UINT CalcConstBufferByteSize(UINT byteSize)
    {
        return (byteSize + 255) & ~255;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
    {
        UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        #endif

        HRESULT hr = S_OK;
        Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint.c_str(), target.c_str(), compileFlags, 0, byteCode.GetAddressOf(), errors.GetAddressOf());

        if (errors != nullptr)
        {
            OutputDebugStringA((char*)errors->GetBufferPointer());
        }
        ThrowIfFailed(hr);
        return byteCode;
    }
    Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename)
    {
        std::ifstream fin(filename, std::ios::binary);
        fin.seekg(0, std::ios_base::end);
        std::fstream::pos_type fileSize = fin.tellg();
        fin.seekg(0, std::ios_base::beg);
        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        ThrowIfFailed(D3DCreateBlob(fileSize, blob.GetAddressOf()));
        fin.read((char*)blob->GetBufferPointer(), fileSize);
        fin.close();
        return blob;
    }


}
float MathHelper::Pi = 3.1415926f;