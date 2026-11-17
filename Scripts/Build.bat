@echo off
setlocal
REM Compile the editor target (AIGameEditor, Win64 Development).
REM Close the editor first: Live Coding blocks external builds.
set UE_ROOT=D:\UE_5.5
for %%I in ("%~dp0..") do set "PROJECT=%%~fI\AIGame.uproject"
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" AIGameEditor Win64 Development -Project="%PROJECT%" -WaitMutex
exit /b %ERRORLEVEL%
