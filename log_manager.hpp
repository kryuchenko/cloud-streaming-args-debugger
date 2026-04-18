#pragma once

#include <cstdio>
#include <string>
#include <windows.h>

// UTF-16LE log file written next to the executable (or to %APPDATA%\CloudStreamingArgsDebugger\).
//
//   InitLogger()     - call once from wWinMain after COM is initialized.
//   Log(L"...")      - append one line prefixed with local time, thread-safe.
//   LogSEH(...)      - thin wrapper used from the SEH-guarded audio thread.
//   CloseLogger()    - flush, close the file, and release the critical section.
//   GetLogPath()     - path to the log file (empty before InitLogger()).
//
// The raw globals `g_log_file` and `g_logPath` remain visible with extern
// linkage so that existing unit tests (tests/logging_tests.cpp) keep compiling.

extern FILE* g_log_file;
extern std::wstring g_logPath;

void InitLogger();
void Log(const std::wstring& text);
void LogSEH(const wchar_t* message);
void CloseLogger();

inline const std::wstring& GetLogPath()
{
    return g_logPath;
}
inline FILE* GetLogFile()
{
    return g_log_file;
}
