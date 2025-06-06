@echo off
echo Validating C++ syntax...

REM Compile with syntax checking only (/Zs flag)
cl /EHsc /std:c++20 /permissive- /Zc:__cplusplus /Zs /W4 /I. /DUNICODE /D_UNICODE cli_args_debugger.cpp
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Syntax errors found in cli_args_debugger.cpp
    exit /b 1
)

cl /EHsc /std:c++20 /permissive- /Zc:__cplusplus /Zs /W4 /I. /DUNICODE /D_UNICODE seh_wrapper.cpp
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Syntax errors found in seh_wrapper.cpp
    exit /b 1
)

echo.
echo All syntax checks passed!

REM Optional: Check with clang-tidy if available
where clang-tidy >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Running clang-tidy...
    clang-tidy cli_args_debugger.cpp -- -std=c++20 -DUNICODE -D_UNICODE -I.
    clang-tidy seh_wrapper.cpp -- -std=c++20 -DUNICODE -D_UNICODE -I.
) else (
    echo.
    echo clang-tidy not found. Install LLVM for additional checks.
)

echo.
echo Validation complete!