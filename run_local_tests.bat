@echo off
echo Building and running tests locally...
echo.

:: Create build directory if it doesn't exist
if not exist "build\tests" mkdir "build\tests"

:: Navigate to build directory
cd build

:: Configure with CMake
echo Configuring project...
cmake -DCMAKE_BUILD_TYPE=Debug ..
if errorlevel 1 goto error

:: Build tests
echo.
echo Building tests...
cmake --build . --config Debug --target cli_args_tests
if errorlevel 1 goto error

:: Run tests
echo.
echo Running tests...
cd tests
cli_args_tests.exe
if errorlevel 1 goto error

echo.
echo All tests completed successfully!
cd ..\..
exit /b 0

:error
echo.
echo ERROR: Tests failed!
cd ..\..
exit /b 1