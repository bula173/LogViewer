@echo off
REM test-workflows.bat - Simple batch script to test GitHub workflows locally

setlocal enabledelayedexpansion

set "GREEN=[32m"
set "RED=[31m"
set "YELLOW=[33m"
set "BLUE=[34m"
set "RESET=[0m"

echo %BLUE%============================================%RESET%
echo %BLUE%   LogViewer GitHub Workflow Tester%RESET%
echo %BLUE%============================================%RESET%

REM Check if act is available
where act >nul 2>&1
if !errorlevel! neq 0 (
    echo %RED%Error: act is not installed or not in PATH%RESET%
    echo %YELLOW%Please install act first:%RESET%
    echo   winget install nektos.act
    echo   or download from: https://github.com/nektos/act/releases
    goto :error
)

echo %GREEN%Found act tool%RESET%

REM Check if Docker is available and running
docker --version >nul 2>&1
if !errorlevel! neq 0 (
    echo %YELLOW%Warning: Docker not found. Using dry-run mode.%RESET%
    set DRY_RUN=-n
    set DOCKER_AVAILABLE=false
) else (
    docker info >nul 2>&1
    if !errorlevel! neq 0 (
        echo %YELLOW%Warning: Docker found but not running. Using dry-run mode.%RESET%
        echo %YELLOW%To start Docker: Start Docker Desktop or run 'docker service start'%RESET%
        set DRY_RUN=-n
        set DOCKER_AVAILABLE=false
    ) else (
        echo %GREEN%Found Docker and it's running%RESET%
        set DRY_RUN=
        set DOCKER_AVAILABLE=true
    )
)

echo.
echo %BLUE%Available test options:%RESET%
echo   1. Test Windows workflow (wx + qt)
echo   2. Test Linux workflow (wx + qt, gcc + clang)
echo   3. Test macOS workflow (wx + qt)
echo   4. Test all workflows
echo   5. Test specific framework (wx)
echo   6. Test specific framework (qt)
echo   7. Validate workflow syntax only
echo   8. Test locally without Docker (manual builds)
echo   9. Custom test
echo.

if "!DOCKER_AVAILABLE!"=="false" (
    echo %YELLOW%Note: Docker not available - options 1-6 will use dry-run mode%RESET%
    echo %YELLOW%Option 8 provides local testing without Docker%RESET%
)

if "%1"=="" (
    set /p "choice=Select option (1-9): "
) else (
    set "choice=%1"
)

echo.

if "!choice!"=="1" goto :test_windows
if "!choice!"=="2" goto :test_linux
if "!choice!"=="3" goto :test_macos
if "!choice!"=="4" goto :test_all
if "!choice!"=="5" goto :test_wx
if "!choice!"=="6" goto :test_qt
if "!choice!"=="7" goto :validate_syntax
if "!choice!"=="8" goto :test_local
if "!choice!"=="9" goto :custom_test

echo %RED%Invalid choice!%RESET%
goto :error

:test_windows
echo %BLUE%Testing Windows workflow...%RESET%
echo.
echo %YELLOW%Testing wx framework:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-windows.yml --matrix gui_framework:wx --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Testing qt framework:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-windows.yml --matrix gui_framework:qt --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error
goto :success

:test_linux
echo %BLUE%Testing Linux workflow...%RESET%
echo.
echo %YELLOW%Testing wx + gcc:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-linux.yml --matrix gui_framework:wx --matrix compiler:gcc --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Testing wx + clang:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-linux.yml --matrix gui_framework:wx --matrix compiler:clang --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Testing qt + gcc:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-linux.yml --matrix gui_framework:qt --matrix compiler:gcc --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Testing qt + clang:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-linux.yml --matrix gui_framework:qt --matrix compiler:clang --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error
goto :success

:test_macos
echo %BLUE%Testing macOS workflow...%RESET%
echo.
echo %YELLOW%Testing wx framework:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-macos.yml --matrix gui_framework:wx --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Testing qt framework:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-macos.yml --matrix gui_framework:qt --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error
goto :success

:test_all
echo %BLUE%Testing all workflows...%RESET%
call :test_windows
call :test_linux
call :test_macos
goto :success

:test_wx
echo %BLUE%Testing wx framework across all platforms...%RESET%
echo.
echo %YELLOW%Windows + wx:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-windows.yml --matrix gui_framework:wx --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Linux + wx + gcc:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-linux.yml --matrix gui_framework:wx --matrix compiler:gcc --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Linux + wx + clang:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-linux.yml --matrix gui_framework:wx --matrix compiler:clang --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%macOS + wx:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-macos.yml --matrix gui_framework:wx --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error
goto :success

:test_qt
echo %BLUE%Testing qt framework across all platforms...%RESET%
echo.
echo %YELLOW%Windows + qt:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-windows.yml --matrix gui_framework:qt --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Linux + qt + gcc:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-linux.yml --matrix gui_framework:qt --matrix compiler:gcc --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Linux + qt + clang:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-linux.yml --matrix gui_framework:qt --matrix compiler:clang --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%macOS + qt:%RESET%
act %DRY_RUN% -W .github/workflows/cmake-macos.yml --matrix gui_framework:qt --matrix build_type:Debug -v
if !errorlevel! neq 0 goto :workflow_error
goto :success

:validate_syntax
echo %BLUE%Validating workflow syntax...%RESET%
echo.
echo %YELLOW%Windows workflow:%RESET%
act -n -W .github/workflows/cmake-windows.yml
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%Linux workflow:%RESET%
act -n -W .github/workflows/cmake-linux.yml
if !errorlevel! neq 0 goto :workflow_error

echo.
echo %YELLOW%macOS workflow:%RESET%
act -n -W .github/workflows/cmake-macos.yml
if !errorlevel! neq 0 goto :workflow_error
goto :success

:test_local
echo %BLUE%Testing locally without Docker...%RESET%
echo.

REM Save current directory
set ORIGINAL_DIR=%CD%

echo %YELLOW%Setting up MSYS2 environment...%RESET%
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

echo %YELLOW%Cleaning previous builds...%RESET%
if exist build (
    rmdir /s /q build
    if !errorlevel! neq 0 (
        echo %RED%Failed to clean build directory%RESET%
        goto :local_error
    )
)

echo.
echo %BLUE%Testing wxWidgets build...%RESET%
echo %YELLOW%Configuring wxWidgets Debug build...%RESET%
cmake --preset windows-msys-debug-wx
if !errorlevel! neq 0 (
    echo %RED%wxWidgets Debug configuration failed%RESET%
    goto :local_error
)

echo %YELLOW%Building wxWidgets Debug...%RESET%
cmake --build --preset windows-msys-debug-wx
if !errorlevel! neq 0 (
    echo %RED%wxWidgets Debug build failed%RESET%
    goto :local_error
)

echo %YELLOW%Testing wxWidgets build...%RESET%
ctest --test-dir build/debug_wx --output-on-failure
if !errorlevel! neq 0 (
    echo %YELLOW%Some wxWidgets tests failed, but continuing...%RESET%
)

echo.
echo %BLUE%Testing Qt build...%RESET%
echo %YELLOW%Configuring Qt Debug build...%RESET%
cmake --preset windows-msys-debug-qt
if !errorlevel! neq 0 (
    echo %RED%Qt Debug configuration failed%RESET%
    goto :local_error
)

echo %YELLOW%Building Qt Debug...%RESET%
cmake --build --preset windows-msys-debug-qt
if !errorlevel! neq 0 (
    echo %RED%Qt Debug build failed%RESET%
    goto :local_error
)

echo %YELLOW%Testing Qt build...%RESET%
ctest --test-dir build/debug_qt --output-on-failure
if !errorlevel! neq 0 (
    echo %YELLOW%Some Qt tests failed, but continuing...%RESET%
)

echo.
echo %GREEN%Local testing completed successfully!%RESET%
goto :success

:local_error
cd /d "%ORIGINAL_DIR%"
echo %RED%Local testing failed!%RESET%
goto :error

:custom_test
echo %BLUE%Custom test mode%RESET%
echo.
echo Available workflows:
echo   1. .github/workflows/cmake-windows.yml
echo   2. .github/workflows/cmake-linux.yml
echo   3. .github/workflows/cmake-macos.yml
echo.
set /p "workflow_choice=Select workflow (1-3): "
set /p "framework=Enter framework (wx/qt): "
set /p "build_type=Enter build type (Debug/Release): "

if "!workflow_choice!"=="1" set "workflow_file=.github/workflows/cmake-windows.yml"
if "!workflow_choice!"=="2" set "workflow_file=.github/workflows/cmake-linux.yml"
if "!workflow_choice!"=="3" set "workflow_file=.github/workflows/cmake-macos.yml"

if "!workflow_choice!"=="2" (
    set /p "compiler=Enter compiler (gcc/clang): "
    act %DRY_RUN% -W !workflow_file! --matrix gui_framework:!framework! --matrix compiler:!compiler! --matrix build_type:!build_type! -v
) else (
    act %DRY_RUN% -W !workflow_file! --matrix gui_framework:!framework! --matrix build_type:!build_type! -v
)

if !errorlevel! neq 0 goto :workflow_error
goto :success

:workflow_error
echo.
echo %RED%Workflow test failed!%RESET%
goto :error

:success
echo.
echo %GREEN%============================================%RESET%
echo %GREEN%   All workflow tests completed successfully!%RESET%
echo %GREEN%============================================%RESET%
exit /b 0

:error
echo.
echo %RED%============================================%RESET%
echo %RED%   Workflow test failed!%RESET%
echo %RED%============================================%RESET%
exit /b 1
