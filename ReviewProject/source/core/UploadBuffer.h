#pragma once

template<typename T>
class UploadBuffer
{
public:
    UploadBuffer(ID3D12Device* device, UINT elementCount, bool isContantBuffer)
        : mIsConstantBuffer(isContantBuffer)
    {
        mElementByteSize = sizeof(T); // per Element sizeByte
        if (mIsConstantBuffer)
        {
            mElementByteSize = d3dUtil::CalcConstBufferByteSize(mElementByteSize);
        }
        D3D12_HEAP_PROPERTIES p_p = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC p_r = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
        ThrowIfFailed(device->CreateCommittedResource(&p_p,
        D3D12_HEAP_FLAG_NONE,
        &p_r,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(mUploadBuffer.GetAddressOf())
        ));
        //只要还会修改当前资源，我们就无须取消映射
        ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
    }
    UploadBuffer(const UploadBuffer&&)=delete;
    UploadBuffer(const UploadBuffer&)=delete;
    UploadBuffer& operator=(const UploadBuffer&) = delete;
    ~UploadBuffer()
    {
        if (mUploadBuffer != nullptr)
            mUploadBuffer->Unmap(0, nullptr);
        mUploadBuffer = nullptr;
    }
    operator ID3D12Resource*(){ return mUploadBuffer.Get(); }
    void CopyData(int elementIndex, const T& data)
    {
        memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
    }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
    BYTE* mMappedData = nullptr;

    UINT mElementByteSize = 0;
    bool mIsConstantBuffer = false;
};