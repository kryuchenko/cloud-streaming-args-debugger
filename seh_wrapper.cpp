// seh_wrapper.cpp
// Turns SEH faults in the audio capture thread (e.g. device disconnect during
// GetBuffer) into a logged termination, instead of letting them propagate as
// an unhandled exception and crashing the process.
//
// Deliberately kept in its own translation unit: /EHsc does not let a C++
// function that uses __try / __except also have a destructor-bearing local,
// so we isolate the SEH boundary here.

#include <stdio.h>
#include <windows.h>

// Forward declare just the interface we call — we do not need the full
// AudioCapture definition here, only the `this` pointer plus one method.
class AudioCapture
{
  public:
    DWORD ThreadMain();
};

// Log function exported from log_manager.cpp (see log_manager.hpp).
extern void LogSEH(const wchar_t* message);

extern "C" DWORD WINAPI RawAudioThreadWithSEH(LPVOID param) noexcept
{
    DWORD exitCode = 0;
    __try
    {
        if (!param)
            return 0xC0000005; // EXCEPTION_ACCESS_VIOLATION

        auto* self = static_cast<AudioCapture*>(param);
        exitCode = self->ThreadMain();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DWORD code = GetExceptionCode();
        wchar_t buf[128];
        if (code == EXCEPTION_ACCESS_VIOLATION)
        {
            OutputDebugStringW(L"SEH: Access violation in audio thread (0xC0000005)\n");
            LogSEH(L"SEH: Access violation in audio thread (0xC0000005)");
            exitCode = 0xC0000005;
        }
        else if (code == EXCEPTION_STACK_OVERFLOW)
        {
            OutputDebugStringW(L"SEH: Stack overflow in audio thread (0xC00000FD)\n");
            LogSEH(L"SEH: Stack overflow in audio thread (0xC00000FD)");
            exitCode = 0xC00000FD;
        }
        else
        {
            wsprintfW(buf, L"SEH: Exception in audio thread, code=0x%08X", code);
            OutputDebugStringW(buf);
            OutputDebugStringW(L"\n");
            LogSEH(buf);
            exitCode = code;
        }
    }
    return exitCode;
}
