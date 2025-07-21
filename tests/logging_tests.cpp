#include <ShlObj.h>
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

// Forward declarations of functions from cli_args_debugger.cpp
extern void InitLogger();
extern void Log(const std::wstring& message);
extern void LogSEH(const wchar_t* message);

// External global variable from cli_args_debugger.cpp
extern std::wstring g_logPath;
extern FILE* g_log_file;

// Helper function to get the log file path
std::wstring GetLogFilePath()
{
    // If logger is initialized, use the actual path
    if (!g_logPath.empty())
    {
        return g_logPath;
    }
    
    // Otherwise, try to determine the expected path
    PWSTR appdata_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
    if (FAILED(hr))
    {
        return L"";
    }

    std::wstring log_path = appdata_path;
    CoTaskMemFree(appdata_path);
    log_path += L"\\ArgumentDebugger\\debug.log";
    return log_path;
}

// Helper function to read last N lines from log file
std::vector<std::wstring> ReadLastLogLines(int n)
{
    // Ensure all pending writes are flushed before reading
    if (g_log_file)
    {
        fflush(g_log_file);
    }
    
    std::wstring log_path = GetLogFilePath();
    std::vector<std::wstring> lines;

    // Open with UTF-16 encoding and shared read access
    FILE* read_file = _wfsopen(log_path.c_str(), L"r, ccs=UTF-16LE", _SH_DENYNO);
    if (!read_file)
    {
        return lines;
    }

    // Skip BOM if present
    wint_t first_char = fgetwc(read_file);
    if (first_char != 0xFEFF)
    {
        // Not a BOM, rewind
        fseek(read_file, 0, SEEK_SET);
    }

    wchar_t buffer[4096];
    while (fgetws(buffer, 4096, read_file))
    {
        std::wstring line(buffer);
        // Remove trailing newline
        if (!line.empty() && line.back() == L'\n')
        {
            line.pop_back();
        }
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }
        
        lines.push_back(line);
        if (lines.size() > static_cast<size_t>(n))
        {
            lines.erase(lines.begin());
        }
    }

    fclose(read_file);
    return lines;
}

// Helper function to clear the log file
void ClearLogFile()
{
    std::wstring log_path = GetLogFilePath();
    if (!log_path.empty())
    {
        // Try to delete the file first
        DeleteFileW(log_path.c_str());

        // Ensure the directory exists
        std::wstring dir_path = log_path.substr(0, log_path.find_last_of(L"\\"));
        CreateDirectoryW(dir_path.c_str(), nullptr);
    }
}

class LoggingTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Clear log file before each test
        ClearLogFile();
        // Initialize logger
        InitLogger();
    }

    void TearDown() override
    {
        // Small delay to ensure file operations complete
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

TEST_F(LoggingTest, InitLoggerCreatesLogFile)
{
    // Logger should already be initialized in SetUp
    std::wstring log_path = GetLogFilePath();
    EXPECT_FALSE(log_path.empty());

    // Verify the file exists
    EXPECT_TRUE(std::filesystem::exists(log_path));
}

TEST_F(LoggingTest, LogWritesSimpleMessage)
{
    const std::wstring test_message = L"Test log message";
    Log(test_message);

    // Give some time for the log to be written
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto lines = ReadLastLogLines(10);
    EXPECT_GT(lines.size(), 0);

    // Check if any line contains our test message
    bool found = false;
    for (const auto& line : lines)
    {
        if (line.find(test_message) != std::wstring::npos)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Log message not found in log file";
}

TEST_F(LoggingTest, LogWritesMultipleMessages)
{
    const std::vector<std::wstring> test_messages = {L"First message", L"Second message", L"Third message"};

    for (const auto& msg : test_messages)
    {
        Log(msg);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto lines = ReadLastLogLines(20);

    // Verify all messages were logged
    for (const auto& msg : test_messages)
    {
        bool found = false;
        for (const auto& line : lines)
        {
            if (line.find(msg) != std::wstring::npos)
            {
                found = true;
                break;
            }
        }
        // Convert wide string to narrow string safely
        std::string narrowMsg;
        for (wchar_t wc : msg) {
            if (wc <= 127) {
                narrowMsg += static_cast<char>(wc);
            } else {
                narrowMsg += '?';
            }
        }
        EXPECT_TRUE(found) << "Message not found: " << narrowMsg;
    }
}

TEST_F(LoggingTest, LogHandlesEmptyMessage)
{
    Log(L"");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should not crash and file should still exist
    EXPECT_TRUE(std::filesystem::exists(GetLogFilePath()));
}

TEST_F(LoggingTest, LogHandlesUnicodeCharacters)
{
    const std::wstring unicode_message = L"Unicode test: 你好世界 🌍 Привет мир";
    Log(unicode_message);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto lines = ReadLastLogLines(10);
    bool found = false;
    for (const auto& line : lines)
    {
        if (line.find(L"Unicode test:") != std::wstring::npos)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Unicode message not found in log file";
}

TEST_F(LoggingTest, LogSEHWritesExceptionCodes)
{
    // Test common exception messages
    const std::vector<std::wstring> exception_messages = {
        L"SEH: Access violation in audio thread (0xC0000005)", L"SEH: Stack overflow in audio thread (0xC00000FD)",
        L"SEH: Exception in audio thread, code=0xC0000094", L"SEH: Exception in audio thread, code=0xDEADBEEF"};

    for (const auto& msg : exception_messages)
    {
        LogSEH(msg.c_str());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto lines = ReadLastLogLines(20);

    // Check for access violation message
    bool found_access_violation = false;
    for (const auto& line : lines)
    {
        if (line.find(L"Access violation") != std::wstring::npos)
        {
            found_access_violation = true;
            break;
        }
    }
    EXPECT_TRUE(found_access_violation);

    // Check for stack overflow message
    bool found_stack_overflow = false;
    for (const auto& line : lines)
    {
        if (line.find(L"Stack overflow") != std::wstring::npos)
        {
            found_stack_overflow = true;
            break;
        }
    }
    EXPECT_TRUE(found_stack_overflow);

    // Check for custom code (should be logged as hex)
    bool found_custom = false;
    for (const auto& line : lines)
    {
        if (line.find(L"0xDEADBEEF") != std::wstring::npos)
        {
            found_custom = true;
            break;
        }
    }
    EXPECT_TRUE(found_custom);
}

TEST_F(LoggingTest, LogIsThreadSafe)
{
    const int num_threads = 10;
    const int messages_per_thread = 50;
    std::vector<std::thread> threads;

    // Launch multiple threads that write to the log
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(
            [i, messages_per_thread]()
            {
                for (int j = 0; j < messages_per_thread; ++j)
                {
                    Log(L"Thread " + std::to_wstring(i) + L" message " + std::to_wstring(j));
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
    }

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify log file is not corrupted and contains expected messages
    auto lines = ReadLastLogLines(1000);

    // We should have many lines (at least some from each thread)
    EXPECT_GT(lines.size(), 100);

    // Check that we have messages from different threads
    int thread_count = 0;
    for (int i = 0; i < num_threads; ++i)
    {
        bool found_thread = false;
        std::wstring thread_marker = L"Thread " + std::to_wstring(i);
        for (const auto& line : lines)
        {
            if (line.find(thread_marker) != std::wstring::npos)
            {
                found_thread = true;
                break;
            }
        }
        if (found_thread)
            thread_count++;
    }

    // We should have messages from most threads
    EXPECT_GT(thread_count, num_threads / 2);
}

TEST_F(LoggingTest, LogHandlesVeryLongMessage)
{
    // Create a very long message
    std::wstring long_message = L"Long message: ";
    for (int i = 0; i < 1000; ++i)
    {
        long_message += L"A";
    }

    Log(long_message);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto lines = ReadLastLogLines(10);
    bool found = false;
    for (const auto& line : lines)
    {
        if (line.find(L"Long message:") != std::wstring::npos)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Long message not found in log file";
}

TEST_F(LoggingTest, MultipleInitLoggerCallsAreSafe)
{
    // Call InitLogger multiple times
    InitLogger();
    InitLogger();
    InitLogger();

    // Should still be able to log
    Log(L"After multiple init calls");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto lines = ReadLastLogLines(10);
    bool found = false;
    for (const auto& line : lines)
    {
        if (line.find(L"After multiple init calls") != std::wstring::npos)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}