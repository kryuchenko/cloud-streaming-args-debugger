#include <Windows.h>
#include <atomic>
#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

// Forward declaration for LogSEH
extern void LogSEH(const wchar_t* message);

// Test data structure
struct SEHTestData
{
    enum class TestMode
    {
        Normal,
        AccessViolation,
        StackOverflow,
        DivideByZero,
        CustomException
    };

    TestMode test_mode = TestMode::Normal;
    std::atomic<bool> thread_executed{false};
    std::atomic<DWORD> execution_result{0};
};

// Test function that simulates various exceptions
DWORD TestExceptionFunction(SEHTestData* data)
{
    data->thread_executed = true;

    switch (data->test_mode)
    {
    case SEHTestData::TestMode::Normal:
        // Normal execution
        return 0;

    case SEHTestData::TestMode::AccessViolation:
        // Trigger access violation
        {
            volatile int* null_ptr = nullptr;
            *null_ptr = 42; // This will cause access violation
        }
        break;

    case SEHTestData::TestMode::StackOverflow:
        // Trigger stack overflow through infinite recursion
        {
            // Simple recursive function to cause stack overflow quickly
            std::function<void(int)> recurse;
            recurse = [&recurse](int depth) {
                volatile char buffer[4096]; // Use more stack space per call
                buffer[0] = static_cast<char>(depth % 256);
                recurse(depth + 1); // Infinite recursion
            };
            recurse(0);
        }
        break;

    case SEHTestData::TestMode::DivideByZero:
        // Trigger divide by zero
        {
            volatile int zero = 0;
            volatile int result = 42 / zero;
            (void)result;
        }
        break;

    case SEHTestData::TestMode::CustomException:
        // Raise custom exception
        RaiseException(0xDEADBEEF, 0, 0, nullptr);
        break;
    }

    // Should not reach here
    return 0xFFFFFFFF;
}

// Test-specific SEH wrapper
extern "C" DWORD WINAPI TestSEHWrapper(LPVOID param) noexcept
{
    DWORD exitCode = 0;
    __try
    {
        SEHTestData* data = static_cast<SEHTestData*>(param);
        exitCode = TestExceptionFunction(data);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DWORD code = GetExceptionCode();
        
        // Log the exception
        wchar_t buf[128];
        wsprintfW(buf, L"TestSEH: Exception caught, code=0x%08X", code);
        LogSEH(buf);
        
        exitCode = code;
    }
    return exitCode;
}

class SEHExceptionTest : public ::testing::Test
{
  protected:
    SEHTestData test_data;

    void SetUp() override
    {
        test_data.test_mode = SEHTestData::TestMode::Normal;
        test_data.thread_executed = false;
        test_data.execution_result = 0;
    }

    // Helper to run thread with SEH wrapper
    DWORD RunThreadWithSEH()
    {
        test_data.thread_executed = false;
        test_data.execution_result = 0;

        // Create thread using the test SEH wrapper
        HANDLE thread = CreateThread(nullptr,
                                     0, // Default stack size
                                     TestSEHWrapper, &test_data, 0, nullptr);

        EXPECT_NE(thread, nullptr);

        // Wait for thread to complete
        DWORD wait_result = WaitForSingleObject(thread, 5000); // 5 second timeout
        EXPECT_EQ(wait_result, WAIT_OBJECT_0);

        // Get exit code
        DWORD exit_code = 0;
        BOOL success = GetExitCodeThread(thread, &exit_code);
        EXPECT_TRUE(success);

        CloseHandle(thread);
        return exit_code;
    }

    // Helper for stack overflow test with smaller stack
    DWORD RunThreadWithSEHSmallStack(DWORD stack_size)
    {
        test_data.thread_executed = false;
        test_data.execution_result = 0;

        // Create thread with specified stack size
        HANDLE thread = CreateThread(nullptr, stack_size, TestSEHWrapper, &test_data, 0, nullptr);

        EXPECT_NE(thread, nullptr);

        // Wait for thread to complete - shorter timeout, stack overflow should be fast
        DWORD wait_result = WaitForSingleObject(thread, 5000); // 5 second timeout for stack overflow
        EXPECT_EQ(wait_result, WAIT_OBJECT_0);

        // Get exit code
        DWORD exit_code = 0;
        BOOL success = GetExitCodeThread(thread, &exit_code);
        EXPECT_TRUE(success);

        CloseHandle(thread);
        return exit_code;
    }
};

TEST_F(SEHExceptionTest, NormalExecution)
{
    test_data.test_mode = SEHTestData::TestMode::Normal;

    DWORD exit_code = RunThreadWithSEH();

    EXPECT_TRUE(test_data.thread_executed);
    EXPECT_EQ(exit_code, 0); // Normal completion
}

TEST_F(SEHExceptionTest, AccessViolationHandling)
{
    test_data.test_mode = SEHTestData::TestMode::AccessViolation;

    DWORD exit_code = RunThreadWithSEH();

    EXPECT_TRUE(test_data.thread_executed);
    EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION)); // 0xC0000005
}

TEST_F(SEHExceptionTest, StackOverflowHandling)
{
    test_data.test_mode = SEHTestData::TestMode::StackOverflow;

    // Use smaller stack size to trigger overflow faster
    DWORD exit_code = RunThreadWithSEHSmallStack(64 * 1024); // 64KB stack

    EXPECT_TRUE(test_data.thread_executed);
    // Stack overflow might be detected as access violation or stack overflow
    EXPECT_TRUE(exit_code == static_cast<DWORD>(EXCEPTION_STACK_OVERFLOW) ||
                exit_code == static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));
}

TEST_F(SEHExceptionTest, DivideByZeroHandling)
{
    test_data.test_mode = SEHTestData::TestMode::DivideByZero;

    DWORD exit_code = RunThreadWithSEH();

    EXPECT_TRUE(test_data.thread_executed);
    EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_INT_DIVIDE_BY_ZERO)); // 0xC0000094
}

TEST_F(SEHExceptionTest, CustomExceptionHandling)
{
    test_data.test_mode = SEHTestData::TestMode::CustomException;

    DWORD exit_code = RunThreadWithSEH();

    EXPECT_TRUE(test_data.thread_executed);
    EXPECT_EQ(exit_code, 0xDEADBEEF); // Custom exception code
}

TEST_F(SEHExceptionTest, MultipleThreadsWithExceptions)
{
    // Test multiple threads with different exceptions
    std::vector<std::pair<SEHTestData::TestMode, DWORD>> test_cases = {
        {SEHTestData::TestMode::Normal, 0},
        {SEHTestData::TestMode::AccessViolation, EXCEPTION_ACCESS_VIOLATION},
        {SEHTestData::TestMode::StackOverflow, EXCEPTION_STACK_OVERFLOW},
        {SEHTestData::TestMode::DivideByZero, EXCEPTION_INT_DIVIDE_BY_ZERO},
        {SEHTestData::TestMode::CustomException, 0xDEADBEEF}};

    for (const auto& [mode, expected_code] : test_cases)
    {
        SEHTestData local_data;
        local_data.test_mode = mode;
        local_data.thread_executed = false;

        HANDLE thread = CreateThread(nullptr,
                                     mode == SEHTestData::TestMode::StackOverflow ? 64 * 1024 : 0,
                                     TestSEHWrapper, &local_data, 0, nullptr);

        ASSERT_NE(thread, nullptr);

        DWORD wait_result = WaitForSingleObject(thread, mode == SEHTestData::TestMode::StackOverflow ? 5000 : 10000);
        EXPECT_EQ(wait_result, WAIT_OBJECT_0);

        DWORD exit_code = 0;
        GetExitCodeThread(thread, &exit_code);
        CloseHandle(thread);

        if (mode == SEHTestData::TestMode::StackOverflow)
        {
            // Stack overflow might be detected as access violation or stack overflow
            EXPECT_TRUE(exit_code == static_cast<DWORD>(EXCEPTION_STACK_OVERFLOW) ||
                        exit_code == static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));
        }
        else
        {
            EXPECT_EQ(exit_code, expected_code);
        }
    }
}

TEST_F(SEHExceptionTest, ThreadStatePreservation)
{
    // Test that thread state is preserved across exceptions
    SEHTestData data1, data2, data3;
    data1.test_mode = SEHTestData::TestMode::Normal;
    data2.test_mode = SEHTestData::TestMode::AccessViolation;
    data3.test_mode = SEHTestData::TestMode::Normal;

    HANDLE thread1 = CreateThread(nullptr, 0, TestSEHWrapper, &data1, 0, nullptr);
    HANDLE thread2 = CreateThread(nullptr, 0, TestSEHWrapper, &data2, 0, nullptr);
    HANDLE thread3 = CreateThread(nullptr, 0, TestSEHWrapper, &data3, 0, nullptr);

    HANDLE threads[] = {thread1, thread2, thread3};
    WaitForMultipleObjects(3, threads, TRUE, 10000);

    DWORD exit_code1 = 0, exit_code2 = 0, exit_code3 = 0;
    GetExitCodeThread(thread1, &exit_code1);
    GetExitCodeThread(thread2, &exit_code2);
    GetExitCodeThread(thread3, &exit_code3);

    CloseHandle(thread1);
    CloseHandle(thread2);
    CloseHandle(thread3);

    EXPECT_EQ(exit_code1, 0);
    EXPECT_EQ(exit_code2, static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));
    EXPECT_EQ(exit_code3, 0);
}

TEST_F(SEHExceptionTest, RapidThreadCreationWithExceptions)
{
    // Stress test: rapidly create threads with exceptions
    const int num_threads = 10;

    for (int i = 0; i < num_threads; ++i)
    {
        SEHTestData data;
        data.test_mode = (i % 2 == 0) ? SEHTestData::TestMode::Normal : SEHTestData::TestMode::AccessViolation;

        HANDLE thread = CreateThread(nullptr, 0, TestSEHWrapper, &data, 0, nullptr);
        ASSERT_NE(thread, nullptr);

        WaitForSingleObject(thread, 5000);

        DWORD exit_code = 0;
        GetExitCodeThread(thread, &exit_code);
        CloseHandle(thread);

        if (i % 2 == 0)
        {
            EXPECT_EQ(exit_code, 0);
        }
        else
        {
            EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));
        }
    }
}

TEST_F(SEHExceptionTest, NullParameterHandling)
{
    // Test with null parameter
    HANDLE thread = CreateThread(nullptr, 0, TestSEHWrapper, nullptr, 0, nullptr);
    ASSERT_NE(thread, nullptr);

    WaitForSingleObject(thread, 5000);

    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);

    // Should handle null gracefully
    EXPECT_NE(exit_code, 0);
}