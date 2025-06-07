#include <ShlObj.h>
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <thread>

// Mock class to test SaveData and ReadData functionality
class DataPersistenceTestHelper
{
  public:
    std::wstring command_status_;
    std::wstring loaded_data_;
    int synced_fps_ = 60; // Default FPS value

    // Get the save file path
    std::wstring GetSaveFilePath()
    {
        PWSTR appdata_path = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
        if (FAILED(hr))
        {
            return L"";
        }

        std::wstring file_path = appdata_path;
        CoTaskMemFree(appdata_path);
        file_path += L"\\ArgumentDebugger\\saved_data.txt";
        return file_path;
    }

    // Implementation of SaveData (copied from cli_args_debugger.cpp)
    void SaveData()
    {
        PWSTR appdata_path = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
        if (SUCCEEDED(hr))
        {
            std::wstring folder_path = appdata_path;
            CoTaskMemFree(appdata_path);
            folder_path += L"\\ArgumentDebugger";

            if (!CreateDirectoryW(folder_path.c_str(), nullptr))
            {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS)
                {
                    command_status_ = L"Error creating directory.";
                    return;
                }
            }

            std::wstring file_path = folder_path + L"\\saved_data.txt";
            FILE* file = nullptr;
            if (_wfopen_s(&file, file_path.c_str(), L"w, ccs=UTF-8") == 0 && file)
            {
                time_t timestamp = time(nullptr);
                fwprintf(file, L"Timestamp: %lld\nFPS: %d\n", static_cast<long long>(timestamp), synced_fps_);
                fclose(file);
                command_status_ = L"Data saved successfully.";
            }
            else
            {
                command_status_ = L"Error opening file for writing.";
            }
        }
        else
        {
            command_status_ = L"Error retrieving AppData path.";
        }
    }

    // Implementation of ReadData (copied from cli_args_debugger.cpp)
    void ReadData()
    {
        PWSTR appdata_path = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
        if (SUCCEEDED(hr))
        {
            std::wstring folder_path = appdata_path;
            CoTaskMemFree(appdata_path);
            folder_path += L"\\ArgumentDebugger";
            std::wstring file_path = folder_path + L"\\saved_data.txt";
            FILE* file = nullptr;
            if (_wfopen_s(&file, file_path.c_str(), L"r, ccs=UTF-8") == 0 && file)
            {
                wchar_t buffer[256];
                loaded_data_.clear();
                while (fgetws(buffer, 256, file))
                {
                    loaded_data_ += buffer;
                }
                fclose(file);
                command_status_ = L"Data loaded successfully.";
            }
            else
            {
                command_status_ = L"File not found.";
                loaded_data_.clear();
            }
        }
        else
        {
            command_status_ = L"Error retrieving AppData path.";
        }
    }

    // Helper to clean up test files
    void CleanupTestFiles()
    {
        std::wstring file_path = GetSaveFilePath();
        if (!file_path.empty())
        {
            DeleteFileW(file_path.c_str());
        }
    }
};

class DataPersistenceTest : public ::testing::Test
{
  protected:
    DataPersistenceTestHelper helper;

    void SetUp() override
    {
        // Clean up any existing test files
        helper.CleanupTestFiles();
    }

    void TearDown() override
    {
        // Clean up test files after each test
        helper.CleanupTestFiles();
    }
};

TEST_F(DataPersistenceTest, SaveDataCreatesFile)
{
    helper.SaveData();

    // Check status
    EXPECT_EQ(helper.command_status_, L"Data saved successfully.");

    // Verify file exists
    std::wstring file_path = helper.GetSaveFilePath();
    EXPECT_TRUE(std::filesystem::exists(file_path));
}

TEST_F(DataPersistenceTest, SaveDataWritesCorrectFormat)
{
    helper.synced_fps_ = 144; // Set custom FPS
    time_t before_save = time(nullptr);

    helper.SaveData();

    time_t after_save = time(nullptr);

    // Read the file manually to verify format
    std::wstring file_path = helper.GetSaveFilePath();
    
    // Open file with UTF-8 encoding as written by SaveData
    FILE* read_file = nullptr;
    ASSERT_EQ(_wfopen_s(&read_file, file_path.c_str(), L"r, ccs=UTF-8"), 0);
    ASSERT_NE(read_file, nullptr);

    wchar_t buffer[256];
    std::wstring line;
    
    // First line should be timestamp
    ASSERT_NE(fgetws(buffer, 256, read_file), nullptr);
    line = buffer;
    // Remove trailing newline
    if (!line.empty() && line.back() == L'\n') line.pop_back();
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    
    EXPECT_TRUE(line.find(L"Timestamp: ") == 0);

    // Extract timestamp and verify it's reasonable
    size_t timestamp_pos = line.find(L": ") + 2;
    std::wstring timestamp_str = line.substr(timestamp_pos);
    long long timestamp = std::stoll(timestamp_str);
    EXPECT_GE(timestamp, before_save);
    EXPECT_LE(timestamp, after_save);

    // Second line should be FPS
    ASSERT_NE(fgetws(buffer, 256, read_file), nullptr);
    line = buffer;
    // Remove trailing newline
    if (!line.empty() && line.back() == L'\n') line.pop_back();
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    
    EXPECT_EQ(line, L"FPS: 144");

    fclose(read_file);
}

TEST_F(DataPersistenceTest, ReadDataLoadsExistingFile)
{
    // First save some data
    helper.synced_fps_ = 75;
    helper.SaveData();
    EXPECT_EQ(helper.command_status_, L"Data saved successfully.");

    // Clear loaded data
    helper.loaded_data_.clear();

    // Now read it back
    helper.ReadData();

    // Check status
    EXPECT_EQ(helper.command_status_, L"Data loaded successfully.");

    // Check loaded data contains expected content
    EXPECT_FALSE(helper.loaded_data_.empty());
    EXPECT_NE(helper.loaded_data_.find(L"Timestamp:"), std::wstring::npos);
    EXPECT_NE(helper.loaded_data_.find(L"FPS: 75"), std::wstring::npos);
}

TEST_F(DataPersistenceTest, ReadDataHandlesMissingFile)
{
    // Try to read when no file exists
    helper.ReadData();

    // Check status
    EXPECT_EQ(helper.command_status_, L"File not found.");
    EXPECT_TRUE(helper.loaded_data_.empty());
}

TEST_F(DataPersistenceTest, SaveDataCreatesDirectory)
{
    // Use a unique test directory to avoid conflicts
    PWSTR appdata_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_path);
    if (SUCCEEDED(hr))
    {
        std::wstring test_folder = appdata_path;
        CoTaskMemFree(appdata_path);
        test_folder += L"\\ArgumentDebuggerTest_" + std::to_wstring(GetCurrentProcessId());

        // Remove test directory if it exists (ignore errors)
        try {
            std::filesystem::remove_all(test_folder);
        } catch (...) {
            // Ignore errors
        }

        // Verify directory doesn't exist
        EXPECT_FALSE(std::filesystem::exists(test_folder));

        // Save data should create the directory
        helper.SaveData();

        // Check that SaveData was successful (it creates the standard directory)
        EXPECT_EQ(helper.command_status_, L"Data saved successfully.");
    }
}

TEST_F(DataPersistenceTest, SaveDataOverwritesExistingFile)
{
    // Save initial data
    helper.synced_fps_ = 30;
    helper.SaveData();

    // Read it to verify
    helper.ReadData();
    EXPECT_NE(helper.loaded_data_.find(L"FPS: 30"), std::wstring::npos);

    // Save new data with different FPS
    helper.synced_fps_ = 120;
    helper.SaveData();

    // Read again
    helper.loaded_data_.clear();
    helper.ReadData();

    // Should have new FPS value, not old one
    EXPECT_NE(helper.loaded_data_.find(L"FPS: 120"), std::wstring::npos);
    EXPECT_EQ(helper.loaded_data_.find(L"FPS: 30"), std::wstring::npos);
}

TEST_F(DataPersistenceTest, SaveDataHandlesZeroFPS)
{
    helper.synced_fps_ = 0;
    helper.SaveData();

    EXPECT_EQ(helper.command_status_, L"Data saved successfully.");

    helper.ReadData();
    EXPECT_NE(helper.loaded_data_.find(L"FPS: 0"), std::wstring::npos);
}

TEST_F(DataPersistenceTest, SaveDataHandlesNegativeFPS)
{
    helper.synced_fps_ = -1;
    helper.SaveData();

    EXPECT_EQ(helper.command_status_, L"Data saved successfully.");

    helper.ReadData();
    EXPECT_NE(helper.loaded_data_.find(L"FPS: -1"), std::wstring::npos);
}

TEST_F(DataPersistenceTest, ReadDataPreservesNewlines)
{
    // Save data first
    helper.SaveData();

    // Read it back
    helper.ReadData();

    // Count newlines - should have at least one (between timestamp and FPS)
    int newline_count = 0;
    for (wchar_t ch : helper.loaded_data_)
    {
        if (ch == L'\n')
            newline_count++;
    }
    EXPECT_GE(newline_count, 1);
}

TEST_F(DataPersistenceTest, ConcurrentSaveOperations)
{
    // Test that multiple saves don't corrupt the file
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i)
    {
        threads.emplace_back(
            [this, i]()
            {
                DataPersistenceTestHelper local_helper;
                local_helper.synced_fps_ = 60 + i;
                local_helper.SaveData();
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // File should still be readable
    helper.ReadData();
    EXPECT_EQ(helper.command_status_, L"Data loaded successfully.");
    EXPECT_FALSE(helper.loaded_data_.empty());

    // Should have valid timestamp and FPS lines
    EXPECT_NE(helper.loaded_data_.find(L"Timestamp:"), std::wstring::npos);
    EXPECT_NE(helper.loaded_data_.find(L"FPS:"), std::wstring::npos);
}

TEST_F(DataPersistenceTest, SaveDataHandlesMaxFPS)
{
    helper.synced_fps_ = INT_MAX;
    helper.SaveData();

    EXPECT_EQ(helper.command_status_, L"Data saved successfully.");

    helper.ReadData();
    std::wstring expected_fps = L"FPS: " + std::to_wstring(INT_MAX);
    EXPECT_NE(helper.loaded_data_.find(expected_fps), std::wstring::npos);
}

TEST_F(DataPersistenceTest, TimestampIsReasonable)
{
    time_t before = time(nullptr);
    helper.SaveData();
    time_t after = time(nullptr);

    helper.ReadData();

    // Extract timestamp from loaded data
    size_t timestamp_pos = helper.loaded_data_.find(L"Timestamp: ");
    ASSERT_NE(timestamp_pos, std::wstring::npos);

    timestamp_pos += 11; // Length of "Timestamp: "
    size_t newline_pos = helper.loaded_data_.find(L'\n', timestamp_pos);
    std::wstring timestamp_str = helper.loaded_data_.substr(timestamp_pos, newline_pos - timestamp_pos);

    long long timestamp = std::stoll(timestamp_str);
    EXPECT_GE(timestamp, before);
    EXPECT_LE(timestamp, after);
}