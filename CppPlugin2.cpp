//=============================================================================
// 文件:    CppPlugin2.cpp
// 说明:    wma 格式播放器插件（「C++ 多态性（导出类）」方式，纯 Win32 无 MFC）
//
//          实现方式与 CppPlugin1.cpp 完全一致，只是支持的格式与提示不同。
//          新增格式只需再写一个 DLL，无需改动宿主程序。
//=============================================================================

#define PLUGIN_EXPORTS
#include "IPlugin.h"

#include <windows.h>
#include <cstdio>

// 具体插件类：继承接口抽象基类 IPlugin，实现 wma 的播放逻辑
class CWmaPlugin : public IPlugin
{
public:
    virtual ~CWmaPlugin() {}

    virtual void LoadFile(const char* szFileName)
    {
        char buf[512];
        _snprintf(buf, sizeof(buf), "wma format is loaded!\nFile: %s",
                  (szFileName != NULL) ? szFileName : "(none)");
        MessageBoxA(NULL, buf, "wma Plugin", MB_OK | MB_ICONINFORMATION);
    }

    virtual void Play()
    {
        MessageBoxA(NULL, "wma format is playing!", "wma Plugin",
                    MB_OK | MB_ICONINFORMATION);
    }

    virtual void Pause()
    {
        MessageBoxA(NULL, "wma format is paused!", "wma Plugin",
                    MB_OK | MB_ICONINFORMATION);
    }

    virtual void Stop()
    {
        MessageBoxA(NULL, "wma format is stopped!", "wma Plugin",
                    MB_OK | MB_ICONINFORMATION);
    }

    virtual const char* GetSupportedFormat()
    {
        return "wma";
    }
};

// 工厂函数：创建 wma 插件对象，返回基类指针（多态）
extern "C" PLUGIN_API IPlugin* CreatePlugin()
{
    return new CWmaPlugin();
}
