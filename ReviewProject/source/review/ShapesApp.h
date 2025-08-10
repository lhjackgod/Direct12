#pragma once
#include "D3DUtil.h"

#include "core/D3DApp.h"
#include "core/FrameResource.h"
class ShapesApp : public D3DApp
{
public:
    void BuildFrameResources();

    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;
private:
    DirectX::XMFLOAT4X4 m_View;
    DirectX::XMFLOAT4X4 m_Proj;
    PassConstant m_MainPassCB;
    DirectX::XMFLOAT3 m_EyePos;
};
