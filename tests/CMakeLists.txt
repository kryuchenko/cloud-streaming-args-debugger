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
)

# Add qrcodegen source files from parent directory
set(QRCODEGEN_SOURCES
    ../qrcodegen.cpp
)

# Define test executable as console application (without WIN32 flag)
add_executable(cli_args_tests ${TEST_SOURCES} ${QRCODEGEN_SOURCES})

# Link with GoogleTest and Windows libraries
target_link_libraries(cli_args_tests PRIVATE 
    GTest::gtest 
    GTest::gtest_main
    shell32  # For SHGetKnownFolderPath
    ole32    # For COM functions
)

# Windows-specific compiler options
if(MSVC)
    target_compile_options(cli_args_tests PRIVATE "/EHsc")
    
    # Explicitly specify UNICODE
    target_compile_definitions(cli_args_tests PRIVATE 
        "UNICODE" 
        "_UNICODE"
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