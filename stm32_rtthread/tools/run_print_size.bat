@echo off
REM Wrapper to run tools\print_size.py on Windows while avoiding PYTHONHOME/PYTHONPATH conflicts.
REM Usage: run_print_size.bat <elf-path>

setlocal
REM Directory of this script (ends with backslash)
set SCRIPT_DIR=%~dp0
REM Workspace root (one level up from tools)
set WORKSPACE_DIR=%SCRIPT_DIR%..\

REM Clear Python env vars that may point to an incompatible Python installation
set "PYTHONHOME="
set "PYTHONPATH="

REM Resolve python executables to try (prefer venv)
set VENV_PY=%WORKSPACE_DIR%\.venv\Scripts\python.exe

REM Accept ELF path as first arg, default to Debug\rtthread.elf if not provided
if "%~1"=="" (
  set ELF_PATH=%WORKSPACE_DIR%Debug\rtthread.elf
) else (
  set ELF_PATH=%~1
)

REM Prefer PowerShell implementation on Windows to avoid Python env conflicts
if exist "%WORKSPACE_DIR%tools\print_size.ps1" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%WORKSPACE_DIR%tools\print_size.ps1" "%ELF_PATH%"
  if %ERRORLEVEL%==0 exit /b 0
)

REM Try venv python
if exist "%VENV_PY%" (
  "%VENV_PY%" "%WORKSPACE_DIR%tools\print_size.py" "%ELF_PATH%"
  exit /b %ERRORLEVEL%
)

REM Try py -3
py -3 "%WORKSPACE_DIR%tools\print_size.py" "%ELF_PATH%" 2>nul
if %ERRORLEVEL%==0 exit /b 0

REM Try system python
python "%WORKSPACE_DIR%tools\print_size.py" "%ELF_PATH%" 2>nul
if %ERRORLEVEL%==0 exit /b 0

echo Warning: no working python executable found to run tools\print_size.py
exit /b 0
