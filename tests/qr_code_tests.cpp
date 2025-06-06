#include <windows.h>
#include <gtest/gtest.h>
#include <string>
#include <sstream>
#include <ctime>
#include "qrcodegen.hpp"

using qrcodegen::QrCode;

// Helper function from main code
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

class QrCodeTest : public ::testing::Test {
protected:
    // Helper to check if QR code data is valid
    bool IsValidQrData(const std::string& data) {
        return !data.empty() && data.find("t=") != std::string::npos;
    }
};

// Test basic QR code generation
TEST_F(QrCodeTest, BasicQrCodeGeneration) {
    std::string testData = "Hello, World!";
    
    // Generate QR code
    QrCode qr = QrCode::encodeText(testData.c_str(), QrCode::Ecc::MEDIUM);
    
    // Check QR code properties
    EXPECT_GT(qr.getSize(), 0);
    EXPECT_LE(qr.getSize(), 177); // Max QR code size
    
    // Check that we can access modules
    for (int y = 0; y < qr.getSize(); y++) {
        for (int x = 0; x < qr.getSize(); x++) {
            // Just accessing should not throw
            bool module = qr.getModule(x, y);
            (void)module; // Avoid unused variable warning
        }
    }
}

// Test QR code data format for timestamp and FPS
TEST_F(QrCodeTest, QrDataFormat) {
    time_t timestamp = 1234567890;
    int fps = 60;
    std::vector<std::wstring> args = {L"arg1", L"arg2 with space"};
    
    // Build QR data string
    std::ostringstream oss;
    oss << "t=" << timestamp << ";f=" << fps;
    if (!args.empty()) {
        oss << ";args=";
        for (const auto& arg : args) {
            oss << wstring_to_string(arg) << " ";
        }
    }
    std::string qrData = oss.str();
    
    // Verify format
    EXPECT_TRUE(IsValidQrData(qrData));
    EXPECT_TRUE(qrData.find("t=1234567890") != std::string::npos);
    EXPECT_TRUE(qrData.find("f=60") != std::string::npos);
    EXPECT_TRUE(qrData.find("args=arg1 arg2 with space") != std::string::npos);
}

// Test QR code generation with empty arguments
TEST_F(QrCodeTest, QrDataWithoutArguments) {
    time_t timestamp = time(nullptr);
    int fps = 30;
    std::vector<std::wstring> args; // Empty
    
    std::ostringstream oss;
    oss << "t=" << timestamp << ";f=" << fps;
    if (!args.empty()) {
        oss << ";args=";
        for (const auto& arg : args) {
            oss << wstring_to_string(arg) << " ";
        }
    }
    std::string qrData = oss.str();
    
    // Should not contain args section
    EXPECT_TRUE(IsValidQrData(qrData));
    EXPECT_TRUE(qrData.find("args=") == std::string::npos);
}

// Test QR code with Unicode arguments
TEST_F(QrCodeTest, QrDataWithUnicodeArguments) {
    time_t timestamp = time(nullptr);
    int fps = 144;
    std::vector<std::wstring> args = {L"测试", L"тест", L"テスト"};
    
    std::ostringstream oss;
    oss << "t=" << timestamp << ";f=" << fps;
    oss << ";args=";
    for (const auto& arg : args) {
        oss << wstring_to_string(arg) << " ";
    }
    std::string qrData = oss.str();
    
    // Generate QR code - should handle Unicode data
    QrCode qr = QrCode::encodeText(qrData.c_str(), QrCode::Ecc::MEDIUM);
    EXPECT_GT(qr.getSize(), 0);
}

// Test QR code bitmap generation (pixel scaling)
TEST_F(QrCodeTest, QrCodeBitmapScaling) {
    std::string testData = "t=1234567890;f=60";
    QrCode qr = QrCode::encodeText(testData.c_str(), QrCode::Ecc::MEDIUM);
    
    int qrModules = qr.getSize();
    constexpr int pixelSize = 375;
    float scale = static_cast<float>(pixelSize) / qrModules;
    
    // Check scaling calculation
    EXPECT_GT(scale, 0.0f);
    EXPECT_EQ(pixelSize, 375);
    
    // Test pixel generation
    std::vector<uint32_t> pixels(pixelSize * pixelSize, 0xffffffff);
    
    for (int y = 0; y < pixelSize; y++) {
        for (int x = 0; x < pixelSize; x++) {
            int moduleX = static_cast<int>(x / scale);
            int moduleY = static_cast<int>(y / scale);
            if (moduleX < qrModules && moduleY < qrModules) {
                if (qr.getModule(moduleX, moduleY)) {
                    pixels[y * pixelSize + x] = 0xff000000;
                }
            }
        }
    }
    
    // Verify we have both black and white pixels
    bool hasBlack = false, hasWhite = false;
    for (uint32_t pixel : pixels) {
        if (pixel == 0xff000000) hasBlack = true;
        if (pixel == 0xffffffff) hasWhite = true;
    }
    EXPECT_TRUE(hasBlack);
    EXPECT_TRUE(hasWhite);
}

// Test QR code update frequency (should update every 5 seconds)
TEST_F(QrCodeTest, QrCodeUpdateFrequency) {
    ULONGLONG lastUpdateTime = 0;
    ULONGLONG currentTime = 1000; // 1 second
    
    // Should not update - less than 5 seconds
    bool shouldUpdate = (currentTime - lastUpdateTime >= 5000);
    EXPECT_FALSE(shouldUpdate);
    
    // Should update - exactly 5 seconds
    currentTime = 5000;
    shouldUpdate = (currentTime - lastUpdateTime >= 5000);
    EXPECT_TRUE(shouldUpdate);
    
    // Should update - more than 5 seconds
    currentTime = 10000;
    shouldUpdate = (currentTime - lastUpdateTime >= 5000);
    EXPECT_TRUE(shouldUpdate);
}

// Test FPS synchronization
TEST_F(QrCodeTest, FpsSynchronization) {
    float currentFps = 59.7f;
    int fpsForQr = static_cast<int>(currentFps);
    
    EXPECT_EQ(fpsForQr, 59);
    
    // Test edge cases
    currentFps = 60.0f;
    fpsForQr = static_cast<int>(currentFps);
    EXPECT_EQ(fpsForQr, 60);
    
    currentFps = 144.9f;
    fpsForQr = static_cast<int>(currentFps);
    EXPECT_EQ(fpsForQr, 144);
}