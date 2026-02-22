@echo off
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if not exist "bin" mkdir bin
rc.exe /fo app.res app.rc
cl.exe /nologo /EHsc /std:c++17 /Fe:bin\MemosTodo.exe main.cpp OverlayWindow.cpp MemosPopup.cpp MemosClient.cpp TaskbarTracker.cpp ConfigWindow.cpp app.res /link User32.lib Gdi32.lib Gdiplus.lib Winhttp.lib Shell32.lib Ole32.lib Comctl32.lib Dwmapi.lib
if %errorlevel% neq 0 (
    echo Build Failed!
    exit /b %errorlevel%
)
echo Build Success! bin\MemosTodo.exe created.
