// seh_wrapper.cpp
#include <windows.h>
#include <stdio.h>  // For swprintf_s

// Forward declaration of the class with explicit method we need to access
class ArgumentDebuggerWindow {
public:
    // Only declare the method we're calling from here
    DWORD AudioCaptureThreadImpl(LPVOID param);
};

// Log function declarations - simplified version for SEH context
extern void LogSEH(const wchar_t* message);

// Note: WINAPI is defined as __stdcall for compatibility with Windows API types
extern "C" DWORD WINAPI RawAudioThreadWithSEH(LPVOID param) noexcept {
    DWORD exitCode = 0;
    __try {
        // Forward to the real implementation
        ArgumentDebuggerWindow* self = static_cast<ArgumentDebuggerWindow*>(param);
        exitCode = self->AudioCaptureThreadImpl(param);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DWORD code = GetExceptionCode();
        // Log through OutputDebugString to avoid std::wstring
        wchar_t buf[128];
        
        if (code == EXCEPTION_ACCESS_VIOLATION) {
            OutputDebugStringW(L"SEH: Access violation in audio thread (0xC0000005)\n");
            LogSEH(L"SEH: Access violation in audio thread (0xC0000005)");
            exitCode = 0xC0000005;  // EXCEPTION_ACCESS_VIOLATION
        } else if (code == EXCEPTION_STACK_OVERFLOW) {
            OutputDebugStringW(L"SEH: Stack overflow in audio thread (0xC00000FD)\n");
            LogSEH(L"SEH: Stack overflow in audio thread (0xC00000FD)");
            exitCode = 0xC00000FD;  // EXCEPTION_STACK_OVERFLOW
        } else {
            // Use wsprintfW instead of swprintf_s for better backward compatibility
            wsprintfW(buf, L"SEH: Exception in audio thread, code=0x%08X", code);
            OutputDebugStringW(buf);
            OutputDebugStringW(L"\n");
            LogSEH(buf);
            exitCode = code;
        }
    }
    return exitCode;
}