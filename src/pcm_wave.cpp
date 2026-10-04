#include "../include/pcm_wave.h"

#include <fstream>

namespace {

constexpr size_t MaxWaveBytes = 16 * 1024 * 1024;

uint16_t readLe16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t readLe32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
}

bool idEquals(const uint8_t *p, char a, char b, char c, char d) {
    return p[0] == static_cast<uint8_t>(a)
        && p[1] == static_cast<uint8_t>(b)
        && p[2] == static_cast<uint8_t>(c)
        && p[3] == static_cast<uint8_t>(d);
}

} /* namespace */

bool PcmWave::fail(const std::string &message) {
    m_samples.clear();
    m_sampleRate = 0;
    m_channelCount = 0;
    m_error = message;
    return false;
}

bool PcmWave::load(const std::string &filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        return fail("could not open file");
    }

    in.seekg(0, std::ios::end);
    const std::streamoff length = in.tellg();
    if (length < 0) {
        return fail("could not read file");
    }
    if (static_cast<unsigned long long>(length) > MaxWaveBytes) {
        return fail("file is too large");
    }

    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    if (length > 0) {
        in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(length));
        if (in.gcount() != length) {
            return fail("could not read file");
        }
    }

    return load(bytes.data(), bytes.size());
}

bool PcmWave::load(const uint8_t *bytes, size_t size) {
    m_samples.clear();
    m_sampleRate = 0;
    m_channelCount = 0;
    m_error.clear();

    if (bytes == nullptr || size < 12 || size > MaxWaveBytes) {
        return fail("not a RIFF WAVE file");
    }

    if (!idEquals(bytes, 'R', 'I', 'F', 'F') || !idEquals(bytes + 8, 'W', 'A', 'V', 'E')) {
        return fail("not a RIFF WAVE file");
    }

    const uint32_t riffSize = readLe32(bytes + 4);
    size_t end = size;
    if (riffSize < 4) {
        return fail("not a RIFF WAVE file");
    }
    const uint64_t riffEnd = 8ull + static_cast<uint64_t>(riffSize);
    if (riffEnd < end) {
        end = static_cast<size_t>(riffEnd);
    }

    bool foundFormat = false;
    bool foundData = false;
    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    size_t dataOffset = 0;
    uint32_t dataSize = 0;

    size_t offset = 12;
    while (offset + 8 <= end) {
        const uint8_t *header = bytes + offset;
        const uint32_t chunkSize = readLe32(header + 4);
        const size_t payload = offset + 8;
        if (static_cast<uint64_t>(payload) + chunkSize > end) {
            return fail("truncated chunk");
        }

        if (idEquals(header, 'f', 'm', 't', ' ')) {
            if (chunkSize < 16) {
                return fail("fmt chunk is too small");
            }
            if (!foundFormat) {
                audioFormat = readLe16(bytes + payload);
                channels = readLe16(bytes + payload + 2);
                sampleRate = readLe32(bytes + payload + 4);
                blockAlign = readLe16(bytes + payload + 12);
                bitsPerSample = readLe16(bytes + payload + 14);
                foundFormat = true;
            }
        }
        else if (idEquals(header, 'd', 'a', 't', 'a')) {
            if (!foundData) {
                dataOffset = payload;
                dataSize = chunkSize;
                foundData = true;
            }
        }

        offset = payload + chunkSize;
        if ((chunkSize & 1u) != 0u) {
            ++offset;
        }
    }

    if (!foundFormat) {
        return fail("missing fmt chunk");
    }
    if (!foundData) {
        return fail("missing data chunk");
    }

    const unsigned int expectedAlign =
        (ExpectedChannelCount * ExpectedBitsPerSample) / 8;
    if (audioFormat != 1
        || channels != ExpectedChannelCount
        || sampleRate != ExpectedSampleRate
        || bitsPerSample != ExpectedBitsPerSample
        || blockAlign != expectedAlign)
    {
        return fail(
            "expected 16-bit mono 44100 Hz PCM, got "
            + std::to_string(bitsPerSample) + "-bit, "
            + std::to_string(channels) + " ch, "
            + std::to_string(sampleRate) + " Hz, format "
            + std::to_string(audioFormat));
    }

    if ((dataSize % blockAlign) != 0u || dataSize == 0u) {
        return fail("data chunk is not a whole number of samples");
    }

    const unsigned int count = dataSize / blockAlign;
    m_samples.resize(count);
    for (unsigned int i = 0; i < count; ++i) {
        const size_t sampleOffset = dataOffset + static_cast<size_t>(i) * 2u;
        m_samples[i] = static_cast<int16_t>(readLe16(bytes + sampleOffset));
    }

    m_sampleRate = static_cast<int>(sampleRate);
    m_channelCount = static_cast<int>(channels);
    return true;
}

const int16_t *PcmWave::samples() const {
    if (m_samples.empty()) {
        return nullptr;
    }
    return m_samples.data();
}

unsigned int PcmWave::sampleCount() const {
    return static_cast<unsigned int>(m_samples.size());
}
