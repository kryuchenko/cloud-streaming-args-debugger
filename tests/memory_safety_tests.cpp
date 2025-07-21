#include <gtest/gtest.h>
#include <windows.h>
#include <vector>
#include <string>
#include <limits>
#include <cstring>

// Test fixture for memory safety tests
class MemorySafetyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Enable heap validation in debug builds
#ifdef _DEBUG
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif
    }
    
    void TearDown() override
    {
#ifdef _DEBUG
        // Check for memory leaks
        _CrtCheckMemory();
#endif
    }
};

// Test for buffer overflow detection
TEST_F(MemorySafetyTest, BufferOverflowProtection)
{
    const size_t bufferSize = 10;
    std::vector<char> buffer(bufferSize);
    
    // Safe operation - should succeed
    EXPECT_NO_THROW({
        for (size_t i = 0; i < bufferSize; ++i) {
            buffer[i] = 'A';
        }
    });
    
    // Using at() for bounds checking
    EXPECT_THROW({
        buffer.at(bufferSize) = 'X';  // Out of bounds
    }, std::out_of_range);
}

// Test for integer overflow in buffer calculations
TEST_F(MemorySafetyTest, IntegerOverflowCheck)
{
    // Test multiplication overflow
    UINT32 frames = 100000;
    UINT32 channels = 50000;
    
    // Check for overflow before multiplication
    UINT64 result = static_cast<UINT64>(frames) * static_cast<UINT64>(channels);
    bool wouldOverflow = (result > UINT32_MAX);
    
    EXPECT_TRUE(wouldOverflow);
    
    // Safe calculation
    const UINT32 safeFrames = 1000;
    const UINT32 safeChannels = 2;
    UINT64 safeResult = static_cast<UINT64>(safeFrames) * static_cast<UINT64>(safeChannels);
    
    EXPECT_LE(safeResult, UINT32_MAX);
}

// Test for string buffer overflow protection
TEST_F(MemorySafetyTest, StringBufferOverflow)
{
    // Test std::wstring with length limit
    const char* longString = "This is a very long string that could potentially cause issues";
    size_t maxLen = 20;
    
    // Safe string construction with length limit
    size_t actualLen = strnlen_s(longString, maxLen);
    std::wstring safeWString(longString, longString + actualLen);
    
    EXPECT_LE(safeWString.length(), maxLen);
}

// Test for null pointer dereference protection
TEST_F(MemorySafetyTest, NullPointerProtection)
{
    char* nullPtr = nullptr;
    
    // Should not crash - strnlen_s handles null
    size_t len = strnlen_s(nullPtr, 100);
    EXPECT_EQ(len, 0);
    
    // Test with std::string
    std::string str;
    const char* cstr = str.c_str();
    EXPECT_NE(cstr, nullptr);  // c_str() never returns null
}

// Test for array bounds checking
TEST_F(MemorySafetyTest, ArrayBoundsChecking)
{
    const size_t arraySize = 5;
    int testArray[arraySize] = {1, 2, 3, 4, 5};
    
    // Safe access
    for (size_t i = 0; i < arraySize; ++i) {
        EXPECT_NO_THROW({
            int value = testArray[i];
            (void)value;
        });
    }
    
    // Simulate bounds check
    auto safeAccess = [&](size_t index) -> int {
        if (index >= arraySize) {
            throw std::out_of_range("Index out of bounds");
        }
        return testArray[index];
    };
    
    EXPECT_THROW(safeAccess(arraySize), std::out_of_range);
}

// Test for memory allocation failures
TEST_F(MemorySafetyTest, AllocationFailureHandling)
{
    // Test nothrow allocation
    size_t hugeSize = SIZE_MAX / 2;  // Likely to fail
    int* ptr = new(std::nothrow) int[hugeSize];
    
    if (!ptr) {
        // Allocation failed as expected
        SUCCEED();
    } else {
        // If somehow allocated, clean up
        delete[] ptr;
        FAIL() << "Unexpectedly allocated huge memory block";
    }
}

// Test for double-free protection
TEST_F(MemorySafetyTest, DoubleFreeProtection)
{
    // Using smart pointers prevents double-free
    {
        std::unique_ptr<int> ptr(new int(42));
        // Automatic cleanup when out of scope
    }
    // No double-free possible
    SUCCEED();
}

// Test for use-after-free protection
TEST_F(MemorySafetyTest, UseAfterFreeProtection)
{
    std::weak_ptr<int> weakPtr;
    
    {
        std::shared_ptr<int> sharedPtr = std::make_shared<int>(42);
        weakPtr = sharedPtr;
        EXPECT_FALSE(weakPtr.expired());
    }
    
    // Object is now freed
    EXPECT_TRUE(weakPtr.expired());
    
    // Safe check before use
    if (auto locked = weakPtr.lock()) {
        FAIL() << "Should not be able to lock expired weak_ptr";
    } else {
        SUCCEED();
    }
}

// Test for stack buffer overflow detection
TEST_F(MemorySafetyTest, StackBufferOverflowDetection)
{
    char stackBuffer[10];
    
    // Safe copy with size limit
    const char* source = "Hello";
    strncpy_s(stackBuffer, sizeof(stackBuffer), source, _TRUNCATE);
    
    EXPECT_STREQ(stackBuffer, "Hello");
    
    // Test truncation
    const char* longSource = "This is too long for the buffer";
    strncpy_s(stackBuffer, sizeof(stackBuffer), longSource, _TRUNCATE);
    
    // Should be truncated to fit
    EXPECT_EQ(strlen(stackBuffer), sizeof(stackBuffer) - 1);
}

// Test for heap corruption detection
TEST_F(MemorySafetyTest, HeapCorruptionDetection)
{
#ifdef _DEBUG
    // In debug mode, CRT can detect heap corruption
    _CrtCheckMemory();
    
    // Allocate and properly free memory
    void* buffer = malloc(100);
    ASSERT_NE(buffer, nullptr);
    
    // Write within bounds
    memset(buffer, 0, 100);
    
    // Check heap integrity
    EXPECT_TRUE(_CrtCheckMemory());
    
    free(buffer);
    
    // Final heap check
    EXPECT_TRUE(_CrtCheckMemory());
#else
    GTEST_SKIP() << "Heap corruption detection requires debug build";
#endif
}