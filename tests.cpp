#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <gtest/gtest.h>

// Include the actual implementation code
#include "cli_args_display.hpp"

// For running tests outside the main application
std::string wstring_to_string(const std::wstring &wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                                        static_cast<int>(wstr.size()),
                                        nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                      static_cast<int>(wstr.size()),
                      &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

// Test fixture for CLI argument formatting tests
class CliArgsTest : public ::testing::Test {
protected:
    // Helper for better error messages with wide strings
    void ExpectWideStringEq(const std::wstring& expected, const std::wstring& actual) {
        EXPECT_EQ(wstring_to_string(expected), wstring_to_string(actual))
            << "Expected: " << wstring_to_string(expected) << std::endl
            << "Actual: " << wstring_to_string(actual);
    }
};

// Test for empty arguments
TEST_F(CliArgsTest, EmptyArguments) {
    std::vector<std::wstring> args = {};
    
    std::wstring expectedHeader = L"No arguments were received.";
    std::wstring expectedArgsLine = L"";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for a single argument
TEST_F(CliArgsTest, SingleArgument) {
    std::vector<std::wstring> args = { L"first" };
    
    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"first";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for multiple arguments
TEST_F(CliArgsTest, MultipleArguments) {
    std::vector<std::wstring> args = { L"one", L"two", L"three" };
    
    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"one two three";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for arguments with spaces
TEST_F(CliArgsTest, ArgumentsWithSpaces) {
    std::vector<std::wstring> args = { L"hello world", L"arg" };
    
    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"hello world\" arg";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for non-ASCII arguments
TEST_F(CliArgsTest, NonAsciiArguments) {
    std::vector<std::wstring> args = { L"Привет", L"мир" };
    
    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"Привет мир";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for paths with spaces
TEST_F(CliArgsTest, PathWithSpaces) {
    std::vector<std::wstring> args = { L"C:\\Program Files\\App", L"-f" };
    
    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"C:\\Program Files\\App\" -f";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for empty string in arguments
TEST_F(CliArgsTest, EmptyStringArgument) {
    std::vector<std::wstring> args = { L"", L"empty-arg" };
    
    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"\" empty-arg";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for argument with quotes
TEST_F(CliArgsTest, ArgumentWithQuotes) {
    std::vector<std::wstring> args = { L"argument with \"quotes\"" };
    
    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"argument with \"quotes\"\"";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for single argument with space - no trailing space
TEST_F(CliArgsTest, SingleArgumentWithSpaceNoTrailingSpace) {
    std::vector<std::wstring> args = { L"single arg with space" };
    
    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"single arg with space\"";
    
    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);
    
    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}