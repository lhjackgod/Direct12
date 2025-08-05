#pragma once



#include "core/D3DApp.h"

class ReviewApp : public D3DApp
{
public:
    ReviewApp(HINSTANCE hInstance)
        : D3DApp(hInstance) {}
    ~ReviewApp() {}

    bool Initialize() override;

private:
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;
};