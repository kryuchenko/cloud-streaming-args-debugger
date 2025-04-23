# GoogleTest Integration for CLI Arguments Debugger

This directory contains tests for the CLI Arguments Debugger project using Google Test framework.

## Building and Running Tests

### Using CMake and vcpkg

1. Install vcpkg and integrate it with your system (https://github.com/microsoft/vcpkg)
2. Build the project with CMake, providing the vcpkg toolchain file:

```bash
# Configure the build
cmake -B build -S .. -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake

# Build the tests
cmake --build build --config Release

# Run the tests
cd build
ctest -C Release -V
```

### Using Manual Build

If you prefer to build without CMake, you need to:

1. Install GoogleTest via vcpkg: `vcpkg install gtest`
2. Build the tests with the correct include and library paths:

```cmd
cl /EHsc /std:c++20 /permissive- /I.. /I<path_to_vcpkg>/installed/<triplet>/include ^
   /DUNICODE /D_UNICODE ^
   ../tests.cpp ^
   /Fe:../build/tests/cli_args_tests.exe ^
   /link <path_to_vcpkg>/installed/<triplet>/lib/gtest.lib ^
         <path_to_vcpkg>/installed/<triplet>/lib/gtest_main.lib
```

## Test Structure

The test suite validates the command-line argument formatting functions in `cli_args_display.hpp`:

1. `BuildCliHeaderText` - Tests whether the header text is correctly generated based on argument presence
2. `BuildCliArgsText` - Tests whether arguments are correctly formatted according to Windows conventions:
   - Arguments without spaces are not quoted
   - Arguments with spaces are wrapped in double quotes
   - Arguments are separated by spaces
   - No trailing space is added

## Adding New Tests

To add new tests, follow the pattern in the existing tests:

1. Create a new TEST_F case with the CliArgsTest fixture
2. Define a set of input arguments and expected outputs
3. Call the functions being tested
4. Use ExpectWideStringEq to compare expected and actual outputs