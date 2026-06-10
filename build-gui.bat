@echo off
REM ftool-gui 编译脚本
REM 编译带图形界面的文件日期修改工具

setlocal enabledelayedexpansion

echo ============================================
echo  ftool GUI - 文件日期修改工具 GUI版 编译脚本
echo ============================================
echo.

set "FOUND_CC="
set "CC_CMD="
set "RC_CMD="

REM 查找 GCC
for /d %%d in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\*WinLibs*") do (
    if exist "%%d\mingw64\bin\gcc.exe" (
        set "GCC=%%d\mingw64\bin\gcc.exe"
        set "WINDRES=%%d\mingw64\bin\windres.exe"
        set "FOUND_CC=1"
        echo [找到] WinLibs GCC
    )
)

if not defined FOUND_CC (
    for /f "tokens=*" %%i in ('where gcc 2^>nul') do (
        set "GCC=gcc"
        set "WINDRES=windres"
        set "FOUND_CC=1"
        echo [找到] 系统 GCC
    )
)

if not defined FOUND_CC (
    echo [错误] 未找到 GCC 编译器！
    echo 请安装: winget install BrechtSanders.WinLibs.POSIX.UCRT -e
    pause
    exit /b 1
)

echo.
echo 正在编译资源文件 ...
echo.

REM 编译资源（清单 + 图标）
"%WINDRES%" embed.rc -o embed.o
if errorlevel 1 (
    echo [失败] 资源编译出错！
    pause
    exit /b 1
)

echo.
echo 正在编译 ftool-gui.c ...
echo.

"%GCC%" -o ftool-gui.exe ftool-gui.c embed.o -static -O2 -s -municode -mwindows -lcomctl32 -lcomdlg32 -lole32

if errorlevel 1 (
    echo [失败] 编译出错！
    pause
    exit /b 1
)

REM 清理临时文件
if exist embed.o del embed.o >nul

if exist ftool-gui.exe (
    echo.
    echo ============================================
    echo  [成功] ftool-gui.exe 已生成！
    echo ============================================
    dir ftool-gui.exe
    echo.
    echo 双击 ftool-gui.exe 启动图形界面
    echo 或拖放文件到 ftool-gui.exe 图标上
    echo.
)

pause
