@echo off
set SCRIPT=%~dp0setup.ps1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"Start-Process PowerShell -Verb RunAs -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File ""%SCRIPT%""'"