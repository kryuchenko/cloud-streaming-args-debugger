#pragma once

// clang-format off
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
// clang-format on

#include <atomic>
#include <string>

#include <wrl/client.h>

// WASAPI microphone capture in shared mode with event-driven pumping on its
// own background thread. The thread body is reached through seh_wrapper.cpp
// so that SEH faults in the audio stack (e.g. device disconnect during
// GetBuffer) can be turned into a logged termination instead of crashing
// the process.
//
// Ownership: the object owns the thread, the event handle and all COM
// interfaces. Destruction calls Stop() — callers usually prefer to call
// Stop() explicitly on shutdown so they can observe any failure.
class AudioCapture
{
  public:
    AudioCapture() = default;
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    // Bring up the WASAPI pipeline and spin up the capture thread. Logs and
    // returns false on failure instead of throwing — a missing microphone
    // must not take down the rest of the UI.
    bool Initialize();

    // Cooperatively stop the capture thread. Signals the event, waits for
    // the thread with the given timeout, and (as a last resort) terminates
    // it. Safe to call multiple times and from the destructor.
    void Stop(DWORD timeout_ms = 5000);

    bool IsAvailable() const
    {
        return mic_available_.load();
    }
    float Level() const
    {
        return mic_level_.load();
    }
    const std::wstring& Name() const
    {
        return mic_name_;
    }

    // Entry point invoked by the SEH wrapper in seh_wrapper.cpp.
    // Returns the thread exit code.
    DWORD ThreadMain();

  private:
    void PollOnce();
    float ComputePeak(const BYTE* data, UINT32 frames, DWORD flags) const;

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> device_enumerator_;
    Microsoft::WRL::ComPtr<IMMDevice> capture_device_;
    Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_client_;
    WAVEFORMATEX* mix_format_ = nullptr;
    HANDLE audio_event_ = nullptr;
    HANDLE audio_thread_ = nullptr;
    std::atomic<float> mic_level_{0.f};
    std::atomic<bool> mic_available_{false};
    std::atomic<bool> thread_running_{false};
    std::wstring mic_name_;
};

// Pure DSP helpers. Deliberately exposed so unit tests can exercise them
// without bringing up a real WASAPI capture session.
namespace audio_capture::detail
{

struct SampleFormat
{
    WORD tag;
    WORD bps;
    UINT32 channels;
};

SampleFormat ResolveFormat(const WAVEFORMATEX* mix);

float PeakFloat32(const BYTE* data, UINT32 total_samples);
float PeakPcm16(const BYTE* data, UINT32 total_samples);
float PeakPcm24(const BYTE* data, UINT32 total_samples);
float PeakPcm32(const BYTE* data, UINT32 total_samples);
float PeakForFormat(const SampleFormat& sf, const BYTE* data, UINT32 total_samples);

} // namespace audio_capture::detail
