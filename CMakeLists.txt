cmake_minimum_required(VERSION 3.15)
project(CloudStreamingArgsDebugger LANGUAGES CXX)

# Specify that this is a Win32 application with wWinMain entry point
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# Header paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Define build output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/build)

# Build main executable
add_executable(cloud-streaming-args-debugger
    WIN32                        # Specify that the application uses WinMain instead of main
    cli_args_debugger.cpp
    seh_wrapper.cpp
    qrcodegen.cpp                # Include QR code generator
)

# Link required libraries
target_link_libraries(cloud-streaming-args-debugger PRIVATE
    d3d11
    d3dcompiler
    d2d1
    dwrite
    ole32
    avrt
    user32
    shell32
    gdi32
    propsys
    winmm
    psapi
)

# Windows-specific compiler options
if(MSVC)
    # Use /EHsc for main file (structured exception handling for C++, SEH exceptions aren't automatically caught)
    target_compile_options(cloud-streaming-args-debugger PRIVATE "/EHsc")
    
    # Explicitly specify UNICODE
    target_compile_definitions(cloud-streaming-args-debugger PRIVATE 
        "UNICODE" 
        "_UNICODE"
    )
endif()

# Installation (optional)
install(TARGETS cloud-streaming-args-debugger DESTINATION bin)

# Tests
enable_testing()
add_subdirectory(tests)