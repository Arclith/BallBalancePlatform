@echo off
:: 声明使用 UTF-8 编码，解决中文路径乱码问题
chcp 65001 >nul

:: 清理环境变量
set PYTHONHOME=
set PYTHONPATH=

:: 切换到脚本所在目录
cd /d "%~dp0"

:: 运行程序
"D:\STM32_project\stm32_rtthread\.venv\Scripts\python.exe" data_analyze.py

pause