#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

// clang-format off
// <windows.h> must come first; <knownfolders.h> and <winternl.h> rely on
// EXTERN_C / GUID being already defined.
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <winternl.h>
// clang-format on

#include "path_info.hpp"

#pragma comment(lib, "shell32")
#pragma comment(lib, "ole32")

namespace
{

// Generic retry pattern for Win32 "fill-a-buffer" APIs that either write
// `len < size` on success or report `ERROR_INSUFFICIENT_BUFFER` when the
// buffer is too small. Avoids MAX_PATH truncation silently.
template <typename Fill> std::wstring FillWithGrowingBuffer(Fill fill)
{
    std::wstring buf;
    buf.resize(MAX_PATH);
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        SetLastError(ERROR_SUCCESS);
        DWORD len = fill(&buf[0], static_cast<DWORD>(buf.size()));
        if (len == 0)
            return {};
        if (len < buf.size())
        {
            buf.resize(len);
            return buf;
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            buf.resize(len);
            return buf;
        }
        buf.resize(buf.size() * 2);
    }
    return {};
}

// For APIs whose first form takes (size=0, buf=nullptr) and returns the
// required character count including the terminator.
template <typename Query> std::wstring FillWithQueriedSize(Query query)
{
    DWORD needed = query(0, nullptr);
    if (!needed)
        return {};
    std::wstring buf(needed, L'\0');
    DWORD got = query(needed, &buf[0]);
    if (got == 0 || got > needed)
        return {};
    buf.resize(got);
    return buf;
}

} // namespace

namespace path_info
{

std::wstring ExecutablePath()
{
    return FillWithGrowingBuffer([](wchar_t* p, DWORD n) { return GetModuleFileNameW(nullptr, p, n); });
}

std::wstring CurrentWorkingDirectory()
{
    return FillWithQueriedSize([](DWORD n, wchar_t* p) { return GetCurrentDirectoryW(n, p); });
}

std::wstring TempDirectory()
{
    std::wstring path = FillWithQueriedSize([](DWORD n, wchar_t* p) { return GetTempPathW(n, p); });
    if (!path.empty() && path.back() == L'\\')
        path.pop_back();
    return path;
}

std::wstring WindowsDirectory()
{
    return FillWithQueriedSize([](DWORD n, wchar_t* p) { return ::GetWindowsDirectoryW(p, n); });
}

std::wstring SystemDirectory()
{
    return FillWithQueriedSize([](DWORD n, wchar_t* p) { return ::GetSystemDirectoryW(p, n); });
}

std::wstring OsVersionString()
{
    using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
    // GetModuleHandleW returns a cached handle — do not call FreeLibrary on it.
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll)
        return L"Unknown";

    auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hNtdll, "RtlGetVersion"));
    if (!rtlGetVersion)
        return L"Unknown";

    RTL_OSVERSIONINFOW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (rtlGetVersion(&osvi) != 0)
        return L"Unknown";

    return L"Windows " + std::to_wstring(osvi.dwMajorVersion) + L"." + std::to_wstring(osvi.dwMinorVersion) +
           L" (Build " + std::to_wstring(osvi.dwBuildNumber) + L")";
}

std::wstring WineOrProtonVersion()
{
    using wine_get_version_func = const char* (*)(void);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll)
        return L"Not detected";

    auto wineGetVersion = reinterpret_cast<wine_get_version_func>(GetProcAddress(hNtdll, "wine_get_version"));
    if (!wineGetVersion)
        return L"Not detected";

    const char* version = wineGetVersion();
    if (!version)
        return L"Not detected";

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, version, -1, nullptr, 0);
    if (size_needed <= 1)
        return L"Not detected";

    std::wstring wversion(size_needed - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, version, -1, &wversion[0], size_needed);

    wchar_t protonBuf[1024]{};
    if (GetEnvironmentVariableW(L"PROTON_VERSION", protonBuf, 1024) > 0)
        return L"Proton " + std::wstring(protonBuf) + L" (Wine " + wversion + L")";
    return L"Wine " + wversion;
}

std::wstring SaveFilePath()
{
    PWSTR appdata_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
    if (FAILED(hr))
        return L"Not available";

    std::wstring path = appdata_path;
    CoTaskMemFree(appdata_path);
    path += L"\\CloudStreamingArgsDebugger\\saved_data.txt";
    return path;
}

std::vector<PathItem> Collect()
{
    const std::wstring fullPath = ExecutablePath();

    std::wstring exeName = fullPath;
    std::wstring exeDir = fullPath;
    const size_t lastSlash = fullPath.find_last_of(L'\\');
    if (lastSlash != std::wstring::npos)
    {
        exeName.erase(0, lastSlash + 1);
        exeDir.erase(lastSlash);
    }

    return {
        {L"OS Version: ", OsVersionString()},
        {L"Wine/Proton: ", WineOrProtonVersion()},
        {L"Executable name: ", exeName},
        {L"Full path: ", fullPath},
        {L"Executable directory: ", exeDir},
        {L"Current directory: ", CurrentWorkingDirectory()},
        {L"Command line: ", GetCommandLineW() ? std::wstring(GetCommandLineW()) : std::wstring()},
        {L"Save file path: ", SaveFilePath()},
        {L"TEMP directory: ", TempDirectory()},
        {L"Windows directory: ", WindowsDirectory()},
        {L"System directory: ", SystemDirectory()},
    };
}

} // namespace path_info
