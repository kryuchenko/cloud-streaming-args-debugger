#include <Windows.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

// Forward declaration of function from cli_args_debugger.cpp
extern std::string wstring_to_string(const std::wstring& wstr);

class StringConversionTest : public ::testing::Test
{
  protected:
    // Helper function to verify round-trip conversion
    bool VerifyRoundTrip(const std::wstring& original)
    {
        std::string utf8 = wstring_to_string(original);

        // Convert back to wide string
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring converted(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &converted[0], size_needed);

        return original == converted;
    }
};

TEST_F(StringConversionTest, EmptyString)
{
    std::wstring empty = L"";
    std::string result = wstring_to_string(empty);
    EXPECT_TRUE(result.empty());
}

TEST_F(StringConversionTest, SimpleASCIIString)
{
    std::wstring ascii = L"Hello, World!";
    std::string result = wstring_to_string(ascii);
    EXPECT_EQ(result, "Hello, World!");
}

TEST_F(StringConversionTest, NumericString)
{
    std::wstring numeric = L"1234567890";
    std::string result = wstring_to_string(numeric);
    EXPECT_EQ(result, "1234567890");
}

TEST_F(StringConversionTest, SpecialCharacters)
{
    std::wstring special = L"!@#$%^&*()_+-=[]{}|;':\",./<>?";
    std::string result = wstring_to_string(special);
    EXPECT_EQ(result, "!@#$%^&*()_+-=[]{}|;':\",./<>?");
}

TEST_F(StringConversionTest, WhitespaceCharacters)
{
    std::wstring whitespace = L"Tab:\t Newline:\n Carriage:\r Space: ";
    std::string result = wstring_to_string(whitespace);

    // Verify each character is preserved
    EXPECT_NE(result.find('\t'), std::string::npos);
    EXPECT_NE(result.find('\n'), std::string::npos);
    EXPECT_NE(result.find('\r'), std::string::npos);
    EXPECT_NE(result.find(' '), std::string::npos);
}

TEST_F(StringConversionTest, CyrillicCharacters)
{
    std::wstring cyrillic = L"Привет мир";
    std::string result = wstring_to_string(cyrillic);

    // UTF-8 encoding should be longer than original
    EXPECT_GT(result.size(), cyrillic.size());

    // Verify round-trip conversion
    EXPECT_TRUE(VerifyRoundTrip(cyrillic));
}

TEST_F(StringConversionTest, ChineseCharacters)
{
    std::wstring chinese = L"你好世界";
    std::string result = wstring_to_string(chinese);

    // UTF-8 encoding should be longer
    EXPECT_GT(result.size(), chinese.size());

    // Verify round-trip conversion
    EXPECT_TRUE(VerifyRoundTrip(chinese));
}

TEST_F(StringConversionTest, JapaneseCharacters)
{
    std::wstring japanese = L"こんにちは世界";
    std::string result = wstring_to_string(japanese);

    // UTF-8 encoding should be longer
    EXPECT_GT(result.size(), japanese.size());

    // Verify round-trip conversion
    EXPECT_TRUE(VerifyRoundTrip(japanese));
}

TEST_F(StringConversionTest, ArabicCharacters)
{
    std::wstring arabic = L"مرحبا بالعالم";
    std::string result = wstring_to_string(arabic);

    // UTF-8 encoding should be longer
    EXPECT_GT(result.size(), arabic.size());

    // Verify round-trip conversion
    EXPECT_TRUE(VerifyRoundTrip(arabic));
}

TEST_F(StringConversionTest, EmojiCharacters)
{
    std::wstring emoji = L"Hello 🌍 World 🚀 Test 😀";
    std::string result = wstring_to_string(emoji);

    // Should contain UTF-8 encoded emoji
    EXPECT_FALSE(result.empty());

    // Note: Full emoji support depends on Windows version
    // At minimum, the ASCII parts should be preserved
    EXPECT_NE(result.find("Hello"), std::string::npos);
    EXPECT_NE(result.find("World"), std::string::npos);
    EXPECT_NE(result.find("Test"), std::string::npos);
}

TEST_F(StringConversionTest, MixedLanguages)
{
    std::wstring mixed = L"English Русский 中文 日本語 العربية";
    std::string result = wstring_to_string(mixed);

    // Should handle all languages
    EXPECT_FALSE(result.empty());
    EXPECT_GT(result.size(), mixed.size());

    // Verify round-trip conversion
    EXPECT_TRUE(VerifyRoundTrip(mixed));
}

TEST_F(StringConversionTest, SingleCharacter)
{
    // Test various single characters
    std::vector<std::wstring> single_chars = {L"A", L"Z", L"0", L"9", L"!", L"€", L"™", L"©", L"®", L"°"};

    for (const auto& wch : single_chars)
    {
        std::string result = wstring_to_string(wch);
        EXPECT_FALSE(result.empty());
        EXPECT_TRUE(VerifyRoundTrip(wch));
    }
}

TEST_F(StringConversionTest, WindowsPaths)
{
    std::wstring path = L"C:\\Program Files\\My App\\data.txt";
    std::string result = wstring_to_string(path);
    EXPECT_EQ(result, "C:\\Program Files\\My App\\data.txt");
}

TEST_F(StringConversionTest, PathWithSpaces)
{
    std::wstring path = L"C:\\Users\\John Doe\\Documents\\My File.txt";
    std::string result = wstring_to_string(path);
    EXPECT_EQ(result, "C:\\Users\\John Doe\\Documents\\My File.txt");
}

TEST_F(StringConversionTest, PathWithUnicode)
{
    std::wstring path = L"C:\\Users\\用户\\文档\\файл.txt";
    std::string result = wstring_to_string(path);

    // Should preserve the path structure
    EXPECT_NE(result.find("C:\\Users\\"), std::string::npos);
    EXPECT_NE(result.find(".txt"), std::string::npos);

    // Verify round-trip
    EXPECT_TRUE(VerifyRoundTrip(path));
}

TEST_F(StringConversionTest, VeryLongString)
{
    // Create a very long string
    std::wstring long_str;
    for (int i = 0; i < 10000; ++i)
    {
        long_str += L"A";
    }

    std::string result = wstring_to_string(long_str);
    EXPECT_EQ(result.size(), 10000);

    // All characters should be 'A'
    for (char ch : result)
    {
        EXPECT_EQ(ch, 'A');
    }
}

TEST_F(StringConversionTest, StringWithNullCharacter)
{
    // String with embedded null character
    std::wstring with_null = L"Before";
    with_null.push_back(L'\0');
    with_null += L"After";

    std::string result = wstring_to_string(with_null);

    // The function uses size() so it should handle embedded nulls
    // "Before" (6 bytes) + null (1 byte) + "After" (5 bytes) = 12 bytes
    EXPECT_EQ(result.size(), 12);
    
    // Verify the content
    EXPECT_EQ(result.substr(0, 6), "Before");
    EXPECT_EQ(result[6], '\0');
    EXPECT_EQ(result.substr(7, 5), "After");
}

TEST_F(StringConversionTest, ControlCharacters)
{
    std::wstring control = L"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D";
    std::string result = wstring_to_string(control);

    // All control characters should be preserved
    EXPECT_EQ(result.size(), control.size());

    for (size_t i = 0; i < result.size(); ++i)
    {
        EXPECT_EQ(static_cast<unsigned char>(result[i]), i + 1);
    }
}

TEST_F(StringConversionTest, MaxUnicodeCharacter)
{
    // Test high Unicode code points
    std::wstring high_unicode = L"\xD800\xDC00"; // U+10000
    std::string result = wstring_to_string(high_unicode);

    // UTF-8 encoding of U+10000 is 4 bytes: 0xF0 0x90 0x80 0x80
    EXPECT_EQ(result.size(), 4);
}

TEST_F(StringConversionTest, RepeatedConversions)
{
    // Test that repeated conversions give consistent results
    std::wstring test = L"Test String 测试 тест";

    std::string result1 = wstring_to_string(test);
    std::string result2 = wstring_to_string(test);
    std::string result3 = wstring_to_string(test);

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result2, result3);
}

TEST_F(StringConversionTest, AllPrintableASCII)
{
    std::wstring all_ascii;
    for (wchar_t ch = 32; ch < 127; ++ch)
    {
        all_ascii += ch;
    }

    std::string result = wstring_to_string(all_ascii);

    // For ASCII, sizes should match
    EXPECT_EQ(result.size(), all_ascii.size());

    // Verify each character
    for (size_t i = 0; i < result.size(); ++i)
    {
        EXPECT_EQ(static_cast<unsigned char>(result[i]), i + 32);
    }
}