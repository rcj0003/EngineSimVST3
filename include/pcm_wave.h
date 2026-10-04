#ifndef ATG_ENGINE_SIM_PCM_WAVE_H
#define ATG_ENGINE_SIM_PCM_WAVE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// 16-bit mono 44100 Hz PCM, the exhaust impulse responses in es/sound-library.
// This is the loader the headless build uses instead of ysWindowsAudioWaveFile.
class PcmWave {
    public:
        static constexpr int ExpectedSampleRate = 44100;
        static constexpr int ExpectedChannelCount = 1;
        static constexpr int ExpectedBitsPerSample = 16;

        bool load(const std::string &filename);
        bool load(const uint8_t *bytes, size_t size);

        const int16_t *samples() const;
        unsigned int sampleCount() const;
        int sampleRate() const { return m_sampleRate; }
        int channelCount() const { return m_channelCount; }

        const std::string &error() const { return m_error; }

    private:
        bool fail(const std::string &message);

        std::vector<int16_t> m_samples;
        int m_sampleRate = 0;
        int m_channelCount = 0;
        std::string m_error;
};

#endif /* ATG_ENGINE_SIM_PCM_WAVE_H */
