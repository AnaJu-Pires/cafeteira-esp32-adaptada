@echo off
setlocal
if "%~1"=="" (
    echo Uso: gravar.cmd COM3
    exit /b 1
)
cd /d "%~dp0"
where idf.py >nul 2>nul
if errorlevel 1 (
    echo [ERRO] Abra primeiro o terminal ESP-IDF PowerShell configurado.
    exit /b 1
)
echo Compilando firmware em %CD% ...
call idf.py build
if errorlevel 1 exit /b 1
echo Gravando firmware na porta %~1 ...
call idf.py -p "%~1" flash monitor
exit /b %errorlevel%
