#include "D3DUtil.h"
#include "review/BoxApp.h"
#include "d3dx12.h"
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    //为调试版本开启运行时内存检测，方便监督内存泄露的情况
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    try
    {
        BoxApp app(hInstance);
        if (!app.Initialize())
            return 0;
        return app.Run();
    }
    catch (MyException& e)
    {
        std::cout << e.what() << std::endl;
        MessageBox(nullptr, L"error", L"Error", MB_OK);
        return 0;
    }
}
