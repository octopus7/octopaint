@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem Normalize duplicate PATH/Path entries that can be injected by some CI hosts.
set "OCTOPAINT_BUILD_PATH=%PATH%"
set "PATH="
set "Path="
set "PATH=%OCTOPAINT_BUILD_PATH%"

set "SCRIPT_DIR=%~dp0"
set "SOLUTION=%SCRIPT_DIR%OctoPaint.sln"
set "VERSION_FILE=%SCRIPT_DIR%VERSION"
set "APP_OUTPUT=%SCRIPT_DIR%out\bin\x64\Release\OctoPaint"
set "TEST_EXE=%SCRIPT_DIR%out\bin\x64\Release\OctoPaint.Core.Tests\OctoPaint.Core.Tests.exe"
set "APPLICATION_TEST_EXE=%SCRIPT_DIR%out\bin\x64\Release\OctoPaint.Application.Tests\OctoPaint.Application.Tests.exe"
set "DOMAIN_TEST_EXE=%SCRIPT_DIR%out\bin\x64\Release\OctoPaint.Core.Domain.Tests\OctoPaint.Core.Domain.Tests.exe"
set "TOOLS_TEST_EXE=%SCRIPT_DIR%out\bin\x64\Release\OctoPaint.Tools.Tests\OctoPaint.Tools.Tests.exe"
set "APPLICATION_LAYER_TEST_EXE=%SCRIPT_DIR%out\bin\x64\Release\OctoPaint.Application.Layer.Tests\OctoPaint.Application.Layer.Tests.exe"
set "APPLICATION_EDITOR_STATE_TEST_EXE=%SCRIPT_DIR%out\bin\x64\Release\OctoPaint.Application.EditorState.Tests\OctoPaint.Application.EditorState.Tests.exe"
set "APPLICATION_PAINT_TEST_EXE=%SCRIPT_DIR%out\bin\x64\Release\OctoPaint.Application.Paint.Tests\OctoPaint.Application.Paint.Tests.exe"
set "RELEASE_DIR=%SCRIPT_DIR%out\release"
set "STAGE_ROOT=%RELEASE_DIR%\stage"
set "STAGE_APP=%STAGE_ROOT%\OctoPaint"
set "INSTALLER_SCRIPT=%SCRIPT_DIR%installer\OctoPaint.wxs"
set "WIX_INTERMEDIATE=%RELEASE_DIR%\wixobj"

if not exist "%SOLUTION%" (call :fail "Solution not found: %SOLUTION%" & exit /b 1)
if not exist "%VERSION_FILE%" (call :fail "Version file not found: %VERSION_FILE%" & exit /b 1)

set /p "OCTOPAINT_VERSION="<"%VERSION_FILE%"
if not defined OCTOPAINT_VERSION (call :fail "VERSION must contain a version number." & exit /b 1)
set "OCTOPAINT_VERSION_FILE=%VERSION_FILE%"
powershell.exe -NoLogo -NoProfile -NonInteractive -Command "$v = [IO.File]::ReadAllText($env:OCTOPAINT_VERSION_FILE).Trim(); if ($v -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { exit 1 }" >nul 2>nul
if errorlevel 1 (call :fail "VERSION must contain a SemVer version in major.minor.patch form, for example 1.2.3." & exit /b 1)
if not exist "%INSTALLER_SCRIPT%" (call :fail "Installer definition not found: %INSTALLER_SCRIPT%" & exit /b 1)

set "ZIP_FILE=%RELEASE_DIR%\OctoPaint-%OCTOPAINT_VERSION%-win-x64.zip"
set "MSI_FILE=%RELEASE_DIR%\OctoPaint-%OCTOPAINT_VERSION%-win-x64.msi"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    where vswhere.exe >nul 2>nul
    if errorlevel 1 (call :fail "vswhere.exe was not found. Install Visual Studio 2022 with Desktop development with C++." & exit /b 1)
    set "VSWHERE=vswhere.exe"
)

set "MSBUILD="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%I"
if not defined MSBUILD (call :fail "MSBuild was not found by vswhere. Install the Visual Studio C++ build tools." & exit /b 1)

set "WIX="
for /f "delims=" %%I in ('where wix.exe 2^>nul') do if not defined WIX set "WIX=%%I"
if not defined WIX if exist "%USERPROFILE%\.dotnet\tools\wix.exe" set "WIX=%USERPROFILE%\.dotnet\tools\wix.exe"
if not defined WIX if exist "%ProgramFiles%\WiX Toolset v7\bin\wix.exe" set "WIX=%ProgramFiles%\WiX Toolset v7\bin\wix.exe"
if not defined WIX if exist "%ProgramFiles%\WiX Toolset v6\bin\wix.exe" set "WIX=%ProgramFiles%\WiX Toolset v6\bin\wix.exe"
if not defined WIX if exist "%ProgramFiles%\WiX Toolset v5\bin\wix.exe" set "WIX=%ProgramFiles%\WiX Toolset v5\bin\wix.exe"
if not defined WIX (call :fail "WiX Toolset 5 or newer was not found. Install the official tool with: dotnet tool install --global wix  (https://docs.firegiant.com/wix/using-wix/)" & exit /b 1)

set "WIX_MAJOR="
for /f "tokens=1 delims=." %%V in ('"%WIX%" --version 2^>nul') do if not defined WIX_MAJOR set "WIX_MAJOR=%%V"
if not defined WIX_MAJOR (call :fail "Could not determine the installed WiX Toolset version: %WIX%" & exit /b 1)
set /a WIX_MAJOR_NUMBER=WIX_MAJOR >nul 2>nul
if errorlevel 1 (call :fail "The installed WiX Toolset returned an invalid version: %WIX_MAJOR%" & exit /b 1)
if %WIX_MAJOR_NUMBER% LSS 5 (call :fail "WiX Toolset 5 or newer is required. Update it with: dotnet tool update --global wix" & exit /b 1)

where tar.exe >nul 2>nul
if errorlevel 1 (call :fail "tar.exe was not found. A supported Windows 10 or Windows 11 installation is required." & exit /b 1)

echo [1/6] Restoring dependencies...
"%MSBUILD%" "%SOLUTION%" /nologo /m /nr:false /t:Restore /p:Configuration=Release /p:Platform=x64 /p:RuntimeIdentifier=win10-x64 /p:WindowsAppSDKSelfContained=true
if errorlevel 1 (call :fail "Dependency restore failed." & exit /b 1)

echo [2/6] Building OctoPaint Release x64...
"%MSBUILD%" "%SOLUTION%" /nologo /m /nr:false /t:Build /p:Configuration=Release /p:Platform=x64 /p:RuntimeIdentifier=win10-x64 /p:WindowsAppSDKSelfContained=true
if errorlevel 1 (call :fail "Release build failed." & exit /b 1)

echo [3/6] Running headless tests...
if not exist "%TEST_EXE%" (call :fail "Test executable was not produced: %TEST_EXE%" & exit /b 1)
"%TEST_EXE%"
if errorlevel 1 (call :fail "OctoPaint.Core.Tests failed." & exit /b 1)
if not exist "%APPLICATION_TEST_EXE%" (call :fail "Test executable was not produced: %APPLICATION_TEST_EXE%" & exit /b 1)
"%APPLICATION_TEST_EXE%"
if errorlevel 1 (call :fail "OctoPaint.Application.Tests failed." & exit /b 1)
if not exist "%DOMAIN_TEST_EXE%" (call :fail "Test executable was not produced: %DOMAIN_TEST_EXE%" & exit /b 1)
"%DOMAIN_TEST_EXE%"
if errorlevel 1 (call :fail "OctoPaint.Core.Domain.Tests failed." & exit /b 1)
if not exist "%TOOLS_TEST_EXE%" (call :fail "Test executable was not produced: %TOOLS_TEST_EXE%" & exit /b 1)
"%TOOLS_TEST_EXE%"
if errorlevel 1 (call :fail "OctoPaint.Tools.Tests failed." & exit /b 1)
if not exist "%APPLICATION_LAYER_TEST_EXE%" (call :fail "Test executable was not produced: %APPLICATION_LAYER_TEST_EXE%" & exit /b 1)
"%APPLICATION_LAYER_TEST_EXE%"
if errorlevel 1 (call :fail "OctoPaint.Application.Layer.Tests failed." & exit /b 1)
if not exist "%APPLICATION_EDITOR_STATE_TEST_EXE%" (call :fail "Test executable was not produced: %APPLICATION_EDITOR_STATE_TEST_EXE%" & exit /b 1)
"%APPLICATION_EDITOR_STATE_TEST_EXE%"
if errorlevel 1 (call :fail "OctoPaint.Application.EditorState.Tests failed." & exit /b 1)
if not exist "%APPLICATION_PAINT_TEST_EXE%" (call :fail "Test executable was not produced: %APPLICATION_PAINT_TEST_EXE%" & exit /b 1)
"%APPLICATION_PAINT_TEST_EXE%"
if errorlevel 1 (call :fail "OctoPaint.Application.Paint.Tests failed." & exit /b 1)

echo [4/6] Staging runtime files...
if not exist "%APP_OUTPUT%\OctoPaint.exe" (call :fail "Application executable was not produced: %APP_OUTPUT%\OctoPaint.exe" & exit /b 1)
if exist "%STAGE_ROOT%" (
    rmdir /s /q "%STAGE_ROOT%"
    if errorlevel 1 (call :fail "Could not clean the staging directory: %STAGE_ROOT%" & exit /b 1)
)
if not exist "%RELEASE_DIR%" (
    mkdir "%RELEASE_DIR%"
    if errorlevel 1 (call :fail "Could not create the release directory: %RELEASE_DIR%" & exit /b 1)
)

robocopy "%APP_OUTPUT%" "%STAGE_APP%" /E /R:2 /W:1 /NFL /NDL /NJH /NJS /NP /XF *.pdb *.ilk *.lib *.exp *.iobj *.ipdb *.obj /XD obj >nul
if errorlevel 8 (call :fail "Failed to stage application runtime files." & exit /b 1)
if not exist "%STAGE_APP%\OctoPaint.exe" (call :fail "Staging did not copy OctoPaint.exe." & exit /b 1)
ver >nul

echo [5/6] Creating %ZIP_FILE%...
if exist "%ZIP_FILE%" (
    del /q "%ZIP_FILE%"
    if errorlevel 1 (call :fail "Could not replace the existing release archive: %ZIP_FILE%" & exit /b 1)
)

pushd "%STAGE_ROOT%"
if errorlevel 1 (call :fail "Could not enter the staging directory: %STAGE_ROOT%" & exit /b 1)
tar.exe -a -c -f "%ZIP_FILE%" "OctoPaint"
set "ARCHIVE_EXIT=%ERRORLEVEL%"
popd
if not "%ARCHIVE_EXIT%"=="0" (call :fail "Failed to create the release archive." & exit /b 1)
if not exist "%ZIP_FILE%" (call :fail "Release archive was not produced: %ZIP_FILE%" & exit /b 1)

echo [6/6] Creating %MSI_FILE%...
if exist "%MSI_FILE%" (
    del /q "%MSI_FILE%"
    if errorlevel 1 (call :fail "Could not replace the existing installer: %MSI_FILE%" & exit /b 1)
)
if exist "%WIX_INTERMEDIATE%" (
    rmdir /s /q "%WIX_INTERMEDIATE%"
    if errorlevel 1 (call :fail "Could not clean the WiX intermediate directory: %WIX_INTERMEDIATE%" & exit /b 1)
)
"%WIX%" build -arch x64 -d "AppVersion=%OCTOPAINT_VERSION%" -b "Payload=%STAGE_APP%" -intermediateFolder "%WIX_INTERMEDIATE%" -pdbtype none -dcl high -o "%MSI_FILE%" "%INSTALLER_SCRIPT%"
if errorlevel 1 (call :fail "WiX Toolset failed to create the MSI installer." & exit /b 1)
if not exist "%MSI_FILE%" (call :fail "Installer was not produced: %MSI_FILE%" & exit /b 1)

echo Release packages created successfully:
echo   %ZIP_FILE%
echo   %MSI_FILE%
exit /b 0

:fail
echo ERROR: %~1 1>&2
exit /b 1
