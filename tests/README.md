# Unit Tests for DirectX Args Debugger

This directory contains comprehensive unit tests for the DirectX Args Debugger application.

## Test Categories

1. **cli_args_tests.cpp** - Tests for command line argument formatting and display
   - Empty arguments handling
   - Single and multiple arguments
   - Arguments with spaces and special characters
   - Unicode argument support
   - Path formatting with quotes

2. **path_utils_tests.cpp** - Tests for path and system information utilities
   - Executable path retrieval
   - Current directory detection
   - Windows system directories
   - AppData and temp paths
   - Relative path calculation
   - OS version detection
   - Wine/Proton detection

3. **qr_code_tests.cpp** - Tests for QR code generation
   - Basic QR code generation
   - Data format validation
   - Unicode data handling
   - Bitmap scaling
   - Update frequency logic
   - FPS synchronization

4. **audio_tests.cpp** - Tests for audio capture functionality
   - COM initialization
   - Audio device enumeration
   - Audio level calculations (16/24/32-bit PCM, float)
   - Level smoothing algorithm
   - Stereo visualization
   - Thread safety with atomics
   - Event handle operations

## Running Tests Locally

### Using the provided batch file:
```batch
run_local_tests.bat
```

### Manual build and run:
```batch
cd build
cmake ..
cmake --build . --config Debug --target cli_args_tests
cd tests
cli_args_tests.exe
```

### Using CMake and vcpkg:
```bash
# Configure the build
cmake -B build -S .. -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake

# Build the tests
cmake --build build --config Release

# Run the tests
cd build
ctest -C Release -V
```

## Requirements

- Visual Studio 2022
- CMake 3.10 or higher
- vcpkg with gtest package installed
- Windows SDK

## Adding New Tests

To add new tests, follow the pattern in the existing tests:

1. Create a new TEST_F case with appropriate fixture
2. Define input data and expected outputs
3. Call the functions being tested
4. Use appropriate EXPECT_* macros to validate results

## CI Integration

Tests are automatically run as part of the GitHub Actions CI pipeline on every push and pull request.