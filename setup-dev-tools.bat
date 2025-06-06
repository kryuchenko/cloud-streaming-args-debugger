@echo off
echo Setting up development tools...

REM Install chocolatey if not present
where choco >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Installing Chocolatey...
    powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
    refreshenv
)

REM Install development tools
echo Installing LLVM (includes clang-format, clang-tidy)...
choco install llvm -y

echo Installing pre-commit...
pip install pre-commit

echo Setting up pre-commit hooks...
pre-commit install

echo.
echo Development tools setup complete!
echo.
echo Available commands:
echo   validate.bat          - Run syntax validation
echo   clang-format *.cpp    - Format code
echo   pre-commit run --all  - Run all checks
echo.
echo VS Code will automatically use these tools if you have the C++ extension installed.