// Smoke tests for path_info:: helpers. These exercise live Win32 calls, so
// they verify the growing-buffer/queried-size patterns don't truncate against
// the actual CI runner's filesystem. They're not attempting exhaustive
// coverage — the helpers are thin wrappers — but they catch regressions in
// the "is the string non-empty / does it point at something sane" dimension.

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <string>

#include <gtest/gtest.h>

#include "../path_info.hpp"

namespace
{

bool ContainsAny(const std::wstring& s, std::initializer_list<const wchar_t*> needles)
{
    for (const wchar_t* n : needles)
    {
        if (s.find(n) != std::wstring::npos)
            return true;
    }
    return false;
}

} // namespace

TEST(PathInfo, ExecutablePathIsNonEmptyAndAbsolute)
{
    const std::wstring p = path_info::ExecutablePath();
    ASSERT_FALSE(p.empty());
    // Windows absolute paths start with a drive letter + colon or a UNC
    // prefix. Accept either.
    EXPECT_TRUE((p.size() >= 3 && p[1] == L':' && (p[2] == L'\\' || p[2] == L'/')) ||
                (p.size() >= 2 && p[0] == L'\\' && p[1] == L'\\'))
        << "not absolute: " << std::string(p.begin(), p.end());
}

TEST(PathInfo, ExecutablePathEndsInExe)
{
    const std::wstring p = path_info::ExecutablePath();
    ASSERT_GE(p.size(), 4u);
    std::wstring tail = p.substr(p.size() - 4);
    std::transform(tail.begin(), tail.end(), tail.begin(), [](wchar_t c) { return std::towlower(c); });
    EXPECT_EQ(tail, L".exe");
}

TEST(PathInfo, CurrentWorkingDirectoryIsNonEmpty)
{
    EXPECT_FALSE(path_info::CurrentWorkingDirectory().empty());
}

TEST(PathInfo, TempDirectoryHasNoTrailingSlash)
{
    const std::wstring t = path_info::TempDirectory();
    ASSERT_FALSE(t.empty());
    EXPECT_NE(t.back(), L'\\');
}

TEST(PathInfo, WindowsDirectoryMentionsWindows)
{
    const std::wstring w = path_info::WindowsDirectory();
    ASSERT_FALSE(w.empty());
    // The folder really is named "Windows" on every SKU CI runs on; a
    // lower-case filesystem would still carry the literal 'W'.
    EXPECT_TRUE(ContainsAny(w, {L"Windows", L"windows"}));
}

TEST(PathInfo, SystemDirectoryUnderWindowsDirectory)
{
    const std::wstring w = path_info::WindowsDirectory();
    const std::wstring s = path_info::SystemDirectory();
    ASSERT_FALSE(w.empty());
    ASSERT_FALSE(s.empty());
    // system32 / SysWOW64 both live beneath the Windows directory on every
    // supported runner configuration.
    EXPECT_TRUE(s.find(w) == 0) << "system dir not inside windows dir";
}

TEST(PathInfo, OsVersionLooksLikeWindows)
{
    const std::wstring v = path_info::OsVersionString();
    ASSERT_FALSE(v.empty());
    // Either "Windows X.Y (Build Z)" on success, or "Unknown" if RtlGetVersion
    // was not resolvable. Both are acceptable smoke outcomes — we just want
    // the helper to never crash or hand back garbage.
    EXPECT_TRUE(v.find(L"Windows") == 0 || v == L"Unknown");
}

TEST(PathInfo, WineOrProtonReturnsSomething)
{
    const std::wstring v = path_info::WineOrProtonVersion();
    // On real Windows: "Not detected". On Wine/Proton: starts with "Wine"
    // or "Proton". Either way it must be non-empty.
    EXPECT_FALSE(v.empty());
}

TEST(PathInfo, SaveFilePathEndsInSavedDataTxt)
{
    const std::wstring p = path_info::SaveFilePath();
    ASSERT_FALSE(p.empty());
    if (p == L"Not available")
        return; // SHGetKnownFolderPath was unavailable; acceptable outcome.
    const std::wstring suffix = L"saved_data.txt";
    ASSERT_GE(p.size(), suffix.size());
    EXPECT_EQ(p.substr(p.size() - suffix.size()), suffix);
}

TEST(PathInfo, CollectProducesExpectedLabels)
{
    const auto items = path_info::Collect();
    ASSERT_EQ(items.size(), 11u);

    // The main class stores this vector and reads labels verbatim in the UI.
    // Keep the order fixed so a future accidental reorder is a test failure.
    const std::vector<std::wstring> expected_labels = {
        L"OS Version: ",           L"Wine/Proton: ",       L"Executable name: ", L"Full path: ",
        L"Executable directory: ", L"Current directory: ", L"Command line: ",    L"Save file path: ",
        L"TEMP directory: ",       L"Windows directory: ", L"System directory: "};
    for (size_t i = 0; i < expected_labels.size(); ++i)
        EXPECT_EQ(items[i].first, expected_labels[i]) << "at index " << i;
}

TEST(PathInfo, CollectValuesAreNonEmptyWhereExpected)
{
    const auto items = path_info::Collect();
    // Executable name / full path / cwd / save path must be populated on a
    // live Windows environment.
    EXPECT_FALSE(items[2].second.empty()); // Executable name
    EXPECT_FALSE(items[3].second.empty()); // Full path
    EXPECT_FALSE(items[5].second.empty()); // Current directory
}
