#include "engine_session.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kSampleRate = 44100;
constexpr double kDurationSeconds = 2.0;
constexpr int kBlockSamples = 512;
constexpr float kThrottle = 0.4f;
constexpr int kHoldRpm = 3000;

void write16(std::ostream &out, uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff)
    };
    out.write(bytes, 2);
}

void write32(std::ostream &out, uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff)
    };
    out.write(bytes, 4);
}

bool writeWav(const std::filesystem::path &path, const std::vector<float> &samples) {
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;

    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    out.write("RIFF", 4);
    write32(out, 36 + dataBytes);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write32(out, 16);
    write16(out, 1);
    write16(out, 1);
    write32(out, kSampleRate);
    write32(out, kSampleRate * sizeof(int16_t));
    write16(out, sizeof(int16_t));
    write16(out, 16);
    out.write("data", 4);
    write32(out, dataBytes);

    for (float sample : samples) {
        const float clamped = std::clamp(sample, -1.0f, 1.0f);
        const auto pcm = static_cast<int16_t>(std::lround(clamped * 32767.0f));
        write16(out, static_cast<uint16_t>(pcm));
    }

    return static_cast<bool>(out);
}

float peakOf(const std::vector<float> &samples) {
    float peak = 0.0f;
    for (float sample : samples)
        peak = std::max(peak, std::abs(sample));
    return peak;
}

EngineSimSession::BlockControls baseControls(const EngineSimSession &session) {
    EngineSimSession::BlockControls controls;
    controls.throttle = kThrottle;
    controls.ignition = true;
    controls.starter = false;
    controls.gear = -1;
    controls.volume = session.volume();
    controls.convolution = session.convolution();
    controls.highFrequencyGain = session.highFrequencyGain();
    controls.noise = session.noise();
    controls.simulationFrequency = session.simulationFrequency();
    return controls;
}

bool renderTake(const std::string &name, bool hold, const std::filesystem::path &path) {
    EngineSimSession session;
    if (!session.loadNear(ENGINE_SIM_ROOT)) {
        std::cerr << name << ": " << session.error() << '\n';
        return false;
    }

    EngineSimSession::BlockControls controls = baseControls(session);
    controls.hold = hold;
    controls.rpm = kHoldRpm;

    session.prepare(kSampleRate);
    const int total = static_cast<int>(std::llround(kDurationSeconds * kSampleRate));
    std::vector<float> samples(static_cast<size_t>(total), 0.0f);

    int rendered = 0;
    while (rendered < total) {
        const int count = std::min(kBlockSamples, total - rendered);
        session.process(controls, count, samples.data() + rendered);
        rendered += count;
    }

    if (!writeWav(path, samples)) {
        std::cerr << name << ": could not write " << path << '\n';
        return false;
    }

    const float peak = peakOf(samples);
    std::cout << name
              << ": " << path
              << "  samples=" << samples.size()
              << "  peak=" << peak
              << "  rpm=" << session.measuredRpm()
              << '\n';
    return peak > 0.0f;
}

} // namespace

int main(int argc, char **argv) {
    const std::filesystem::path directory = argc > 1 ? argv[1] : std::filesystem::current_path();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "could not create " << directory << ": " << error.message() << '\n';
        return 1;
    }

    const bool freeOk = renderTake("free-running", false, directory / "free-running.wav");
    const bool holdOk = renderTake("hold", true, directory / "hold.wav");
    return freeOk && holdOk ? 0 : 1;
}
