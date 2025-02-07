# CLI Arguments Debugger

This is a simple Windows application that shows your command-line arguments in a full-screen window. It also displays a rotating 3D cube and a QR code that updates with the current timestamp, FPS, and the arguments you passed.

## What It Does

- **Displays Command-Line Arguments:** Shows any arguments you pass to the program.
- **3D Cube Animation:** Renders a rotating cube using Direct3D 11.
- **QR Code:** Generates and displays a QR code with the current UNIX time, FPS, and your arguments (updates every 5 seconds).
- **Keyboard Input:** Type into the window and if you type `exit` (or press Escape), the app will close.

## How to Build

1. **Requirements:**
   - **Visual Studio 2022:** Install the "Desktop development with C++" and "Universal Windows Platform development" workloads.
   - **Windows SDK:** Latest version is recommended.
   - **DirectX:** Use either the DirectX SDK (June 2010) or a modern Windows SDK.
   - **QrCodeGen Library:** Download `qrcodegen.hpp` and `qrcodegen.cpp` from [QrCodeGen by Nayuki](https://github.com/nayuki/QR-Code-generator) and place them in the same folder as the source file.

2. **Setup:**
   - Create a new C++ project in Visual Studio.
   - Add `cli_args_debugger.cpp`, `qrcodegen.hpp`, and `qrcodegen.cpp` to your project.
   - Build the project (the code uses pragma directives to link the required DirectX libraries).

## How to Run

- Run the compiled executable from a command prompt with any arguments you want:
  ```bat
  cli_args_debugger.exe arg1 arg2 "another argument"
