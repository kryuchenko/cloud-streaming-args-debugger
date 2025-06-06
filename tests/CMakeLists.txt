cmake_minimum_required(VERSION 3.10)
project(ArgumentDebuggerTests LANGUAGES CXX)

# Explicitly specify that tests are console applications
set(CMAKE_WIN32_EXECUTABLE OFF)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GoogleTest package
find_package(GTest CONFIG REQUIRED)

# Define output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../build/tests)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/../build/tests)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/../build/tests)

# Define test sources
set(TEST_SOURCES
    cli_args_tests.cpp
    path_utils_tests.cpp
    qr_code_tests.cpp
    audio_tests.cpp
    logging_tests.cpp
    data_persistence_tests.cpp
    string_conversion_tests.cpp
    seh_exception_tests.cpp
    fps_calculation_tests.cpp
)

# Add source files from parent directory that contain functions we're testing
set(PARENT_SOURCES
    ../qrcodegen.cpp
    ../cli_args_debugger.cpp
    ../seh_wrapper.cpp
)

# Define test executable as console application (without WIN32 flag)
add_executable(cli_args_tests ${TEST_SOURCES} ${PARENT_SOURCES})

# Link with GoogleTest and Windows libraries
target_link_libraries(cli_args_tests PRIVATE 
    GTest::gtest 
    GTest::gtest_main
    shell32  # For SHGetKnownFolderPath
    ole32    # For COM functions
    d3d11    # For DirectX
    d2d1     # For Direct2D
    dwrite   # For DirectWrite
    dxgi     # For DXGI
    avrt     # For AvSetMmThreadCharacteristics
)

# Windows-specific compiler options
if(MSVC)
    target_compile_options(cli_args_tests PRIVATE "/EHsc")
    
    # Explicitly specify UNICODE and exclude main from cli_args_debugger.cpp
    target_compile_definitions(cli_args_tests PRIVATE 
        "UNICODE" 
        "_UNICODE"
        "EXCLUDE_MAIN"  # To exclude wWinMain from cli_args_debugger.cpp
    )
    
    # Explicitly specify console subsystem for the linker
    set_target_properties(cli_args_tests PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

# Set include directories to include the parent directory
target_include_directories(cli_args_tests PRIVATE ".." ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Register the test with CTest
add_test(NAME CliArgsTests COMMAND cli_args_tests)