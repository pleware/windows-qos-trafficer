@echo off
setlocal
:: ------------------------------------------------------------------
:: Unit tests: compile a standalone test harness (no WinDivert needed)
:: and run it. Fails the build on any failing check.
:: ------------------------------------------------------------------

:: Locate MSVC
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS_INSTALL=%%i
if not defined VS_INSTALL ( echo ERROR: Visual Studio not found. & exit /b 1 )
call "%VS_INSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul

cl /nologo /EHsc /std:c17 /TC /W3 ^
    /I..\tests /I. ^
    ..\tests\test_main.c ..\tests\test_qos_math.c ..\tests\test_schedule.c ..\tests\test_token_bucket.c ^
    schedule.c token_bucket.c localization_api.c localization_data.c ^
    /Fe:..\tests\tests.exe
if %ERRORLEVEL% neq 0 ( echo ERROR: test compilation failed. & exit /b %ERRORLEVEL% )

..\tests\tests.exe
exit /b %ERRORLEVEL%
