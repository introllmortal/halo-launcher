@echo off
chcp 65001 >nul
title Патч обновления сборки Create Clockwork Empire
echo ========================================================
echo  Патч привязки сборки к GitHub обновлениям
echo ========================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0patch_instance.ps1"

echo.
echo ========================================================
pause
