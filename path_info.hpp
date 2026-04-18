#pragma once

#include <string>
#include <utility>
#include <vector>

// `label` keeps a trailing space so the UI can draw the value immediately
// after it. Using a plain pair keeps this module drop-in compatible with the
// previous `cached_path_items_` field, which already had this type.
using PathItem = std::pair<std::wstring, std::wstring>;

// The namespace guards against Windows.h macros such as `GetWindowsDirectory`
// and `GetSystemDirectory` rewriting these names into their W/A variants.
namespace path_info
{

// Collect executable path, working dir, OS version, Wine/Proton marker, TEMP,
// Windows, System and save-file paths. All Win32 calls are sized from their
// own return values, so paths longer than MAX_PATH are not silently truncated.
std::vector<PathItem> Collect();

// Helpers exposed for unit tests and for callers that need only one value.
std::wstring ExecutablePath();
std::wstring CurrentWorkingDirectory();
std::wstring TempDirectory();
std::wstring WindowsDirectory();
std::wstring SystemDirectory();
std::wstring OsVersionString();
std::wstring WineOrProtonVersion();
std::wstring SaveFilePath();

} // namespace path_info
