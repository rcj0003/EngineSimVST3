#include <gtest/gtest.h>

#include "../include/convolution_filter.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

bool sameBits(float a, float b) {
    uint32_t left = 0;
    uint32_t right = 0;
    std::memcpy(&left, &a, sizeof(left));
    std::memcpy(&right, &b, sizeof(right));
    return left == right;
}

std::vector<float> scalarFir(const std::vector<float> &impulse, const std::vector<float> &input) {
    ConvolutionFilter filter;
    filter.initialize(static_cast<int>(impulse.size()));
    std::memcpy(filter.getImpulseResponse(), impulse.data(), sizeof(float) * impulse.size());

    std::vector<float> output(input.size(), 0.0f);
    for (size_t i = 0; i < input.size(); ++i)
        output[i] = filter.f(input[i]);
    filter.destroy();
    return output;
}

std::vector<float> blockFir(const std::vector<float> &impulse, const std::vector<float> &input) {
    ConvolutionFilter filter;
    filter.initialize(static_cast<int>(impulse.size()));
    std::memcpy(filter.getImpulseResponse(), impulse.data(), sizeof(float) * impulse.size());

    std::vector<float> output(input.size(), 0.0f);
    filter.process(input.data(), output.data(), static_cast<int>(input.size()));
    filter.destroy();
    return output;
}

void expectSameSequence(const std::vector<float> &left, const std::vector<float> &right) {
    ASSERT_EQ(left.size(), right.size());
    for (size_t i = 0; i < left.size(); ++i)
        EXPECT_TRUE(sameBits(left[i], right[i])) << "sample " << i;
}

int16_t quantizeInt16(float sample) {
    long rounded = std::lround(sample);
    if (rounded > INT16_MAX)
        rounded = INT16_MAX;
    if (rounded < INT16_MIN)
        rounded = INT16_MIN;
    return static_cast<int16_t>(rounded);
}

void expectSameInt16(const std::vector<float> &left, const std::vector<float> &right) {
    ASSERT_EQ(left.size(), right.size());
    float peak = 0.0f;
    for (float sample : left)
        peak = std::max(peak, std::abs(sample));
    const float gain = peak > 0.0f ? 20000.0f / peak : 1.0f;
    for (size_t i = 0; i < left.size(); ++i) {
        EXPECT_EQ(quantizeInt16(left[i] * gain), quantizeInt16(right[i] * gain))
            << "sample " << i;
    }
}

std::vector<std::complex<double>> dft(const std::vector<std::complex<double>> &input, bool inverse) {
    const int n = static_cast<int>(input.size());
    std::vector<std::complex<double>> output(static_cast<size_t>(n));
    const double sign = inverse ? 1.0 : -1.0;
    for (int k = 0; k < n; ++k) {
        std::complex<double> sum = 0.0;
        for (int t = 0; t < n; ++t) {
            const double angle = sign * 2.0 * 3.14159265358979323846 * k * t / n;
            sum += input[static_cast<size_t>(t)] * std::complex<double>(std::cos(angle), std::sin(angle));
        }
        output[static_cast<size_t>(k)] = inverse ? sum / static_cast<double>(n) : sum;
    }
    return output;
}

std::vector<float> overlapAddDft(const std::vector<float> &impulse, const std::vector<float> &input) {
    const int taps = static_cast<int>(impulse.size());
    const int block = 8;
    const int fftSize = 16;
    std::vector<std::complex<double>> spectrum(static_cast<size_t>(fftSize));
    for (int i = 0; i < taps && i < fftSize; ++i)
        spectrum[static_cast<size_t>(i)] = impulse[static_cast<size_t>(i)];
    spectrum = dft(spectrum, false);

    std::vector<float> output(input.size(), 0.0f);
    for (int start = 0; start < static_cast<int>(input.size()); start += block) {
        std::vector<std::complex<double>> frame(static_cast<size_t>(fftSize));
        for (int i = 0; i < block && start + i < static_cast<int>(input.size()); ++i)
            frame[static_cast<size_t>(i)] = input[static_cast<size_t>(start + i)];
        frame = dft(frame, false);
        for (int i = 0; i < fftSize; ++i)
            frame[static_cast<size_t>(i)] *= spectrum[static_cast<size_t>(i)];
        frame = dft(frame, true);
        for (int i = 0; i < fftSize && start + i < static_cast<int>(output.size()); ++i)
            output[static_cast<size_t>(start + i)] += static_cast<float>(frame[static_cast<size_t>(i)].real());
    }
    return output;
}

} // namespace

TEST(ConvolutionFilterTests, BlockMatchesScalarBits) {
    const int lengths[] = {1, 2, 3, 4, 5, 7, 8, 17, 64, 128, 512, 513};
    for (int taps : lengths) {
        std::vector<float> impulse(static_cast<size_t>(taps));
        for (int i = 0; i < taps; ++i)
            impulse[static_cast<size_t>(i)] = std::sin(0.31f * static_cast<float>(i)) * 0.02f;

        std::vector<float> input(static_cast<size_t>(taps * 3 + 5));
        for (size_t i = 0; i < input.size(); ++i)
            input[i] = std::sin(0.17f * static_cast<float>(i));

        const std::vector<float> scalar = scalarFir(impulse, input);
        const std::vector<float> block = blockFir(impulse, input);
        expectSameSequence(scalar, block);
        expectSameInt16(scalar, block);
    }
}

TEST(ConvolutionFilterTests, PiecewiseProcessMatchesScalarBits) {
    const int taps = 300;
    std::vector<float> impulse(static_cast<size_t>(taps));
    std::vector<float> input(1600);
    for (int i = 0; i < taps; ++i)
        impulse[static_cast<size_t>(i)] = std::cos(0.07f * static_cast<float>(i)) * 0.03f;
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = std::sin(0.013f * static_cast<float>(i) + 0.4f);

    ConvolutionFilter pieces;
    pieces.initialize(taps);
    std::memcpy(pieces.getImpulseResponse(), impulse.data(), sizeof(float) * impulse.size());

    std::vector<float> output(input.size(), 0.0f);
    const int slices[] = {1, 3, 4, 5, 7, 8, 64, 100, 511, 512, 513};
    int done = 0;
    for (int slice : slices) {
        if (done >= static_cast<int>(input.size()))
            break;
        const int width = std::min(slice, static_cast<int>(input.size()) - done);
        pieces.process(input.data() + done, output.data() + done, width);
        done += width;
    }
    if (done < static_cast<int>(input.size())) {
        pieces.process(input.data() + done, output.data() + done,
            static_cast<int>(input.size()) - done);
    }
    pieces.destroy();

    const std::vector<float> scalar = scalarFir(impulse, input);
    expectSameSequence(scalar, output);
    expectSameInt16(scalar, output);
}

TEST(ConvolutionFilterTests, AdvanceHistoryMatchesSkippedFir) {
    const std::vector<float> impulse = {0.4f, -0.2f, 0.1f, 0.05f, -0.01f};
    const std::vector<float> skipped = {0.2f, -0.3f, 0.7f, 0.1f, -0.4f, 0.25f};
    const std::vector<float> kept = {1.0f, -1.0f, 0.5f, 0.25f, -0.75f};

    ConvolutionFilter advanced;
    ConvolutionFilter scalar;
    advanced.initialize(static_cast<int>(impulse.size()));
    scalar.initialize(static_cast<int>(impulse.size()));
    std::memcpy(advanced.getImpulseResponse(), impulse.data(), sizeof(float) * impulse.size());
    std::memcpy(scalar.getImpulseResponse(), impulse.data(), sizeof(float) * impulse.size());

    advanced.advanceHistory(skipped.data(), static_cast<int>(skipped.size()));
    for (float sample : skipped)
        scalar.f(sample);

    std::vector<float> blockOutput(kept.size(), 0.0f);
    advanced.process(kept.data(), blockOutput.data(), static_cast<int>(kept.size()));
    for (size_t i = 0; i < kept.size(); ++i)
        EXPECT_TRUE(sameBits(blockOutput[i], scalar.f(kept[i]))) << i;

    advanced.destroy();
    scalar.destroy();
}

TEST(ConvolutionFilterTests, DirectFormStaysAheadOfDftRounding) {
    std::vector<float> impulse(8);
    std::vector<float> input(32);
    for (int i = 0; i < 8; ++i)
        impulse[static_cast<size_t>(i)] = 0.1f + 0.01f * static_cast<float>(i);
    for (int i = 0; i < 32; ++i)
        input[static_cast<size_t>(i)] = 0.1f * static_cast<float>((i % 5) - 2);

    const std::vector<float> direct = scalarFir(impulse, input);
    const std::vector<float> transformed = overlapAddDft(impulse, input);
    expectSameSequence(direct, blockFir(impulse, input));
    expectSameInt16(direct, blockFir(impulse, input));

    int different = 0;
    for (size_t i = 0; i < direct.size(); ++i) {
        if (!sameBits(direct[i], transformed[i]))
            ++different;
    }
    EXPECT_GT(different, 0);
}
