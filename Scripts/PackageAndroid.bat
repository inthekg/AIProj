@echo off
setlocal
REM Package Android APK (ASTC).
REM Usage: PackageAndroid.bat [Development or Shipping]   (default: Development)
REM Source: the UAT command the editor actually ran for Platforms > Android > Package Project
REM         (Saved/Logs/AIGame.log, 2026-11-17 13:06).
REM Changed from the source: removed "-EditorIO -EditorIOPort=9885" (editor-only channel; no editor listens when run from cmd);
REM                         -clientconfig comes from the first argument instead of fixed Development.
REM If C++ changed, run Build.bat first: -skipbuildeditor does not rebuild the editor DLLs used for cooking.
set UE_ROOT=D:\UE_5.5
for %%I in ("%~dp0..") do set "PROJECT=%%~fI\AIGame.uproject"
REM Output folder: ProjectRoot\Archive (resolved to an absolute path at run time; ignored by git).
for %%I in ("%~dp0..\Archive") do set "ARCHIVE_DIR=%%~fI"
set CONFIG=Development
if not "%~1"=="" set "CONFIG=%~1"
if /I "%CONFIG%"=="Development" set CONFIG=Development& goto :config_ok
if /I "%CONFIG%"=="Shipping" set CONFIG=Shipping& goto :config_ok
echo Usage: PackageAndroid.bat [Development or Shipping]
exit /b 1
:config_ok
echo Packaging Android (%CONFIG%)...
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" -ScriptsForProject="%PROJECT%" Turnkey -command=VerifySdk -platform=Android -UpdateIfNeeded -project="%PROJECT%" BuildCookRun -nop4 -utf8output -nocompileeditor -skipbuildeditor -cook -project="%PROJECT%" -target=AIGame -unrealexe="%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -platform=Android -cookflavor=ASTC -installed -stage -archive -package -build -pak -iostore -compressed -prereqs -archivedirectory="%ARCHIVE_DIR%" -clientconfig=%CONFIG% -nocompile -nocompileuat
exit /b %ERRORLEVEL%
