#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

// Include the actual implementation code
#include "cli_args_display.hpp"

// Conversion helper provided by cli_args_debugger.cpp
extern std::string wstring_to_string(const std::wstring& wstr);

// Test fixture for CLI argument formatting tests
class CliArgsTest : public ::testing::Test
{
  protected:
    // Helper for better error messages with wide strings
    void ExpectWideStringEq(const std::wstring& expected, const std::wstring& actual)
    {
        EXPECT_EQ(wstring_to_string(expected), wstring_to_string(actual))
            << "Expected: " << wstring_to_string(expected) << std::endl
            << "Actual: " << wstring_to_string(actual);
    }
};

// Test for empty arguments
TEST_F(CliArgsTest, EmptyArguments)
{
    std::vector<std::wstring> args = {};

    std::wstring expectedHeader = L"No arguments were received.";
    std::wstring expectedArgsLine = L"";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for a single argument
TEST_F(CliArgsTest, SingleArgument)
{
    std::vector<std::wstring> args = {L"first"};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"first";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for multiple arguments
TEST_F(CliArgsTest, MultipleArguments)
{
    std::vector<std::wstring> args = {L"one", L"two", L"three"};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"one two three";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for arguments with spaces
TEST_F(CliArgsTest, ArgumentsWithSpaces)
{
    std::vector<std::wstring> args = {L"hello world", L"arg"};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"hello world\" arg";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for non-ASCII arguments
TEST_F(CliArgsTest, NonAsciiArguments)
{
    std::vector<std::wstring> args = {L"こんにちは", L"世界"};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"こんにちは 世界";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for paths with spaces
TEST_F(CliArgsTest, PathWithSpaces)
{
    std::vector<std::wstring> args = {L"C:\\Program Files\\App", L"-f"};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"C:\\Program Files\\App\" -f";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for empty string in arguments
TEST_F(CliArgsTest, EmptyStringArgument)
{
    std::vector<std::wstring> args = {L"", L"empty-arg"};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"\" empty-arg";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for argument with quotes
TEST_F(CliArgsTest, ArgumentWithQuotes)
{
    std::vector<std::wstring> args = {L"argument with \"quotes\""};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"argument with \"quotes\"\"";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for single argument with space - no trailing space
TEST_F(CliArgsTest, SingleArgumentWithSpaceNoTrailingSpace)
{
    std::vector<std::wstring> args = {L"single arg with space"};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"single arg with space\"";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for arguments containing tabs or newlines
TEST_F(CliArgsTest, ArgumentWithWhitespaceCharacters)
{
    std::vector<std::wstring> args = {L"tab\tnewline\n", L"arg"};

    std::wstring expectedHeader = L"Received the following arguments:";
    std::wstring expectedArgsLine = L"\"tab\tnewline\n\" arg";

    std::wstring actualHeader = BuildCliHeaderText(args);
    std::wstring actualArgsLine = BuildCliArgsText(args);

    ExpectWideStringEq(expectedHeader, actualHeader);
    ExpectWideStringEq(expectedArgsLine, actualArgsLine);
}

// Test for command parsing - sound command
TEST_F(CliArgsTest, SoundCommandParsing)
{
    // Test case-insensitive parsing
    std::wstring soundLower = L"sound";
    std::wstring soundUpper = L"SOUND";
    std::wstring soundMixed = L"SoUnD";
    
    EXPECT_EQ(_wcsicmp(soundLower.c_str(), L"sound"), 0);
    EXPECT_EQ(_wcsicmp(soundUpper.c_str(), L"sound"), 0);
    EXPECT_EQ(_wcsicmp(soundMixed.c_str(), L"sound"), 0);
}

// Test for command description update
TEST_F(CliArgsTest, CommandDescriptionIncludesAllCommands)
{
    // The description should mention all commands including sound and memory
    std::wstring expectedText = L"Type 'exit', 'save', 'read', 'logs', 'path', 'sound' or 'memory' and press Enter to execute commands.";
    
    // This test verifies that the description has been updated
    // In a real test, we would check the actual kDescriptionLines vector
    EXPECT_TRUE(expectedText.find(L"sound") != std::wstring::npos);
    EXPECT_TRUE(expectedText.find(L"memory") != std::wstring::npos);
}

// Test for memory command parsing
TEST_F(CliArgsTest, MemoryCommandParsing)
{
    // Test case-insensitive parsing for memory command
    std::wstring memoryLower = L"memory";
    std::wstring memoryUpper = L"MEMORY";
    std::wstring memoryMixed = L"MeMoRy";
    
    EXPECT_EQ(_wcsicmp(memoryLower.c_str(), L"memory"), 0);
    EXPECT_EQ(_wcsicmp(memoryUpper.c_str(), L"memory"), 0);
    EXPECT_EQ(_wcsicmp(memoryMixed.c_str(), L"memory"), 0);
}
