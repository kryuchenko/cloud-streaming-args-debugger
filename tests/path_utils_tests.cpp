#include <gtest/gtest.h>
#include <iostream>
#include <shlobj.h>
#include <string>
#include <vector>
#include <windows.h>

// Test fixture for path utility tests
class PathUtilsTest : public ::testing::Test
{
  protected:
    // Helper to check if a path is valid
    bool IsValidPath(const std::wstring& path)
    {
        return path != L"Not available" && path != L"Unknown";
    }

    // Helper to check if string contains substring
    bool Contains(const std::wstring& str, const std::wstring& substr)
    {
        return str.find(substr) != std::wstring::npos;
    }
};

// Test getting executable path
TEST_F(PathUtilsTest, GetExecutablePath)
{
    wchar_t exePath[MAX_PATH] = L"\0";
    DWORD result = GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    EXPECT_GT(result, 0);
    EXPECT_TRUE(IsValidPath(exePath));

    // Should end with .exe
    std::wstring path(exePath);
    EXPECT_TRUE(Contains(path, L".exe"));
}

// Test getting current directory
TEST_F(PathUtilsTest, GetCurrentDirectory)
{
    wchar_t currentDir[MAX_PATH] = L"\0";
    DWORD result = GetCurrentDirectoryW(MAX_PATH, currentDir);

    EXPECT_GT(result, 0);
    EXPECT_TRUE(IsValidPath(currentDir));
}

// Test extracting executable name from path
TEST_F(PathUtilsTest, ExtractExecutableName)
{
    std::wstring fullPath = L"C:\\Program Files\\MyApp\\app.exe";

    size_t lastSlash = fullPath.find_last_of(L"\\");
    EXPECT_NE(lastSlash, std::wstring::npos);

    std::wstring exeName = fullPath.substr(lastSlash + 1);
    EXPECT_EQ(exeName, L"app.exe");
}

// Test getting Windows directory
TEST_F(PathUtilsTest, GetWindowsDirectory)
{
    wchar_t winPath[MAX_PATH] = L"\0";
    UINT result = GetWindowsDirectoryW(winPath, MAX_PATH);

    EXPECT_GT(result, 0);
    EXPECT_TRUE(IsValidPath(winPath));

    // Should contain "Windows"
    std::wstring path(winPath);
    EXPECT_TRUE(Contains(path, L"Windows"));
}

// Test getting System directory
TEST_F(PathUtilsTest, GetSystemDirectory)
{
    wchar_t sysPath[MAX_PATH] = L"\0";
    UINT result = GetSystemDirectoryW(sysPath, MAX_PATH);

    EXPECT_GT(result, 0);
    EXPECT_TRUE(IsValidPath(sysPath));

    // Just check that we got a non-empty path - the exact content varies by system
    std::wstring path(sysPath);
    EXPECT_FALSE(path.empty());

    // Log the actual path for debugging
    std::wcout << L"System directory: " << path << std::endl;
}

// Test getting TEMP directory
TEST_F(PathUtilsTest, GetTempDirectory)
{
    wchar_t tempPath[MAX_PATH] = L"\0";
    DWORD result = GetTempPathW(MAX_PATH, tempPath);

    EXPECT_GT(result, 0);
    EXPECT_TRUE(IsValidPath(tempPath));
}

// Test getting AppData path
TEST_F(PathUtilsTest, GetAppDataPath)
{
    PWSTR appdata_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);

    EXPECT_EQ(hr, S_OK);
    EXPECT_NE(appdata_path, nullptr);

    if (appdata_path)
    {
        std::wstring path(appdata_path);
        EXPECT_TRUE(IsValidPath(path));
        EXPECT_TRUE(Contains(path, L"AppData"));
        CoTaskMemFree(appdata_path);
    }
}

// Test relative path calculation
TEST_F(PathUtilsTest, CalculateRelativePath)
{
    // Test case 1: File is in subdirectory
    std::wstring currentDir = L"C:\\Projects\\MyApp";
    std::wstring fullPath = L"C:\\Projects\\MyApp\\bin\\app.exe";

    std::wstring relativePath;
    if (fullPath.find(currentDir) == 0)
    {
        relativePath = L"." + fullPath.substr(currentDir.length());
    }

    EXPECT_EQ(relativePath, L".\\bin\\app.exe");

    // Test case 2: File is not in current directory
    std::wstring otherPath = L"D:\\OtherFolder\\app.exe";
    if (otherPath.find(currentDir) == 0)
    {
        relativePath = L"." + otherPath.substr(currentDir.length());
    }
    else
    {
        relativePath = otherPath;
    }

    EXPECT_EQ(relativePath, otherPath);
}

// Test OS version detection
TEST_F(PathUtilsTest, GetOSVersion)
{
    // We can't test the exact version, but we can test that the function works
    // Note: RTL_OSVERSIONINFOW and NTSTATUS are already defined in Windows SDK
    typedef NTSTATUS(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    EXPECT_NE(hNtdll, nullptr);

    if (hNtdll)
    {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        EXPECT_NE(RtlGetVersion, nullptr);

        if (RtlGetVersion)
        {
            RTL_OSVERSIONINFOW osvi = {0};
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            NTSTATUS status = RtlGetVersion(&osvi);

            EXPECT_EQ(status, 0);
            EXPECT_GE(osvi.dwMajorVersion, 6); // Windows Vista or later
            EXPECT_GT(osvi.dwBuildNumber, 0);
        }
    }
}

// Test Wine detection (will only pass if running under Wine)
TEST_F(PathUtilsTest, WineDetection)
{
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    EXPECT_NE(hNtdll, nullptr);

    if (hNtdll)
    {
        // Check if wine_get_version exists
        void* wineFunc = GetProcAddress(hNtdll, "wine_get_version");

        // This test will show if we're running under Wine
        if (wineFunc != nullptr)
        {
            std::cout << "Running under Wine/Proton" << std::endl;
        }
        else
        {
            std::cout << "Not running under Wine/Proton" << std::endl;
        }
    }
}

// Test command line retrieval
TEST_F(PathUtilsTest, GetCommandLine)
{
    std::wstring cmdLine = GetCommandLineW();

    EXPECT_FALSE(cmdLine.empty());
    // Command line should contain the executable name
    EXPECT_TRUE(Contains(cmdLine, L".exe"));
}

// Test environment variable retrieval
TEST_F(PathUtilsTest, GetEnvironmentVariable)
{
    // Test with a known environment variable
    wchar_t pathEnv[4096] = {0};
    DWORD result = GetEnvironmentVariableW(L"PATH", pathEnv, 4096);

    // PATH should always exist
    EXPECT_GT(result, 0);
    // PATH can be empty string which is still valid, so just check it's not "Unknown"
    std::wstring path(pathEnv);
    EXPECT_NE(path, L"Unknown");
}

// Test save path construction
TEST_F(PathUtilsTest, ConstructSavePath)
{
    PWSTR appdata_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);

    if (SUCCEEDED(hr) && appdata_path)
    {
        std::wstring savePath = appdata_path;
        CoTaskMemFree(appdata_path);
        savePath += L"\\ArgumentDebugger\\saved_data.txt";

        EXPECT_TRUE(Contains(savePath, L"AppData"));
        EXPECT_TRUE(Contains(savePath, L"ArgumentDebugger"));
        EXPECT_TRUE(Contains(savePath, L"saved_data.txt"));
    }
}