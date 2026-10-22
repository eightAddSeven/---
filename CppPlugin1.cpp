//=============================================================================
// 文件:    CppPlugin1.cpp
// 说明:    mp3 格式播放器插件（「C++ 多态性（导出类）」方式，纯 Win32 无 MFC）
//
//          1. 定义 PLUGIN_EXPORTS 使接口 IPlugin 被「导出」；
//          2. 具体类 CMp3Plugin 公有继承接口抽象基类 IPlugin；
//          3. 重写全部纯虚函数，用 MessageBox 弹出提示（模拟播放）；
//          4. 导出工厂函数 CreatePlugin() 返回基类指针。
//=============================================================================

#define PLUGIN_EXPORTS
#include "IPlugin.h"

#include <windows.h>
#include <cstdio>

// 具体插件类：继承接口抽象基类 IPlugin，实现 mp3 的播放逻辑
class CMp3Plugin : public IPlugin
{
public:
    virtual ~CMp3Plugin() {}

    virtual void LoadFile(const char* szFileName)
    {
        char buf[512];
        _snprintf(buf, sizeof(buf), "mp3 format is loaded!\nFile: %s",
                  (szFileName != NULL) ? szFileName : "(none)");
        MessageBoxA(NULL, buf, "mp3 Plugin", MB_OK | MB_ICONINFORMATION);
    }

    virtual void Play()
    {
        MessageBoxA(NULL, "mp3 format is playing!", "mp3 Plugin",
                    MB_OK | MB_ICONINFORMATION);
    }

    virtual void Pause()
    {
        MessageBoxA(NULL, "mp3 format is paused!", "mp3 Plugin",
                    MB_OK | MB_ICONINFORMATION);
    }

    virtual void Stop()
    {
        MessageBoxA(NULL, "mp3 format is stopped!", "mp3 Plugin",
                    MB_OK | MB_ICONINFORMATION);
    }

    virtual const char* GetSupportedFormat()
    {
        return "mp3";
    }
};

// 工厂函数：创建 mp3 插件对象，返回基类指针（多态）
extern "C" PLUGIN_API IPlugin* CreatePlugin()
{
    return new CMp3Plugin();
}
