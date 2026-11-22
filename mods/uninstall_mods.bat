@echo off
setlocal
python "%~dp0mod_manager.py" uninstall
set "ERR=%errorlevel%"
if /i not "%~1"=="--nopause" pause
exit /b %ERR%
