#include "D3DUtil.h"
#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassContant>>(device, passCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConsts>>(device, objectCount, true);
}
FrameResource::~FrameResource()
{
    
}

