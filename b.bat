@echo off
cmake --build build --config RelWithDebInfo %*
if errorlevel 1 exit /b %errorlevel%
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy-release.ps1"
exit /b %errorlevel%
