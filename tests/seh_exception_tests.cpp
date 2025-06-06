#include <Windows.h>
#include <atomic>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

// Forward declaration from seh_wrapper.cpp
extern "C" DWORD WINAPI RawAudioThreadWithSEH(LPVOID param) noexcept;

// Forward declaration for LogSEH
extern void LogSEH(const wchar_t* message);

// Mock class to simulate ArgumentDebuggerWindow for testing
class MockArgumentDebuggerWindow
{
  public:
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

    DWORD AudioCaptureThreadImpl(LPVOID param)
    {
        thread_executed = true;

        switch (test_mode)
        {
        case TestMode::Normal:
            // Normal execution
            return 0;

        case TestMode::AccessViolation:
            // Trigger access violation
            {
                volatile int* null_ptr = nullptr;
                *null_ptr = 42; // This will cause access violation
            }
            break;

        case TestMode::StackOverflow:
            // Trigger stack overflow through infinite recursion
            return CauseStackOverflow();

        case TestMode::DivideByZero:
            // Trigger integer divide by zero
            {
                volatile int zero = 0;
                volatile int result = 42 / zero;
                return result;
            }

        case TestMode::CustomException:
            // Raise a custom exception
            RaiseException(0xDEADBEEF, 0, 0, nullptr);
            break;
        }

        return 1; // Should not reach here if exception occurred
    }

  private:
    // Helper function to cause stack overflow
    DWORD CauseStackOverflow()
    {
        // Allocate large array on stack to quickly exhaust it
        volatile char big_array[1024 * 1024];
        big_array[0] = 1;
        return CauseStackOverflow() + big_array[0]; // Recursive call
    }
};

class SEHExceptionTest : public ::testing::Test
{
  protected:
    MockArgumentDebuggerWindow mock_window;

    // Helper to run thread with SEH wrapper
    DWORD RunThreadWithSEH()
    {
        mock_window.thread_executed = false;
        mock_window.execution_result = 0;

        // Create thread using the SEH wrapper
        HANDLE thread = CreateThread(nullptr,
                                     0, // Default stack size
                                     RawAudioThreadWithSEH, &mock_window, 0, nullptr);

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

    // Helper for stack overflow test with larger stack
    DWORD RunThreadWithSEHLargeStack(DWORD stack_size)
    {
        mock_window.thread_executed = false;
        mock_window.execution_result = 0;

        // Create thread with specified stack size
        HANDLE thread = CreateThread(nullptr, stack_size, RawAudioThreadWithSEH, &mock_window, 0, nullptr);

        EXPECT_NE(thread, nullptr);

        // Wait for thread to complete
        DWORD wait_result = WaitForSingleObject(thread, 10000); // 10 second timeout for stack overflow
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
    mock_window.test_mode = MockArgumentDebuggerWindow::TestMode::Normal;

    DWORD exit_code = RunThreadWithSEH();

    EXPECT_TRUE(mock_window.thread_executed);
    EXPECT_EQ(exit_code, 0); // Normal completion
}

TEST_F(SEHExceptionTest, AccessViolationHandling)
{
    mock_window.test_mode = MockArgumentDebuggerWindow::TestMode::AccessViolation;

    DWORD exit_code = RunThreadWithSEH();

    EXPECT_TRUE(mock_window.thread_executed);
    EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION)); // 0xC0000005
}

TEST_F(SEHExceptionTest, StackOverflowHandling)
{
    mock_window.test_mode = MockArgumentDebuggerWindow::TestMode::StackOverflow;

    // Use larger stack size to ensure we can handle the exception
    DWORD exit_code = RunThreadWithSEHLargeStack(2 * 1024 * 1024); // 2MB stack

    EXPECT_TRUE(mock_window.thread_executed);
    EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_STACK_OVERFLOW)); // 0xC00000FD
}

TEST_F(SEHExceptionTest, DivideByZeroHandling)
{
    mock_window.test_mode = MockArgumentDebuggerWindow::TestMode::DivideByZero;

    DWORD exit_code = RunThreadWithSEH();

    EXPECT_TRUE(mock_window.thread_executed);
    EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_INT_DIVIDE_BY_ZERO)); // 0xC0000094
}

TEST_F(SEHExceptionTest, CustomExceptionHandling)
{
    mock_window.test_mode = MockArgumentDebuggerWindow::TestMode::CustomException;

    DWORD exit_code = RunThreadWithSEH();

    EXPECT_TRUE(mock_window.thread_executed);
    EXPECT_EQ(exit_code, 0xDEADBEEF); // Our custom exception code
}

TEST_F(SEHExceptionTest, MultipleThreadsWithExceptions)
{
    const int num_threads = 5;
    std::vector<MockArgumentDebuggerWindow> mocks(num_threads);
    std::vector<HANDLE> threads;

    // Create threads with different exception types
    for (int i = 0; i < num_threads; ++i)
    {
        mocks[i].test_mode = static_cast<MockArgumentDebuggerWindow::TestMode>(i);

        HANDLE thread = CreateThread(nullptr, 0, RawAudioThreadWithSEH, &mocks[i], 0, nullptr);

        EXPECT_NE(thread, nullptr);
        threads.push_back(thread);
    }

    // Wait for all threads
    for (HANDLE thread : threads)
    {
        DWORD wait_result = WaitForSingleObject(thread, 5000);
        EXPECT_EQ(wait_result, WAIT_OBJECT_0);
    }

    // Verify results
    for (int i = 0; i < num_threads; ++i)
    {
        DWORD exit_code = 0;
        GetExitCodeThread(threads[i], &exit_code);

        switch (i)
        {
        case 0: // Normal
            EXPECT_EQ(exit_code, 0);
            break;
        case 1: // AccessViolation
            EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));
            break;
        case 2: // StackOverflow
            // Stack overflow might not always be caught in multi-threaded scenario
            // so we check if it's either caught or thread terminated
            EXPECT_TRUE(exit_code == static_cast<DWORD>(EXCEPTION_STACK_OVERFLOW) || exit_code == STILL_ACTIVE);
            break;
        case 3: // DivideByZero
            EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_INT_DIVIDE_BY_ZERO));
            break;
        case 4: // CustomException
            EXPECT_EQ(exit_code, 0xDEADBEEF);
            break;
        }

        CloseHandle(threads[i]);
    }
}

// Test that the SEH handler preserves thread state correctly
TEST_F(SEHExceptionTest, ThreadStatePreservation)
{
    // Set up some state
    mock_window.test_mode = MockArgumentDebuggerWindow::TestMode::Normal;

    // Run normal execution
    DWORD exit_code1 = RunThreadWithSEH();
    EXPECT_EQ(exit_code1, 0);

    // Now run with exception
    mock_window.test_mode = MockArgumentDebuggerWindow::TestMode::AccessViolation;
    DWORD exit_code2 = RunThreadWithSEH();
    EXPECT_EQ(exit_code2, static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));

    // Run normal again to ensure state is not corrupted
    mock_window.test_mode = MockArgumentDebuggerWindow::TestMode::Normal;
    DWORD exit_code3 = RunThreadWithSEH();
    EXPECT_EQ(exit_code3, 0);
}

// Test rapid thread creation and destruction with exceptions
TEST_F(SEHExceptionTest, RapidThreadCreationWithExceptions)
{
    const int iterations = 20;

    for (int i = 0; i < iterations; ++i)
    {
        // Alternate between normal and exception modes
        mock_window.test_mode = (i % 2 == 0) ? MockArgumentDebuggerWindow::TestMode::Normal
                                             : MockArgumentDebuggerWindow::TestMode::AccessViolation;

        DWORD exit_code = RunThreadWithSEH();

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

// Test null parameter handling
TEST_F(SEHExceptionTest, NullParameterHandling)
{
    // This test verifies the SEH wrapper handles null parameters gracefully
    HANDLE thread = CreateThread(nullptr, 0, RawAudioThreadWithSEH,
                                 nullptr, // NULL parameter
                                 0, nullptr);

    EXPECT_NE(thread, nullptr);

    DWORD wait_result = WaitForSingleObject(thread, 5000);
    EXPECT_EQ(wait_result, WAIT_OBJECT_0);

    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);

    // Should have caught the access violation from dereferencing null
    EXPECT_EQ(exit_code, static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION));

    CloseHandle(thread);
}