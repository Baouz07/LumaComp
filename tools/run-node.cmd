@echo off
rem Runs a Node.js script using the DSH Electron binary as the Node runtime.
setlocal
set ELECTRON_RUN_AS_NODE=1
"C:\Users\Shiyu\AppData\Local\Programs\DSH Desktop\DSH Desktop.exe" %*
exit /b %ERRORLEVEL%
