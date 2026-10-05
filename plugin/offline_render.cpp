#include "engine_session.h"

#include <algorithm>
#include <chrono>
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

bool resetReturnsToRest() {
    EngineSimSession running;
    if (!running.loadNear(ENGINE_SIM_ROOT)) {
        std::cerr << "reset: " << running.error() << '\n';
        return false;
    }

    EngineSimSession fresh;
    if (!fresh.loadNear(ENGINE_SIM_ROOT)) {
        std::cerr << "reset: " << fresh.error() << '\n';
        return false;
    }

    running.prepare(kSampleRate);
    fresh.prepare(kSampleRate);

    const EngineSimSession::BlockControls controls = baseControls(running);
    const EngineSimSession::BlockControls freshControls = baseControls(fresh);
    std::vector<float> block(static_cast<size_t>(kBlockSamples), 0.0f);

    const int warmupBlocks = std::max(1, static_cast<int>(std::llround(1.0 * kSampleRate / kBlockSamples)));
    for (int i = 0; i < warmupBlocks; ++i)
        running.process(controls, kBlockSamples, block.data());

    const float runningRpm = running.measuredRpm();

    const int settleBlocks = std::max(1, static_cast<int>(std::llround(0.05 * kSampleRate / kBlockSamples)));
    for (int i = 0; i < settleBlocks; ++i)
        fresh.process(freshControls, kBlockSamples, block.data());
    const float freshRpm = fresh.measuredRpm();

    // A reset between notes is applied with no simulation step in between.
    running.applyResetEdge(true);
    running.applyResetEdge(false);
    if (running.measuredRpm() > 1.0f) {
        std::cerr << "reset: rpm stayed at " << running.measuredRpm() << " after the edge\n";
        return false;
    }

    for (int i = 0; i < settleBlocks; ++i)
        running.process(controls, kBlockSamples, block.data());
    const float restartedRpm = running.measuredRpm();

    const float tolerance = std::max(80.0f, freshRpm * 0.35f);
    const bool matchesFresh = std::abs(restartedRpm - freshRpm) <= tolerance;
    const bool leftRunning = runningRpm > 800.0f && restartedRpm < runningRpm * 0.5f;

    std::cout << "reset: running=" << runningRpm
              << " fresh=" << freshRpm
              << " restarted=" << restartedRpm
              << '\n';

    if (!matchesFresh || !leftRunning) {
        std::cerr << "reset: restarted engine did not match a fresh start\n";
        return false;
    }

    return true;
}

bool profileScript(const std::filesystem::path &script) {
    EngineSimSession session;
    if (!session.loadNear(ENGINE_SIM_ROOT)) {
        std::cerr << script << ": " << session.error() << '\n';
        return false;
    }

    EngineSimSession::CompiledEngine compiled;
    std::string error;
    if (!session.compileScript(script.string(), compiled, error) || !session.install(compiled)) {
        std::cerr << script << ": " << (error.empty() ? session.error() : error) << '\n';
        return false;
    }

    session.prepare(kSampleRate);
    session.setStageTimingEnabled(true);
    session.resetStageTimings();

    EngineSimSession::BlockControls controls = baseControls(session);
    controls.ignition = true;
    constexpr double profileSeconds = 1.0;
    const int total = static_cast<int>(std::llround(profileSeconds * kSampleRate));
    std::vector<float> samples(static_cast<size_t>(kBlockSamples), 0.0f);

    const auto wallStart = std::chrono::steady_clock::now();
    int rendered = 0;
    while (rendered < total) {
        const int count = std::min(kBlockSamples, total - rendered);
        session.process(controls, count, samples.data());
        rendered += count;
    }
    const double wallSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - wallStart).count();

    const EngineSimSession::StageTimings timings = session.stageTimings();
    const double audioSeconds = static_cast<double>(total) / kSampleRate;
    std::cout << "profile " << script.filename()
              << " cylinders=" << session.cylinderCount()
              << " simHz=" << session.simulationFrequency()
              << " sampleRate=" << kSampleRate
              << " audioSeconds=" << audioSeconds
              << " peak=" << peakOf(samples)
              << '\n';
    std::cout << "  solverSeconds=" << timings.solverSeconds
              << " fluidSeconds=" << timings.fluidSeconds
              << " convolutionSeconds=" << timings.convolutionSeconds
              << " physicsSteps=" << timings.physicsSteps
              << " wallSeconds=" << wallSeconds
              << '\n';
    std::cout << "  perAudioSecond solver=" << (timings.solverSeconds / audioSeconds)
              << " fluid=" << (timings.fluidSeconds / audioSeconds)
              << " convolution=" << (timings.convolutionSeconds / audioSeconds)
              << " wall=" << (wallSeconds / audioSeconds)
              << '\n';
    return true;
}

} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "--profile") {
        if (argc < 4) {
            std::cerr << "usage: engine-sim-offline-render --profile four-cylinder.mr eight-cylinder.mr\n";
            return 1;
        }
        const bool first = profileScript(argv[2]);
        const bool second = profileScript(argv[3]);
        return first && second ? 0 : 1;
    }

    const std::filesystem::path directory = argc > 1 ? argv[1] : std::filesystem::current_path();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "could not create " << directory << ": " << error.message() << '\n';
        return 1;
    }

    const bool freeOk = renderTake("free-running", false, directory / "free-running.wav");
    const bool holdOk = renderTake("hold", true, directory / "hold.wav");
    const bool resetOk = resetReturnsToRest();
    return freeOk && holdOk && resetOk ? 0 : 1;
}
