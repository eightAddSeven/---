@echo off
rem 使用 MinGW g++ 构建（前提：g++ 已加入 PATH）

g++ -Wall -O2 -I. -shared -o CppPlugin1.dll CppPlugin1.cpp
if errorlevel 1 goto :err

g++ -Wall -O2 -I. -shared -o CppPlugin2.dll CppPlugin2.cpp
if errorlevel 1 goto :err

g++ -Wall -O2 -I. -mwindows -o PlayerGUI.exe PlayerGUI.cpp -lcomdlg32
if errorlevel 1 goto :err

echo.
echo 构建完成：PlayerGUI.exe  CppPlugin1.dll  CppPlugin2.dll
echo 运行方式：双击 PlayerGUI.exe
goto :eof

:err
echo.
echo 构建失败，请检查 g++ 是否可用及源代码是否有误。
