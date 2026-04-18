// Unit tests for the pure DSP helpers in audio_capture::detail. These don't
// require a live microphone — they operate on in-memory byte buffers — so
// they're safe to run in CI on any Windows build agent.

// clang-format off
#include <windows.h>
#include <mmreg.h>
#include <ks.h>        // must precede <ksmedia.h>
#include <ksmedia.h>
// clang-format on

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "../audio_capture.hpp"

using audio_capture::detail::PeakFloat32;
using audio_capture::detail::PeakForFormat;
using audio_capture::detail::PeakPcm16;
using audio_capture::detail::PeakPcm24;
using audio_capture::detail::PeakPcm32;
using audio_capture::detail::ResolveFormat;
using audio_capture::detail::SampleFormat;

namespace
{

std::vector<BYTE> BytesFromFloats(std::initializer_list<float> values)
{
    std::vector<BYTE> bytes(values.size() * sizeof(float));
    std::memcpy(bytes.data(), std::data(values), bytes.size());
    return bytes;
}

std::vector<BYTE> BytesFromInt16s(std::initializer_list<int16_t> values)
{
    std::vector<BYTE> bytes(values.size() * sizeof(int16_t));
    std::memcpy(bytes.data(), std::data(values), bytes.size());
    return bytes;
}

std::vector<BYTE> BytesFromInt32s(std::initializer_list<int32_t> values)
{
    std::vector<BYTE> bytes(values.size() * sizeof(int32_t));
    std::memcpy(bytes.data(), std::data(values), bytes.size());
    return bytes;
}

// 24-bit PCM is stored as three little-endian bytes per sample.
std::vector<BYTE> Bytes24FromInt32s(std::initializer_list<int32_t> values)
{
    std::vector<BYTE> bytes;
    bytes.reserve(values.size() * 3);
    for (int32_t v : values)
    {
        bytes.push_back(static_cast<BYTE>(v & 0xFF));
        bytes.push_back(static_cast<BYTE>((v >> 8) & 0xFF));
        bytes.push_back(static_cast<BYTE>((v >> 16) & 0xFF));
    }
    return bytes;
}

} // namespace

// ---------------------------------------------------------------------------
// PeakFloat32 — 32-bit IEEE_FLOAT stream
// ---------------------------------------------------------------------------

TEST(PeakFloat32, PicksMaxAbsoluteValue)
{
    auto buf = BytesFromFloats({0.2f, -0.7f, 0.5f, -0.3f});
    EXPECT_FLOAT_EQ(PeakFloat32(buf.data(), 4), 0.7f);
}

TEST(PeakFloat32, AllZerosYieldsZero)
{
    auto buf = BytesFromFloats({0.f, 0.f, 0.f});
    EXPECT_FLOAT_EQ(PeakFloat32(buf.data(), 3), 0.f);
}

TEST(PeakFloat32, UnitySampleYieldsOne)
{
    auto buf = BytesFromFloats({-1.0f, 0.0f, 1.0f});
    EXPECT_FLOAT_EQ(PeakFloat32(buf.data(), 3), 1.0f);
}

// ---------------------------------------------------------------------------
// PeakPcm16 — signed 16-bit PCM normalised by 32768
// ---------------------------------------------------------------------------

TEST(PeakPcm16, MaxPositiveNearOne)
{
    auto buf = BytesFromInt16s({0, 16384, -8192});
    EXPECT_NEAR(PeakPcm16(buf.data(), 3), 16384.0f / 32768.0f, 1e-6f);
}

TEST(PeakPcm16, MinNegativeYieldsOne)
{
    // INT16_MIN / 32768 = -1.0 exactly → |.| = 1.0
    auto buf = BytesFromInt16s({0, std::numeric_limits<int16_t>::min()});
    EXPECT_FLOAT_EQ(PeakPcm16(buf.data(), 2), 1.0f);
}

TEST(PeakPcm16, SilenceYieldsZero)
{
    auto buf = BytesFromInt16s({0, 0, 0, 0});
    EXPECT_FLOAT_EQ(PeakPcm16(buf.data(), 4), 0.f);
}

// ---------------------------------------------------------------------------
// PeakPcm24 — 3 bytes per sample, little-endian, normalised by 2^23
// ---------------------------------------------------------------------------

TEST(PeakPcm24, SignExtendsNegative)
{
    // -0x400000 is half-scale negative (≈ -0.5). Test that the sign bit is
    // preserved by the 3-byte → int32 decode.
    auto buf = Bytes24FromInt32s({0, -0x400000, 0x200000});
    const float peak = PeakPcm24(buf.data(), 3);
    EXPECT_NEAR(peak, 0.5f, 1e-5f);
}

TEST(PeakPcm24, FullScalePositive)
{
    auto buf = Bytes24FromInt32s({0x7FFFFF});
    EXPECT_NEAR(PeakPcm24(buf.data(), 1), 0x7FFFFF / 8388608.0f, 1e-6f);
}

TEST(PeakPcm24, MinValueHitsUnity)
{
    // -0x800000 / 2^23 = -1.0 exactly.
    auto buf = Bytes24FromInt32s({-0x800000});
    EXPECT_FLOAT_EQ(PeakPcm24(buf.data(), 1), 1.0f);
}

// ---------------------------------------------------------------------------
// PeakPcm32 — signed 32-bit PCM normalised by 2^31
// ---------------------------------------------------------------------------

TEST(PeakPcm32, HalfScale)
{
    auto buf = BytesFromInt32s({0, 0x40000000, -0x20000000});
    EXPECT_NEAR(PeakPcm32(buf.data(), 3), 0.5f, 1e-6f);
}

TEST(PeakPcm32, MinNegativeYieldsOne)
{
    auto buf = BytesFromInt32s({std::numeric_limits<int32_t>::min()});
    EXPECT_FLOAT_EQ(PeakPcm32(buf.data(), 1), 1.0f);
}

// ---------------------------------------------------------------------------
// PeakForFormat — dispatch by tag/bps
// ---------------------------------------------------------------------------

TEST(PeakForFormat, DispatchesFloat32)
{
    SampleFormat sf{WAVE_FORMAT_IEEE_FLOAT, 32, 1};
    auto buf = BytesFromFloats({0.25f, -0.8f});
    EXPECT_FLOAT_EQ(PeakForFormat(sf, buf.data(), 2), 0.8f);
}

TEST(PeakForFormat, DispatchesPcm16)
{
    SampleFormat sf{WAVE_FORMAT_PCM, 16, 1};
    auto buf = BytesFromInt16s({0, -16384});
    EXPECT_NEAR(PeakForFormat(sf, buf.data(), 2), 0.5f, 1e-5f);
}

TEST(PeakForFormat, DispatchesPcm24)
{
    SampleFormat sf{WAVE_FORMAT_PCM, 24, 1};
    auto buf = Bytes24FromInt32s({-0x400000});
    EXPECT_NEAR(PeakForFormat(sf, buf.data(), 1), 0.5f, 1e-5f);
}

TEST(PeakForFormat, DispatchesPcm32)
{
    SampleFormat sf{WAVE_FORMAT_PCM, 32, 1};
    auto buf = BytesFromInt32s({0x40000000});
    EXPECT_NEAR(PeakForFormat(sf, buf.data(), 1), 0.5f, 1e-6f);
}

TEST(PeakForFormat, UnsupportedFormatReturnsZero)
{
    // 8-bit PCM and A-law/µ-law are not implemented. Peak should degrade to 0
    // rather than reinterpret memory or crash.
    SampleFormat sf{WAVE_FORMAT_PCM, 8, 1};
    std::vector<BYTE> buf = {0xFF, 0x00, 0x80};
    EXPECT_FLOAT_EQ(PeakForFormat(sf, buf.data(), 3), 0.f);

    SampleFormat alaw{WAVE_FORMAT_ALAW, 8, 1};
    EXPECT_FLOAT_EQ(PeakForFormat(alaw, buf.data(), 3), 0.f);
}

// ---------------------------------------------------------------------------
// ResolveFormat — plain WAVEFORMATEX and WAVEFORMATEXTENSIBLE handling
// ---------------------------------------------------------------------------

TEST(ResolveFormat, PassesThroughPlainPcm16Stereo)
{
    WAVEFORMATEX mix{};
    mix.wFormatTag = WAVE_FORMAT_PCM;
    mix.nChannels = 2;
    mix.wBitsPerSample = 16;
    mix.cbSize = 0;

    auto sf = ResolveFormat(&mix);
    EXPECT_EQ(sf.tag, WAVE_FORMAT_PCM);
    EXPECT_EQ(sf.bps, 16);
    EXPECT_EQ(sf.channels, 2u);
}

TEST(ResolveFormat, PassesThroughPlainFloat32Mono)
{
    WAVEFORMATEX mix{};
    mix.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    mix.nChannels = 1;
    mix.wBitsPerSample = 32;

    auto sf = ResolveFormat(&mix);
    EXPECT_EQ(sf.tag, WAVE_FORMAT_IEEE_FLOAT);
    EXPECT_EQ(sf.bps, 32);
    EXPECT_EQ(sf.channels, 1u);
}

TEST(ResolveFormat, UnwrapsExtensibleFloat32)
{
    WAVEFORMATEXTENSIBLE wfex{};
    wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels = 2;
    wfex.Format.wBitsPerSample = 32;
    wfex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfex.Samples.wValidBitsPerSample = 32;
    wfex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    auto sf = ResolveFormat(reinterpret_cast<const WAVEFORMATEX*>(&wfex));
    EXPECT_EQ(sf.tag, WAVE_FORMAT_IEEE_FLOAT);
    EXPECT_EQ(sf.bps, 32);
    EXPECT_EQ(sf.channels, 2u);
}

TEST(ResolveFormat, UnwrapsExtensiblePcmWithValidBits)
{
    // Device declares a 32-bit container with only 24 valid bits (common on
    // pro audio cards). ResolveFormat should prefer wValidBitsPerSample.
    WAVEFORMATEXTENSIBLE wfex{};
    wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels = 2;
    wfex.Format.wBitsPerSample = 32;
    wfex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfex.Samples.wValidBitsPerSample = 24;
    wfex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    auto sf = ResolveFormat(reinterpret_cast<const WAVEFORMATEX*>(&wfex));
    EXPECT_EQ(sf.tag, WAVE_FORMAT_PCM);
    EXPECT_EQ(sf.bps, 24);
}

TEST(ResolveFormat, ExtensibleWithZeroChannelsDefaultsToMono)
{
    WAVEFORMATEX mix{};
    mix.wFormatTag = WAVE_FORMAT_PCM;
    mix.nChannels = 0; // broken device description
    mix.wBitsPerSample = 16;

    auto sf = ResolveFormat(&mix);
    EXPECT_EQ(sf.channels, 1u);
}
