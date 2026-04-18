#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include "audio_capture.hpp"

// clang-format off
#include <windows.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ks.h>
#include <ksmedia.h>
#include <propsys.h>
#include <propvarutil.h>
// clang-format on

#include <cstdint>
#include <stdexcept>

#include "log_manager.hpp"

#pragma comment(lib, "ole32")
#pragma comment(lib, "avrt")
#pragma comment(lib, "propsys")

using Microsoft::WRL::ComPtr;

// Forward declaration of the SEH-guarded thread entry that hands control
// back to AudioCapture::ThreadMain via static_cast on the parameter.
extern "C" DWORD WINAPI RawAudioThreadWithSEH(LPVOID param) noexcept;

// Helper for DX_CALL-style throw on HRESULT failure. Duplicates the macro
// from cli_args_debugger.cpp to keep this TU independent.
#define AC_CALL(expr, msg)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        HRESULT _hr = (expr);                                                                                          \
        if (FAILED(_hr))                                                                                               \
            throw std::runtime_error(msg);                                                                             \
    } while (0)

namespace audio_capture::detail
{

// Resolve tag / bits-per-sample for WAVEFORMATEXTENSIBLE, falling back to
// the base WAVEFORMATEX fields for plain PCM/IEEE_FLOAT streams.
SampleFormat ResolveFormat(const WAVEFORMATEX* mix)
{
    SampleFormat sf{mix->wFormatTag, mix->wBitsPerSample, mix->nChannels};

    if (sf.tag == WAVE_FORMAT_EXTENSIBLE)
    {
        if (mix->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
        {
            auto* wfex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mix);
            if (IsEqualGUID(wfex->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
                sf.tag = WAVE_FORMAT_IEEE_FLOAT;
            else if (IsEqualGUID(wfex->SubFormat, KSDATAFORMAT_SUBTYPE_PCM))
                sf.tag = WAVE_FORMAT_PCM;
            else
                Log(L"Unknown SubFormat GUID in WAVE_FORMAT_EXTENSIBLE");

            sf.bps =
                wfex->Samples.wValidBitsPerSample ? wfex->Samples.wValidBitsPerSample : wfex->Format.wBitsPerSample;
        }
        else
        {
            Log(L"WAVE_FORMAT_EXTENSIBLE with invalid cbSize: " + std::to_wstring(mix->cbSize));
            sf.tag = WAVE_FORMAT_PCM;
            sf.bps = mix->wBitsPerSample;
        }
    }
    if (sf.channels == 0)
    {
        Log(L"Invalid channel count: 0");
        sf.channels = 1;
    }
    return sf;
}

float PeakFloat32(const BYTE* data, UINT32 total_samples)
{
    float peak = 0.f;
    const auto* samples = reinterpret_cast<const float*>(data);
    for (UINT32 i = 0; i < total_samples; ++i)
    {
        float v = samples[i];
        if (v < 0)
            v = -v;
        if (v > peak)
            peak = v;
    }
    return peak;
}

float PeakPcm16(const BYTE* data, UINT32 total_samples)
{
    float peak = 0.f;
    const auto* samples = reinterpret_cast<const int16_t*>(data);
    for (UINT32 i = 0; i < total_samples; ++i)
    {
        float v = samples[i] / 32768.0f;
        if (v < 0)
            v = -v;
        if (v > peak)
            peak = v;
    }
    return peak;
}

float PeakPcm24(const BYTE* data, UINT32 total_samples)
{
    float peak = 0.f;
    const auto* p = reinterpret_cast<const uint8_t*>(data);
    for (UINT32 i = 0; i < total_samples; ++i)
    {
        int32_t sample = (p[3 * i] << 8) | (p[3 * i + 1] << 16) | (p[3 * i + 2] << 24);
        sample >>= 8;
        float v = sample / 8388608.0f;
        if (v < 0)
            v = -v;
        if (v > peak)
            peak = v;
    }
    return peak;
}

float PeakPcm32(const BYTE* data, UINT32 total_samples)
{
    float peak = 0.f;
    const auto* samples = reinterpret_cast<const int32_t*>(data);
    for (UINT32 i = 0; i < total_samples; ++i)
    {
        float v = samples[i] / 2147483648.0f;
        if (v < 0)
            v = -v;
        if (v > peak)
            peak = v;
    }
    return peak;
}

float PeakForFormat(const SampleFormat& sf, const BYTE* data, UINT32 total_samples)
{
    if (sf.tag == WAVE_FORMAT_IEEE_FLOAT && sf.bps == 32)
        return PeakFloat32(data, total_samples);
    if (sf.tag == WAVE_FORMAT_PCM && sf.bps == 16)
        return PeakPcm16(data, total_samples);
    if (sf.tag == WAVE_FORMAT_PCM && sf.bps == 24)
        return PeakPcm24(data, total_samples);
    if (sf.tag == WAVE_FORMAT_PCM && sf.bps == 32)
        return PeakPcm32(data, total_samples);

    Log(L"Unsupported audio format: tag=" + std::to_wstring(sf.tag) + L", bps=" + std::to_wstring(sf.bps));
    return 0.f;
}

} // namespace audio_capture::detail

AudioCapture::~AudioCapture()
{
    Stop();

    capture_client_.Reset();
    audio_client_.Reset();
    capture_device_.Reset();
    device_enumerator_.Reset();

    if (mix_format_)
    {
        CoTaskMemFree(mix_format_);
        mix_format_ = nullptr;
    }
}

bool AudioCapture::Initialize()
{
    try
    {
        AC_CALL(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&device_enumerator_)),
                "IMMDeviceEnumerator failed");
        AC_CALL(device_enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, capture_device_.GetAddressOf()),
                "No default capture device");

        ComPtr<IPropertyStore> store;
        AC_CALL(capture_device_->OpenPropertyStore(STGM_READ, &store), "OpenPropertyStore failed");
        PROPVARIANT pv;
        PropVariantInit(&pv);
        AC_CALL(store->GetValue(PKEY_Device_FriendlyName, &pv), "GetValue(FriendlyName) failed");
        mic_name_ = pv.vt == VT_LPWSTR ? pv.pwszVal : L"Unknown microphone";
        PropVariantClear(&pv);

        mic_available_ = true;

        AC_CALL(capture_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                          reinterpret_cast<void**>(audio_client_.GetAddressOf())),
                "IAudioClient activate failed");

        AC_CALL(audio_client_->GetMixFormat(&mix_format_), "GetMixFormat failed");

        // 100ms buffer for smoother level visualisation.
        REFERENCE_TIME buf_dur = 100 * 10000;
        AC_CALL(audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, buf_dur, 0,
                                          mix_format_, nullptr),
                "AudioClient init failed");
        AC_CALL(audio_client_->GetService(IID_PPV_ARGS(&capture_client_)), "GetService(IAudioCaptureClient)");

        audio_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!audio_event_)
            throw std::runtime_error("Failed to create audio event");
        audio_client_->SetEventHandle(audio_event_);

        thread_running_.store(true);
        audio_thread_ = CreateThread(nullptr, 0, RawAudioThreadWithSEH, this, 0, nullptr);
        if (!audio_thread_)
        {
            thread_running_.store(false);
            throw std::runtime_error("CreateThread failed for audio capture");
        }

        Log(L"Microphone initialized successfully");
        return true;
    }
    catch (const std::exception& ex)
    {
        Log(L"Microphone initialization failed: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
        mic_available_ = false;
        return false;
    }
}

void AudioCapture::Stop(DWORD timeout_ms)
{
    thread_running_.store(false);
    if (audio_event_)
        SetEvent(audio_event_);

    if (audio_thread_)
    {
        DWORD wait_result = WaitForSingleObject(audio_thread_, timeout_ms);
        if (wait_result == WAIT_TIMEOUT)
        {
            Log(L"WARNING: Audio thread did not terminate gracefully, forcing termination");
            TerminateThread(audio_thread_, 0);
        }
        CloseHandle(audio_thread_);
        audio_thread_ = nullptr;
    }

    if (audio_event_)
    {
        CloseHandle(audio_event_);
        audio_event_ = nullptr;
    }

    if (audio_client_)
    {
        try
        {
            audio_client_->Stop();
            Log(L"Audio client stopped");
        }
        catch (...)
        {
            Log(L"Exception stopping audio client");
        }
    }
}

DWORD AudioCapture::ThreadMain()
{
    // Each thread needs its own COM apartment.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return 0;

    HANDLE mm_handle = nullptr;
    try
    {
        DWORD task_index = 0;
        mm_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
        if (!mm_handle)
            Log(L"Warning: AvSetMmThreadCharacteristicsW failed");

        audio_client_->Start();
        Log(L"Audio capture thread started");

        while (thread_running_.load())
        {
            DWORD wait = WaitForSingleObject(audio_event_, 200);
            if (wait == WAIT_OBJECT_0)
            {
                Log(L"Audio thread: signal received");
            }
            else if (wait == WAIT_TIMEOUT)
            {
                static ULONGLONG last_timeout_log = 0;
                ULONGLONG now = GetTickCount64();
                if (now - last_timeout_log > 30000)
                {
                    Log(L"Audio thread: timeout (normal)");
                    last_timeout_log = now;
                }
            }
            else
            {
                Log(L"Audio thread: wait failed, code=" + std::to_wstring(wait));
            }

            PollOnce();
        }

        audio_client_->Stop();
        Log(L"Audio capture thread stopped");
    }
    catch (const std::exception& ex)
    {
        Log(L"Audio thread exception: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
    }
    catch (...)
    {
        Log(L"Audio thread: unknown exception");
    }

    if (mm_handle)
        AvRevertMmThreadCharacteristics(mm_handle);
    CoUninitialize();
    return 0;
}

void AudioCapture::PollOnce()
{
    try
    {
        if (!thread_running_.load() || !capture_client_)
            return;

        UINT32 pkt_len = 0;
        HRESULT hr = capture_client_->GetNextPacketSize(&pkt_len);
        if (FAILED(hr))
        {
            Log(L"PollMicrophone: Initial GetNextPacketSize failed, hr=0x" + std::to_wstring(hr));
            return;
        }
        if (pkt_len == 0)
            return;

        while (pkt_len > 0)
        {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            hr = capture_client_->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            if (FAILED(hr))
            {
                Log(L"PollMicrophone: GetBuffer failed, hr=0x" + std::to_wstring(hr));
                break;
            }

            if (!data || frames == 0)
            {
                Log(L"PollMicrophone: Invalid buffer — data=" + std::wstring(data ? L"valid" : L"NULL") + L", frames=" +
                    std::to_wstring(frames));
                capture_client_->ReleaseBuffer(frames);
                break;
            }

            float peak = ComputePeak(data, frames, flags);
            float current_level = mic_level_.load();
            mic_level_.store(current_level * 0.5f + peak * 0.5f);

            if (!thread_running_.load())
            {
                Log(L"PollMicrophone: Thread signaled to exit, breaking");
                capture_client_->ReleaseBuffer(frames);
                break;
            }

            hr = capture_client_->ReleaseBuffer(frames);
            if (FAILED(hr))
            {
                Log(L"PollMicrophone: ReleaseBuffer failed, hr=0x" + std::to_wstring(hr));
                break;
            }

            if (!thread_running_.load())
            {
                Log(L"PollMicrophone: Thread signaled to exit before next packet");
                break;
            }

            hr = capture_client_->GetNextPacketSize(&pkt_len);
            if (FAILED(hr))
            {
                Log(L"PollMicrophone: GetNextPacketSize failed after processing, hr=0x" + std::to_wstring(hr));
                break;
            }
        }
    }
    catch (const std::exception& ex)
    {
        Log(L"PollMicrophone exception: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
    }
    catch (...)
    {
        Log(L"PollMicrophone: unknown exception");
    }
}

float AudioCapture::ComputePeak(const BYTE* data, UINT32 frames, DWORD flags) const
{
    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !data || !mix_format_)
        return 0.f;

    using namespace audio_capture::detail;
    const SampleFormat sf = ResolveFormat(mix_format_);

    UINT64 total64 = static_cast<UINT64>(frames) * static_cast<UINT64>(sf.channels);
    if (total64 > UINT32_MAX)
    {
        Log(L"Sample count overflow: " + std::to_wstring(total64));
        return 0.f;
    }
    const UINT32 total_samples = static_cast<UINT32>(total64);

    return PeakForFormat(sf, data, total_samples);
}
