@echo off
setlocal

:: Create required directories
mkdir build\tests 2>nul
mkdir obj 2>nul

:: Compile the tests
cl /EHsc /std:c++20 /permissive- /I. /DUNICODE /D_UNICODE ^
    tests.cpp ^
    /Fe:build\tests\cli_args_tests.exe ^
    /Fo:obj\

:: Run the tests
echo.
echo Running tests...
echo.
build\tests\cli_args_tests.exe

:: Check result
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Tests failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
) else (
    echo.
    echo All tests passed!
)

endlocal