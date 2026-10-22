//=============================================================================
// 文件:    IPlugin.h
// 说明:    播放器插件接口（抽象基类）
//
//          本头文件是「C++ 多态性（导出类）」插件方式的核心，被宿主程序
//          与所有插件 DLL 共享。它定义了一个统一的插件接口（抽象基类
//          IPlugin），宿主程序与插件都只面向这个接口编程。
//
//          与「导出函数」方式的区别（参考论文《软件插件技术的原理与实现》）：
//            * 导出函数方式：插件 DLL 导出多个相互独立的 C 函数
//              （LoadFile / Play / Stop / Pause / GetSupportedFormat），
//              宿主程序用 GetProcAddress 逐个取得函数地址再调用，
//              这里没有任何多态，只是「按名字查函数」。
//            * 导出类方式：插件 DLL 只导出一个「工厂函数」 CreatePlugin()，
//              它返回一个「基类指针 IPlugin*」；具体实现类（如 CMp3Plugin）
//              隐藏在 DLL 内部，宿主程序通过基类指针调用「虚函数」，
//              从而触发 C++ 的运行时多态（动态绑定 / 晚绑定）。
//
//          正是基类中的 virtual 关键字与「导出类 + 工厂函数」的组合，
//          构成了软件插件技术中「导出类」方案的实现原理。
//=============================================================================
#ifndef IPLUGIN_H
#define IPLUGIN_H

// ---------------------------------------------------------------------------
// 导出/导入宏
//   在插件 DLL 内部编译时定义 PLUGIN_EXPORTS  -> 将类“导出”(dllexport)
//   在宿主程序中编译时不定义 PLUGIN_EXPORTS  -> 将类“导入”(dllimport)
// ---------------------------------------------------------------------------
#ifdef PLUGIN_EXPORTS
    #define PLUGIN_API __declspec(dllexport)
#else
    #define PLUGIN_API __declspec(dllimport)
#endif

// ---------------------------------------------------------------------------
// 插件接口抽象基类
//   通过「纯虚函数」( = 0 ) 声明统一的插件规范，不提供具体实现。
//   所有插件必须公有继承该类并重写（override）全部纯虚函数。
// ---------------------------------------------------------------------------
class PLUGIN_API IPlugin
{
public:
    // 虚析构函数：保证通过基类指针 delete 派生类对象时，能正确调用到
    // 派生类的析构函数（这是与多态配套的关键语法点）。
    virtual ~IPlugin() {}

    // 加载媒体文件（参数为文件名）
    virtual void LoadFile(const char* szFileName) = 0;

    // 播放
    virtual void Play() = 0;

    // 暂停
    virtual void Pause() = 0;

    // 停止
    virtual void Stop() = 0;

    // 返回本插件支持的媒体格式（如 "mp3"、"wma"）
    virtual const char* GetSupportedFormat() = 0;
};

// ---------------------------------------------------------------------------
// 工厂函数声明
//   每个插件 DLL 必须导出一个名为 CreatePlugin 的函数，用于创建具体的
//   插件对象，并以「基类指针 IPlugin*」返回 —— 这是「导出类」方式的统一入口。
//
//   用 extern "C" 修饰是为了关闭 C++ 的名字改编(name mangling)，
//   使导出符号名固定为 "CreatePlugin"，便于宿主用 GetProcAddress 查找。
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
    PLUGIN_API IPlugin* CreatePlugin();
#ifdef __cplusplus
}
#endif

// 工厂函数指针类型，供宿主程序 GetProcAddress 获取函数地址后转换使用
typedef IPlugin* (*CREATE_PLUGIN_FUNC)();

#endif  // IPLUGIN_H
