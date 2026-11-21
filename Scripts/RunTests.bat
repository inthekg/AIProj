@echo off
setlocal
REM Run automation tests from the command line. Usage: RunTests.bat [filter] (default: AIGame)
REM Run after Build.bat succeeds, with the editor closed. Exit code is non-zero if any test fails.
set UE_ROOT=D:\UE_5.5
for %%I in ("%~dp0..") do set "PROJECT_DIR=%%~fI"
set "FILTER=%~1"
if "%FILTER%"=="" set "FILTER=AIGame"
set "LOG=%PROJECT_DIR%\Saved\Logs\RunTests.log"
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT_DIR%\AIGame.uproject" -ExecCmds="Automation RunTests %FILTER%;Quit" -unattended -nopause -nosplash -nullrhi -abslog="%LOG%" >nul 2>&1
set RESULT=%ERRORLEVEL%
findstr /C:"Test Completed." /C:"LogAutomationController: Error" /C:"TEST COMPLETE" "%LOG%"
exit /b %RESULT%
