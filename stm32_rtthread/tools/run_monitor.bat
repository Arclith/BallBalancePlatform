@echo off
chcp 65001 >nul
set PYTHONHOME=
set PYTHONPATH=
cd /d "%~dp0"
echo 正在启动彩色串口监视器...
"D:\STM32_project\stm32_rtthread\.venv\Scripts\python.exe" serial_monitor.py
pause
