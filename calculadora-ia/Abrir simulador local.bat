@echo off
setlocal
title Calculadora IA - simulador local
cd /d "%~dp0"
rem Usa Node.js 18 o mas nuevo si esta instalado; si no, PowerShell (viene con Windows).
node -e "process.exit(Number(process.versions.node.split('.')[0]) >= 18 ? 0 : 1)" >nul 2>nul
if %errorlevel%==0 (
  echo Abriendo el simulador local con Node.js...
  node "%~dp0tools\servidor-local.mjs"
) else (
  echo Abriendo el simulador local con PowerShell...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\servidor-local.ps1"
)
echo.
echo El simulador local se cerro.
pause
