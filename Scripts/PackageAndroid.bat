@echo off
setlocal
REM Package Android APK (ASTC, Development).
REM Source: the UAT command the editor actually ran for Platforms > Android > Package Project
REM         (Saved/Logs/AIGame.log, 2026-11-17 13:06).
REM Changed from the source: removed "-EditorIO -EditorIOPort=9885" (editor-only channel; no editor listens when run from cmd).
REM If C++ changed, run Build.bat first: -skipbuildeditor does not rebuild the editor DLLs used for cooking.
set UE_ROOT=D:\UE_5.5
for %%I in ("%~dp0..") do set "PROJECT=%%~fI\AIGame.uproject"
set ARCHIVE_DIR=C:\Users\qkdtm\Desktop\App
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" -ScriptsForProject="%PROJECT%" Turnkey -command=VerifySdk -platform=Android -UpdateIfNeeded -project="%PROJECT%" BuildCookRun -nop4 -utf8output -nocompileeditor -skipbuildeditor -cook -project="%PROJECT%" -target=AIGame -unrealexe="%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -platform=Android -cookflavor=ASTC -installed -stage -archive -package -build -pak -iostore -compressed -prereqs -archivedirectory="%ARCHIVE_DIR%" -clientconfig=Development -nocompile -nocompileuat
exit /b %ERRORLEVEL%
