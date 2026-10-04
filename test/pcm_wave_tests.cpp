#include <gtest/gtest.h>

#include "../include/pcm_wave.h"

#include <string>
#include <utility>
#include <vector>

namespace {

void appendBytes(std::vector<uint8_t> &out, const uint8_t *bytes, size_t count) {
    out.insert(out.end(), bytes, bytes + count);
}

void appendLe16(std::vector<uint8_t> &out, uint16_t value) {
    const uint8_t bytes[2] = {
        static_cast<uint8_t>(value & 0xffu),
        static_cast<uint8_t>((value >> 8) & 0xffu)
    };
    appendBytes(out, bytes, 2);
}

void appendLe32(std::vector<uint8_t> &out, uint32_t value) {
    const uint8_t bytes[4] = {
        static_cast<uint8_t>(value & 0xffu),
        static_cast<uint8_t>((value >> 8) & 0xffu),
        static_cast<uint8_t>((value >> 16) & 0xffu),
        static_cast<uint8_t>((value >> 24) & 0xffu)
    };
    appendBytes(out, bytes, 4);
}

void appendId(std::vector<uint8_t> &out, const char *id) {
    appendBytes(out, reinterpret_cast<const uint8_t *>(id), 4);
}

struct Chunk {
    char id[5];
    std::vector<uint8_t> payload;
};

std::vector<uint8_t> makeRiff(const std::vector<Chunk> &chunks) {
    std::vector<uint8_t> body;
    for (const Chunk &chunk : chunks) {
        appendId(body, chunk.id);
        appendLe32(body, static_cast<uint32_t>(chunk.payload.size()));
        appendBytes(body, chunk.payload.data(), chunk.payload.size());
        if ((chunk.payload.size() & 1u) != 0u) {
            body.push_back(0);
        }
    }

    std::vector<uint8_t> file;
    appendId(file, "RIFF");
    appendLe32(file, static_cast<uint32_t>(4 + body.size()));
    appendId(file, "WAVE");
    appendBytes(file, body.data(), body.size());
    return file;
}

std::vector<uint8_t> fmtChunk(
    uint16_t format,
    uint16_t channels,
    uint32_t sampleRate,
    uint16_t bits,
    bool extraByte)
{
    std::vector<uint8_t> payload;
    appendLe16(payload, format);
    appendLe16(payload, channels);
    appendLe32(payload, sampleRate);
    const uint16_t align = static_cast<uint16_t>(channels * (bits / 8));
    appendLe32(payload, sampleRate * align);
    appendLe16(payload, align);
    appendLe16(payload, bits);
    if (extraByte) {
        appendLe16(payload, 0);
    }
    return payload;
}

std::vector<uint8_t> pcmSamples(const std::vector<int16_t> &samples) {
    std::vector<uint8_t> payload;
    for (int16_t sample : samples) {
        appendLe16(payload, static_cast<uint16_t>(sample));
    }
    return payload;
}

Chunk chunk(const char *id, std::vector<uint8_t> payload) {
    Chunk result;
    result.id[0] = id[0];
    result.id[1] = id[1];
    result.id[2] = id[2];
    result.id[3] = id[3];
    result.id[4] = '\0';
    result.payload = std::move(payload);
    return result;
}

} /* namespace */

TEST(PcmWaveTests, LoadsMonoPcmAndSkipsTrailingChunks) {
    const std::vector<int16_t> expected = { 384, -32768, 32767, 0 };
    std::vector<uint8_t> listPayload = { 'I', 'N', 'F', 'O', 1 };
    const std::vector<uint8_t> file = makeRiff({
        chunk("fmt ", fmtChunk(1, 1, 44100, 16, false)),
        chunk("data", pcmSamples(expected)),
        chunk("LIST", std::move(listPayload))
    });

    PcmWave wave;
    ASSERT_TRUE(wave.load(file.data(), file.size())) << wave.error();
    ASSERT_EQ(wave.sampleCount(), static_cast<unsigned int>(expected.size()));
    EXPECT_EQ(wave.sampleRate(), 44100);
    EXPECT_EQ(wave.channelCount(), 1);
    for (unsigned int i = 0; i < wave.sampleCount(); ++i) {
        EXPECT_EQ(wave.samples()[i], expected[i]);
    }
}

TEST(PcmWaveTests, ReadsDataBeforeFormatAndOddSizedChunks) {
    const std::vector<int16_t> expected = { -16942, 93 };
    const std::vector<uint8_t> file = makeRiff({
        chunk("JUNK", std::vector<uint8_t>{ 0x7e }),
        chunk("data", pcmSamples(expected)),
        chunk("fmt ", fmtChunk(1, 1, 44100, 16, true))
    });

    PcmWave wave;
    ASSERT_TRUE(wave.load(file.data(), file.size())) << wave.error();
    ASSERT_EQ(wave.sampleCount(), 2u);
    EXPECT_EQ(wave.samples()[0], -16942);
    EXPECT_EQ(wave.samples()[1], 93);
}

TEST(PcmWaveTests, RejectsOtherLayouts) {
    const std::vector<int16_t> one = { 1 };
    const std::vector<std::vector<uint8_t>> files = {
        makeRiff({
            chunk("fmt ", fmtChunk(1, 2, 44100, 16, false)),
            chunk("data", pcmSamples({ 1, 2 }))
        }),
        makeRiff({
            chunk("fmt ", fmtChunk(1, 1, 48000, 16, false)),
            chunk("data", pcmSamples(one))
        }),
        makeRiff({
            chunk("fmt ", fmtChunk(3, 1, 44100, 16, false)),
            chunk("data", pcmSamples(one))
        }),
        makeRiff({
            chunk("fmt ", fmtChunk(1, 1, 44100, 16, false))
        }),
        { 'R', 'I', 'F', 'F', 4, 0, 0, 0, 'J', 'U', 'N', 'K' }
    };

    for (const std::vector<uint8_t> &file : files) {
        PcmWave wave;
        EXPECT_FALSE(wave.load(file.data(), file.size()));
        EXPECT_TRUE(wave.samples() == nullptr);
        EXPECT_FALSE(wave.error().empty());
    }
}

TEST(PcmWaveTests, RejectsAPartialSample) {
    std::vector<uint8_t> data = pcmSamples({ 1, 2 });
    data.pop_back();
    const std::vector<uint8_t> file = makeRiff({
        chunk("fmt ", fmtChunk(1, 1, 44100, 16, false)),
        chunk("data", std::move(data))
    });

    PcmWave wave;
    EXPECT_FALSE(wave.load(file.data(), file.size()));
    EXPECT_EQ(wave.error(), "data chunk is not a whole number of samples");
}

TEST(PcmWaveTests, RejectsAPartialDataChunk) {
    std::vector<uint8_t> file = makeRiff({
        chunk("fmt ", fmtChunk(1, 1, 44100, 16, false)),
        chunk("data", pcmSamples({ 1, 2, 3, 4 }))
    });
    file.resize(file.size() - 4);
    const uint32_t riffSize = static_cast<uint32_t>(file.size() - 8);
    file[4] = static_cast<uint8_t>(riffSize & 0xffu);
    file[5] = static_cast<uint8_t>((riffSize >> 8) & 0xffu);
    file[6] = static_cast<uint8_t>((riffSize >> 16) & 0xffu);
    file[7] = static_cast<uint8_t>((riffSize >> 24) & 0xffu);

    PcmWave wave;
    EXPECT_FALSE(wave.load(file.data(), file.size()));
    EXPECT_EQ(wave.error(), "truncated chunk");
}

TEST(PcmWaveTests, LoadsSoundLibraryImpulseResponses) {
    const std::string root =
        std::string(ENGINE_SIM_SOURCE_DIR) + "/es/sound-library/";

    struct Expected {
        const char *path;
        unsigned int sampleCount;
        int16_t first;
        int16_t second;
        int16_t third;
        int16_t middle;
        int16_t last;
    };

    const Expected files[] = {
        { "archive/test_engine_14_eq_adjusted_16.wav", 9363, 915, 1385, 1736, 107, 0 },
        { "archive/test_engine_15_eq_adjusted_16.wav", 12924, 1698, 1890, 1336, -11325, 0 },
        { "archive/test_engine_16_eq_adjusted_16.wav", 22621, 5927, 7239, 7250, 252, 0 },
        { "new/mild_exhaust.wav", 9647, -16942, -32262, -32393, 0, 0 },
        { "new/mild_exhaust_reverb.wav", 42415, -12706, -24197, -24295, -3, 0 },
        { "new/minimal_muffling_01.wav", 17555, -274, -532, -737, 0, 0 },
        { "new/minimal_muffling_02.wav", 17555, -32, -87, -166, 0, 0 },
        { "new/minimal_muffling_03.wav", 11736, 93, 93, 87, -1, -1 },
        { "sharp/sharp_01.wav", 11736, 93, 93, 87, -1, -1 },
        { "smooth/smooth_39.wav", 33705, 384, 627, 724, 0, 0 }
    };

    for (const Expected &expected : files) {
        PcmWave wave;
        const std::string path = root + expected.path;
        ASSERT_TRUE(wave.load(path)) << path << ": " << wave.error();
        EXPECT_EQ(wave.sampleRate(), 44100) << path;
        EXPECT_EQ(wave.channelCount(), 1) << path;
        ASSERT_EQ(wave.sampleCount(), expected.sampleCount) << path;
        EXPECT_EQ(wave.samples()[0], expected.first) << path;
        EXPECT_EQ(wave.samples()[1], expected.second) << path;
        EXPECT_EQ(wave.samples()[2], expected.third) << path;
        EXPECT_EQ(wave.samples()[expected.sampleCount / 2], expected.middle) << path;
        EXPECT_EQ(wave.samples()[expected.sampleCount - 1], expected.last) << path;
    }

    PcmWave archive;
    ASSERT_TRUE(archive.load(root + "archive/test_engine_14_eq_adjusted_16.wav"));
    EXPECT_EQ(archive.samples()[266], static_cast<int16_t>(-32768));
}
