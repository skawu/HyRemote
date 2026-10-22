@echo off
rem Console-only input adapter; CMake owns candidate validation and resolution.
rem Reading CON explicitly avoids inheriting the redirected pipe from execute_process.
setlocal EnableExtensions EnableDelayedExpansion
<nul set /p "=Choose Qt Kit [1-%~1; q=cancel] (no default): " >CON
if errorlevel 1 exit /b 3
set "HYREMOTE_QT_CHOICE="
set /p "HYREMOTE_QT_CHOICE=" <CON
if errorlevel 1 exit /b 3
echo HYREMOTE_QT_KIT_CHOICE=!HYREMOTE_QT_CHOICE!
exit /b 0
