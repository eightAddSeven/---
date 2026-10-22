#=============================================================================
# 构建脚本（MinGW-w64 / g++）
#   使用方式：make  （或 make all）
#=============================================================================

CXX      = g++
CXXFLAGS = -Wall -O2 -I.

# 导出类插件（DLL）：源文件开头 #define PLUGIN_EXPORTS 使 IPlugin 被导出
CppPlugin1.dll: CppPlugin1.cpp IPlugin.h
	$(CXX) $(CXXFLAGS) -shared -o $@ CppPlugin1.cpp

CppPlugin2.dll: CppPlugin2.cpp IPlugin.h
	$(CXX) $(CXXFLAGS) -shared -o $@ CppPlugin2.cpp

# 宿主（Win32 GUI 程序）：-mwindows 生成无控制台的图形界面程序，
# -lcomdlg32 链接“打开文件”通用对话框库
PlayerGUI.exe: PlayerGUI.cpp IPlugin.h
	$(CXX) $(CXXFLAGS) -mwindows -o $@ PlayerGUI.cpp -lcomdlg32

all: CppPlugin1.dll CppPlugin2.dll PlayerGUI.exe

clean:
	rm -f CppPlugin1.dll CppPlugin2.dll PlayerGUI.exe

.PHONY: all clean
