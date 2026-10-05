#include <gtest/gtest.h>

#include "../include/synthesizer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <vector>

using namespace std::chrono_literals;

void setupStandardSynthesizer(Synthesizer &synth) {
    Synthesizer::Parameters params;
    params.audioBufferSize = 512 * 16;
    params.audioSampleRate = 16;
    params.inputBufferSize = 256;
    params.inputChannelCount = 8;
    params.inputSampleRate = 32;

    Synthesizer::AudioParameters audioParams;
    audioParams.airNoise = 0.0;
    audioParams.inputSampleNoise = 0.0;
    audioParams.levelerMaxGain = 1.0;
    audioParams.levelerMinGain = 1.0;
    audioParams.dF_F_mix = 0.0;
    params.initialAudioParameters = audioParams;

    synth.initialize(params);
}

void setupSynchronizedSynthesizer(Synthesizer &synth) {
    Synthesizer::Parameters params;
    params.audioBufferSize = 512 * 16;
    params.audioSampleRate = 32;
    params.inputBufferSize = 1024;
    params.inputChannelCount = 8;
    params.inputSampleRate = 32;

    Synthesizer::AudioParameters audioParams;
    audioParams.airNoise = 0.0;
    audioParams.inputSampleNoise = 0.0;
    audioParams.levelerMaxGain = 1.0;
    audioParams.levelerMinGain = 1.0;
    audioParams.dF_F_mix = 0.0;
    params.initialAudioParameters = audioParams;

    synth.initialize(params);
}

TEST(SynthesizerTests, SynthesizerSanityCheck) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);
    synth.destroy();
}
/*
TEST(SynthesizerTests, SynthesizerConversionTest) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);

    EXPECT_NEAR(synth.inputSampleToTimeOffset(0.0), 0.0, 1E-6);
    EXPECT_NEAR(synth.inputSampleToTimeOffset(1.0), 1 / 32.0, 1E-6);

    EXPECT_NEAR(synth.audioSampleToTimeOffset(0), -0.5, 1E-6);

    synth.destroy();
}

TEST(SynthesizerTests, SynthesizerTrimTest) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);

    const double timeOffset0 = synth.audioSampleToTimeOffset(0);

    synth.trimInput(0.5, false);

    const double timeOffset1 = synth.audioSampleToTimeOffset(8);

    EXPECT_NEAR(timeOffset1, timeOffset0, 1E-6);

    synth.destroy();
}

TEST(SynthesizerTests, SynthesizerSampleTest) {
    Synthesizer synth;
    setupStandardSynthesizer(synth);

    for (int i = 0; i < 1024; ++i) {
        const double v = (double)i;
        const double data[] = { v, v, v, v, v, v, v, v };
        synth.writeInput(data);
    }

    const double end_t = 1023 / 32.0;

    const double v0 = synth.sampleInput(end_t, 0);
    const double v1 = synth.sampleInput(end_t - 1 / 64.0, 0);

    EXPECT_NEAR(v0, 1023.0, 1E-6);
    EXPECT_NEAR(v1, 1022.5, 1E-6);

    synth.trimInput(0.5);

    const double v0_trim = synth.sampleInput(end_t, 0);
    const double v1_trim = synth.sampleInput(end_t - 1 / 64.0, 0);

    EXPECT_NEAR(v0, v0_trim, 1E-6);
    EXPECT_NEAR(v1, v1_trim, 1E-6);

    synth.destroy();
}
*/

std::vector<int16_t> renderDryRamp(int inputSamples) {
    std::srand(1);

    Synthesizer synth;
    setupSynchronizedSynthesizer(synth);
    Synthesizer::AudioParameters audio = synth.audioParameters();
    audio.convolution = 0.0f;
    synth.audioParameters() = audio;

    std::vector<int16_t> output(static_cast<size_t>(inputSamples) + 16, 0);
    int totalSamples = 0;

    for (int i = 0; i < inputSamples;) {
        for (int j = 0; j < 16 && i < inputSamples; ++j, ++i) {
            const double v = static_cast<double>(i);
            const double data[] = { v, v, v, v, v, v, v, v };
            synth.writeInput(data);
        }

        synth.endInputBlock();
        synth.renderAudio();

        const int room = static_cast<int>(output.size()) - totalSamples;
        if (room <= 0)
            break;
        totalSamples += synth.readAudioOutput(std::min(16, room), output.data() + totalSamples);
    }

    output.resize(static_cast<size_t>(totalSamples));
    synth.destroy();
    return output;
}

TEST(SynthesizerTests, SynthesizerSystemTestSingleThread) {
    const std::vector<int16_t> first = renderDryRamp(64);
    const std::vector<int16_t> second = renderDryRamp(64);

    ASSERT_GT(first.size(), 16u);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first[0], 0);
    EXPECT_NE(first.back(), 0);
}

TEST(SynthesizerTests, SynthesizerSystemTest) {
    constexpr int inputSamples = 64;

    std::srand(1);
    Synthesizer synth;
    setupSynchronizedSynthesizer(synth);
    Synthesizer::AudioParameters audio = synth.audioParameters();
    audio.convolution = 0.0f;
    synth.audioParameters() = audio;
    synth.startAudioRenderingThread();

    for (int i = 0; i < inputSamples; ++i) {
        const double v = static_cast<double>(i);
        const double data[] = { v, v, v, v, v, v, v, v };
        synth.writeInput(data);
    }

    synth.endInputBlock();
    synth.waitProcessed();

    int16_t output[128];
    const int got = synth.readAudioOutput(128, output);
    EXPECT_GT(got, 0);

    synth.endAudioRenderingThread();
    synth.destroy();
}
