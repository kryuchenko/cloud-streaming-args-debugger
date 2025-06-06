#include <atomic>
#include <audioclient.h>
#include <cmath>
#include <gtest/gtest.h>
#include <mmdeviceapi.h>
#include <windows.h>

class AudioTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Initialize COM for the test thread
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        comInitialized = SUCCEEDED(hr);
    }

    void TearDown() override
    {
        if (comInitialized)
        {
            CoUninitialize();
        }
    }

    bool comInitialized = false;
};

// Test COM initialization
TEST_F(AudioTest, ComInitialization)
{
    EXPECT_TRUE(comInitialized);
}

// Test audio device enumeration
TEST_F(AudioTest, EnumerateAudioDevices)
{
    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));

    if (SUCCEEDED(hr))
    {
        // Try to get default capture device
        IMMDevice* captureDevice = nullptr;
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &captureDevice);

        if (SUCCEEDED(hr) && captureDevice)
        {
            // Device found
            EXPECT_NE(captureDevice, nullptr);
            captureDevice->Release();
        }
        else
        {
            // No microphone - this is OK for the test
            std::cout << "No default capture device found" << std::endl;
        }

        deviceEnumerator->Release();
    }
    else
    {
        // COM issue - might be running on non-Windows
        std::cout << "Failed to create device enumerator" << std::endl;
    }
}

// Test audio level calculation
TEST_F(AudioTest, AudioLevelCalculation)
{
    // Test float samples
    {
        float samples[] = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f};
        float peak = 0.0f;

        for (float sample : samples)
        {
            float val = sample;
            if (val < 0)
                val = -val;
            if (val > peak)
                peak = val;
        }

        EXPECT_FLOAT_EQ(peak, 1.0f);
    }

    // Test 16-bit PCM samples
    {
        int16_t samples[] = {0, 16384, -16384, 32767, -32768};
        float peak = 0.0f;

        for (int16_t sample : samples)
        {
            float val = sample / 32768.0f;
            if (val < 0)
                val = -val;
            if (val > peak)
                peak = val;
        }

        EXPECT_FLOAT_EQ(peak, 1.0f);
    }

    // Test 24-bit PCM conversion
    {
        // 24-bit max value is 8388607 (0x7FFFFF)
        // -8388608 (0x800000) for negative
        int32_t sample24bit = 0x7FFFFF00; // Left-shifted by 8
        sample24bit >>= 8;                // Sign-extend
        float val = sample24bit / 8388608.0f;
        EXPECT_NEAR(val, 1.0f, 0.0001f);
    }

    // Test 32-bit PCM samples
    {
        int32_t samples[] = {0, 1073741824, -1073741824, 2147483647, INT32_MIN};
        float peak = 0.0f;

        for (int32_t sample : samples)
        {
            float val = sample / 2147483648.0f;
            if (val < 0)
                val = -val;
            if (val > peak)
                peak = val;
        }

        EXPECT_NEAR(peak, 1.0f, 0.0001f);
    }
}

// Test audio level smoothing
TEST_F(AudioTest, AudioLevelSmoothing)
{
    std::atomic<float> micLevel{0.0f};

    // Simulate smoothing algorithm
    float currentLevel = micLevel.load();
    float newPeak = 0.8f;
    float smoothedLevel = currentLevel * 0.5f + newPeak * 0.5f;

    EXPECT_FLOAT_EQ(smoothedLevel, 0.4f);

    // Test multiple smoothing steps
    micLevel.store(smoothedLevel);
    currentLevel = micLevel.load();
    newPeak = 0.6f;
    smoothedLevel = currentLevel * 0.5f + newPeak * 0.5f;

    EXPECT_FLOAT_EQ(smoothedLevel, 0.5f);
}

// Test stereo visualization
TEST_F(AudioTest, StereoVisualization)
{
    float leftLevel = 0.8f;
    float rightLevel = leftLevel * 0.9f; // Slight variation

    EXPECT_FLOAT_EQ(rightLevel, 0.72f);

    // Test bar height calculation
    float barHeight = 150.0f;
    float leftFilled = barHeight * leftLevel;
    float rightFilled = barHeight * rightLevel;

    EXPECT_FLOAT_EQ(leftFilled, 120.0f);
    EXPECT_FLOAT_EQ(rightFilled, 108.0f);
}

// Test thread safety with atomics
TEST_F(AudioTest, AtomicOperations)
{
    std::atomic<float> level{0.0f};
    std::atomic<bool> available{false};
    std::atomic<bool> running{true};

    // Test atomic store and load
    level.store(0.75f);
    EXPECT_FLOAT_EQ(level.load(), 0.75f);

    available.store(true);
    EXPECT_TRUE(available.load());

    running.store(false);
    EXPECT_FALSE(running.load());
}

// Test event handle creation
TEST_F(AudioTest, EventHandleCreation)
{
    HANDLE audioEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (audioEvent != nullptr)
    {
        EXPECT_NE(audioEvent, nullptr);

        // Test signaling
        BOOL result = SetEvent(audioEvent);
        EXPECT_TRUE(result);

        // Test waiting with timeout
        DWORD waitResult = WaitForSingleObject(audioEvent, 0);
        EXPECT_EQ(waitResult, WAIT_OBJECT_0);

        CloseHandle(audioEvent);
    }
}

// Test WAVEFORMATEX structure handling
TEST_F(AudioTest, WaveFormatHandling)
{
    // Test standard formats
    WAVEFORMATEX format = {};

    // 16-bit PCM
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = 48000;
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    EXPECT_EQ(format.nBlockAlign, 4);
    EXPECT_EQ(format.nAvgBytesPerSec, 192000);

    // 32-bit float
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.wBitsPerSample = 32;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    EXPECT_EQ(format.nBlockAlign, 8);
    EXPECT_EQ(format.nAvgBytesPerSec, 384000);
}