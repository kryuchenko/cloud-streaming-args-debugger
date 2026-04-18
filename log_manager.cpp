#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include "log_manager.hpp"

#include <knownfolders.h>
#include <share.h>
#include <shlobj.h>

#pragma comment(lib, "shell32")
#pragma comment(lib, "ole32")

FILE* g_log_file = nullptr;
std::wstring g_logPath;

namespace
{
CRITICAL_SECTION g_log_cs;
bool g_log_cs_initialized = false;
} // namespace

void InitLogger()
{
    PWSTR appdata_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
    if (SUCCEEDED(hr))
    {
        g_logPath.assign(appdata_path);
        CoTaskMemFree(appdata_path);
        g_logPath += L"\\CloudStreamingArgsDebugger\\debug.log";
        std::wstring dir = g_logPath.substr(0, g_logPath.find_last_of(L"\\"));
        CreateDirectoryW(dir.c_str(), nullptr);
    }
    else
    {
        wchar_t exePath[MAX_PATH] = L"\0";
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        g_logPath.assign(exePath);
        size_t pos = g_logPath.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            g_logPath.erase(pos + 1);
        g_logPath += L"CloudStreamingArgsDebugger.log";
    }

    g_log_file = _wfsopen(g_logPath.c_str(), L"a+, ccs=UTF-16LE", _SH_DENYNO);

    if (g_log_file && ftell(g_log_file) == 0)
    {
        fputwc(0xFEFF, g_log_file); // UTF-16LE BOM
    }

    InitializeCriticalSection(&g_log_cs);
    g_log_cs_initialized = true;
}

void Log(const std::wstring& text)
{
    if (!g_log_file || !g_log_cs_initialized)
        return;

    EnterCriticalSection(&g_log_cs);

    SYSTEMTIME st;
    GetLocalTime(&st);

    std::wstring entry = L"[";
    entry += std::to_wstring(st.wYear) + L"-";
    if (st.wMonth < 10)
        entry += L"0";
    entry += std::to_wstring(st.wMonth) + L"-";
    if (st.wDay < 10)
        entry += L"0";
    entry += std::to_wstring(st.wDay) + L" ";
    if (st.wHour < 10)
        entry += L"0";
    entry += std::to_wstring(st.wHour) + L":";
    if (st.wMinute < 10)
        entry += L"0";
    entry += std::to_wstring(st.wMinute) + L":";
    if (st.wSecond < 10)
        entry += L"0";
    entry += std::to_wstring(st.wSecond) + L"] ";

    entry += text + L"\n";

    fputws(entry.c_str(), g_log_file);
    fflush(g_log_file);

    LeaveCriticalSection(&g_log_cs);
}

void LogSEH(const wchar_t* message)
{
    if (!message)
        return;
    Log(std::wstring(message));
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

void CloseLogger()
{
    if (g_log_file)
    {
        fclose(g_log_file);
        g_log_file = nullptr;
    }
    if (g_log_cs_initialized)
    {
        DeleteCriticalSection(&g_log_cs);
        g_log_cs_initialized = false;
    }
}
