#include "engine_session.h"

#include "compiler.h"
#include "exhaust_system.h"
#include "impulse_response.h"
#include "pcm_wave.h"
#include "simulator.h"
#include "synthesizer.h"
#include "units.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr double kStarterReleaseRpm = 400.0;
constexpr int kMaxChunkSamples = 4096;

std::string readText(const std::filesystem::path &path) {
    std::ifstream in(path);
    if (!in)
        return {};

    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    constexpr std::size_t kLimit = 4000;
    if (text.size() > kLimit)
        text.resize(kLimit);
    return text;
}

bool isEngineRoot(const std::filesystem::path &directory) {
    std::error_code error;
    return std::filesystem::exists(directory / "assets" / "main.mr", error)
        && !error
        && std::filesystem::exists(directory / "es" / "engine_sim.mr", error)
        && !error;
}

std::string thisModulePath() {
#if defined(_WIN32)
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&thisModulePath),
            &module))
        return {};

    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
        return {};
    return std::filesystem::path(buffer).string();
#else
    Dl_info info;
    if (dladdr(reinterpret_cast<void *>(&thisModulePath), &info) == 0 || info.dli_fname == nullptr)
        return {};
    return info.dli_fname;
#endif
}

std::filesystem::path findEngineRoot(std::filesystem::path start) {
    std::error_code error;
    if (start.empty())
        return {};

    if (std::filesystem::is_regular_file(start, error))
        start = start.parent_path();

    for (int i = 0; i < 10 && !start.empty(); ++i) {
        if (isEngineRoot(start))
            return start;
        // macOS .app: assets live under Contents/Resources so codesign stays sealed.
        if (isEngineRoot(start / "Resources"))
            return start / "Resources";
        if (isEngineRoot(start / "engine-sim"))
            return start / "engine-sim";

        const std::filesystem::path parent = start.parent_path();
        if (parent == start)
            break;
        start = parent;
    }

    return {};
}

std::filesystem::path canonicalPath(const std::filesystem::path &path) {
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (error || canonical.empty())
        return path;
    return canonical;
}

// Returns false when the file cannot be read. A one-sample tap still has to be
// installed: initializeImpulseResponse drops samples quieter than 100, and a
// zero-length convolution divides by its length while rendering.
bool loadImpulse(Simulator &simulator, int index, const std::string &filename, float volume) {
    PcmWave wave;
    if (!filename.empty() && wave.load(filename) && wave.sampleCount() > 0) {
        simulator.synthesizer().initializeImpulseResponse(
            wave.samples(),
            wave.sampleCount(),
            volume,
            index);
        return true;
    }

    const int16_t tap = 101;
    simulator.synthesizer().initializeImpulseResponse(&tap, 1, 0.0f, index);
    return false;
}

void noteMissingImpulse(std::string &warnings, const std::string &filename) {
    if (!warnings.empty())
        warnings += "; ";
    warnings += "could not open ";
    warnings += filename.empty() ? "impulse response" : filename;
}

void discardCompiled(EngineSimSession::CompiledEngine &compiled) {
    if (compiled.engine != nullptr) {
        compiled.engine->destroy();
        delete compiled.engine;
        compiled.engine = nullptr;
    }

    delete compiled.vehicle;
    compiled.vehicle = nullptr;
    delete compiled.transmission;
    compiled.transmission = nullptr;
}

bool instantiatesMain(const std::filesystem::path &path) {
    std::ifstream in(path);
    if (!in)
        return false;

    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text.find("main()") != std::string::npos;
}

// Engine files define public node main and are normally imported by main.mr, which
// is what actually calls main(). A picked file with no main() call needs that call.
bool writeEntryScript(
    const std::filesystem::path &script,
    std::filesystem::path &wrapper,
    std::string &error)
{
    const std::string generic = script.generic_string();
    if (generic.find('"') != std::string::npos) {
        error = "engine path cannot contain a quote";
        return false;
    }

    std::error_code failure;
    const auto directory = std::filesystem::temp_directory_path(failure);
    if (failure) {
        error = "could not find a temporary directory";
        return false;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    wrapper = directory / ("engine-sim-entry-" + std::to_string(stamp) + ".mr");

    std::ofstream out(wrapper);
    if (!out) {
        error = "could not write " + wrapper.string();
        return false;
    }

    out << "import \"" << generic << "\"\nmain()\n";
    if (!out) {
        error = "could not write " + wrapper.string();
        std::filesystem::remove(wrapper, failure);
        return false;
    }

    return true;
}

enum class CompileUnitResult { Failed, NoEngine, Ready };

CompileUnitResult compileUnit(
    const std::filesystem::path &script,
    const std::filesystem::path &searchDirectory,
    const std::filesystem::path &library,
    const std::string &assetDirectory,
    EngineSimSession::CompiledEngine &compiled,
    std::string &error)
{
    compiled = {};
    error.clear();

    es_script::Compiler compiler;
    compiler.initialize();
    compiler.addSearchPath(library.string());
    if (!searchDirectory.empty())
        compiler.addSearchPath(searchDirectory.string());
    if (!assetDirectory.empty())
        compiler.addSearchPath(assetDirectory);

    bool compiledOk = false;
    try {
        compiledOk = compiler.compile(script.string());
    }
    catch (const std::exception &ex) {
        error = ex.what();
        compiler.destroy();
        return CompileUnitResult::Failed;
    }

    if (!compiledOk) {
        error = "failed to compile " + script.string();
        const std::string log = readText("error_log.log");
        if (!log.empty())
            error += "\n" + log;
        compiler.destroy();
        return CompileUnitResult::Failed;
    }

    // The compiler writes into one static slot. Clear it first so a script that
    // never calls set_engine cannot hand back the engine this session still owns.
    es_script::Compiler::Output *slot = es_script::Compiler::output();
    slot->engine = nullptr;
    slot->vehicle = nullptr;
    slot->transmission = nullptr;

    es_script::Compiler::Output output;
    try {
        output = compiler.execute();
    }
    catch (const std::exception &ex) {
        compiled.engine = slot->engine;
        compiled.vehicle = slot->vehicle;
        compiled.transmission = slot->transmission;
        slot->engine = nullptr;
        slot->vehicle = nullptr;
        slot->transmission = nullptr;
        discardCompiled(compiled);
        compiler.destroy();
        error = ex.what();
        return CompileUnitResult::Failed;
    }

    slot->engine = nullptr;
    slot->vehicle = nullptr;
    slot->transmission = nullptr;
    compiler.destroy();

    compiled.engine = output.engine;
    compiled.vehicle = output.vehicle;
    compiled.transmission = output.transmission;
    compiled.scriptPath = script.string();
    if (compiled.engine == nullptr || compiled.vehicle == nullptr || compiled.transmission == nullptr) {
        discardCompiled(compiled);
        error = "script did not produce an engine, vehicle, and transmission";
        return CompileUnitResult::NoEngine;
    }

    return CompileUnitResult::Ready;
}

} // namespace

EngineSimSession::EngineSimSession() = default;

EngineSimSession::~EngineSimSession() {
    release();
}

void EngineSimSession::release() {
    if (m_simulator != nullptr) {
        m_simulator->destroy();
        delete m_simulator;
        m_simulator = nullptr;
    }

    if (m_engine != nullptr) {
        m_engine->destroy();
        delete m_engine;
        m_engine = nullptr;
    }

    delete m_vehicle;
    m_vehicle = nullptr;
    delete m_transmission;
    m_transmission = nullptr;

    m_sampleRate = 0.0;
    m_smoothedThrottle = 0.0;
    m_appliedGear = -1;
    m_crankUntilRunning = false;
    m_measuredRpm.store(0.0f, std::memory_order_relaxed);
}

bool EngineSimSession::loadNear(const std::string &hint) {
    // The host executable is not the plugin. Start from this module so assets
    // stored beside the VST3 bundle are found without a compile-time path.
    std::filesystem::path root = findEngineRoot(thisModulePath());
    if (root.empty())
        root = findEngineRoot(hint);
    if (root.empty())
        root = findEngineRoot(std::filesystem::current_path());
    if (root.empty())
        root = findEngineRoot(ENGINE_SIM_ROOT);

    if (root.empty()) {
        m_error = "could not find assets/main.mr and es/engine_sim.mr";
        return false;
    }

    return load((root / "assets").string(), (root / "es").string());
}

bool EngineSimSession::load(const std::string &assetDirectory, const std::string &libraryDirectory) {
    m_assetDirectory = assetDirectory;
    m_libraryDirectory = libraryDirectory;

    const std::filesystem::path script = std::filesystem::path(assetDirectory) / "main.mr";
    CompiledEngine compiled;
    std::string error;
    if (!compileScript(script.string(), compiled, error)) {
        m_error = error;
        return false;
    }

    return install(compiled);
}

void EngineSimSession::discard(CompiledEngine &compiled) {
    discardCompiled(compiled);
}

bool EngineSimSession::compileScript(
    const std::string &scriptPath,
    CompiledEngine &compiled,
    std::string &error) const
{
    compiled = {};
    error.clear();

    const std::filesystem::path script = canonicalPath(scriptPath);
    const std::filesystem::path library = m_libraryDirectory;
    if (!std::filesystem::exists(script)) {
        error = "could not find " + script.string();
        return false;
    }
    if (!std::filesystem::exists(library / "engine_sim.mr")) {
        error = "could not find " + (library / "engine_sim.mr").string();
        return false;
    }

    const std::filesystem::path searchDirectory = script.parent_path();
    if (instantiatesMain(script))
        return compileUnit(script, searchDirectory, library, m_assetDirectory, compiled, error) == CompileUnitResult::Ready;

    std::filesystem::path wrapper;
    if (!writeEntryScript(script, wrapper, error))
        return false;

    const CompileUnitResult result = compileUnit(
        wrapper,
        searchDirectory,
        library,
        m_assetDirectory,
        compiled,
        error);
    std::error_code failure;
    std::filesystem::remove(wrapper, failure);

    if (result != CompileUnitResult::Ready) {
        const bool namesScript = error.find(script.filename().string()) != std::string::npos;
        const std::string summary = "failed to compile " + wrapper.string();
        if (error.compare(0, summary.size(), summary) == 0)
            error.replace(0, summary.size(), "failed to compile " + script.string());

        if (!namesScript && error.find("Undefined node type") != std::string::npos)
            error = script.string() + " did not define main";
        return false;
    }

    compiled.scriptPath = script.string();
    return true;
}

bool EngineSimSession::install(CompiledEngine &compiled) {
    if (compiled.engine == nullptr || compiled.vehicle == nullptr || compiled.transmission == nullptr) {
        discard(compiled);
        m_error = "script did not produce an engine, vehicle, and transmission";
        return false;
    }

    Simulator *simulator = compiled.engine->createSimulator(compiled.vehicle, compiled.transmission);
    if (simulator == nullptr) {
        discard(compiled);
        m_error = "could not create a simulator";
        return false;
    }

    release();

    m_engine = compiled.engine;
    m_vehicle = compiled.vehicle;
    m_transmission = compiled.transmission;
    m_simulator = simulator;
    m_scriptPath = compiled.scriptPath;
    compiled.engine = nullptr;
    compiled.vehicle = nullptr;
    compiled.transmission = nullptr;

    m_error.clear();
    m_warnings.clear();
    m_simulationFrequency = std::max(400, static_cast<int>(m_engine->getSimulationFrequency()));
    m_simulator->setSimulationFrequency(m_simulationFrequency);

    Synthesizer::AudioParameters audio = m_simulator->synthesizer().getAudioParameters();
    audio.inputSampleNoise = static_cast<float>(m_engine->getInitialJitter());
    audio.airNoise = static_cast<float>(m_engine->getInitialNoise());
    audio.dF_F_mix = static_cast<float>(m_engine->getInitialHighFrequencyGain());
    m_simulator->synthesizer().setAudioParameters(audio);

    m_volume = audio.volume;
    m_convolution = audio.convolution;
    m_highFrequencyGain = audio.dF_F_mix;
    m_noise = audio.airNoise;

    for (int i = 0; i < m_engine->getExhaustSystemCount(); ++i) {
        ImpulseResponse *response = m_engine->getExhaustSystem(i)->getImpulseResponse();
        if (response == nullptr) {
            loadImpulse(*m_simulator, i, {}, 0.0f);
            noteMissingImpulse(m_warnings, {});
            continue;
        }

        const std::string filename = response->getFilename();
        if (!loadImpulse(
                *m_simulator,
                i,
                filename,
                static_cast<float>(response->getVolume())))
            noteMissingImpulse(m_warnings, filename);
    }

    m_engine->getIgnitionModule()->m_enabled = true;
    m_transmission->changeGear(-1);
    m_transmission->setClutchPressure(0.0);
    m_appliedGear = m_transmission->getGear();
    m_simulator->m_starterMotor.m_enabled = true;
    m_crankUntilRunning = true;
    m_simulator->m_dyno.m_enabled = false;
    m_simulator->m_dyno.m_hold = false;

    return true;
}

void EngineSimSession::prepare(double sampleRate) {
    if (m_simulator == nullptr || sampleRate <= 0.0)
        return;

    m_sampleRate = sampleRate;
    m_simulator->synthesizer().setAudioSampleRate(static_cast<float>(sampleRate));
}

void EngineSimSession::applyControls(const BlockControls &controls) {
    m_smoothedThrottle = static_cast<double>(controls.throttle) * 0.5 + 0.5 * m_smoothedThrottle;
    m_engine->setSpeedControl(m_smoothedThrottle);
    m_engine->getIgnitionModule()->m_enabled = controls.ignition;
    m_simulator->m_starterMotor.m_enabled = controls.starter || m_crankUntilRunning;
    m_transmission->setClutchPressure(std::clamp(static_cast<double>(controls.clutch), 0.0, 1.0));

    if (controls.gear != m_appliedGear) {
        m_transmission->changeGear(controls.gear);
        m_appliedGear = m_transmission->getGear();
    }

    if (controls.hold) {
        double speed = units::rpm(static_cast<double>(controls.rpm));
        const double minimum = m_engine->getDynoMinSpeed();
        const double maximum = m_engine->getDynoMaxSpeed();
        if (minimum < maximum)
            speed = std::clamp(speed, minimum, maximum);

        m_simulator->m_dyno.m_enabled = true;
        m_simulator->m_dyno.m_hold = true;
        m_simulator->m_dyno.m_rotationSpeed = speed;
    }
    else {
        m_simulator->m_dyno.m_enabled = false;
        m_simulator->m_dyno.m_hold = false;
        m_simulator->m_dyno.m_rotationSpeed = 0.0;
    }

    const int frequency = std::clamp(controls.simulationFrequency, 400, 400000);
    if (frequency != m_simulator->getSimulationFrequency()) {
        m_simulator->setSimulationFrequency(frequency);
        m_simulationFrequency = frequency;
    }

    Synthesizer::AudioParameters audio = m_simulator->synthesizer().getAudioParameters();
    audio.volume = controls.volume;
    audio.convolution = controls.convolution;
    audio.dF_F_mix = controls.highFrequencyGain;
    audio.airNoise = controls.noise;
    m_simulator->synthesizer().setAudioParameters(audio);
}

void EngineSimSession::renderChunk(int numSamples, float *output) {
    if (numSamples <= 0)
        return;

    Synthesizer &synthesizer = m_simulator->synthesizer();
    m_simulator->prepareAudioSteps();

    const int maxSteps = static_cast<int>(std::ceil(
        static_cast<double>(numSamples) * m_simulator->getSimulationFrequency() / m_sampleRate)) + 8;
    int steps = 0;
    while (synthesizer.queuedSamples() < numSamples && steps < maxSteps) {
        if (!m_simulator->simulateOneMoreStep())
            break;
        ++steps;
    }

    if (steps > 0)
        m_simulator->endFrame();

    synthesizer.renderBlock(numSamples, output);
}

void EngineSimSession::process(const BlockControls &controls, int numSamples, float *output) {
    if (output == nullptr || numSamples <= 0)
        return;

    if (m_simulator == nullptr || m_engine == nullptr || m_transmission == nullptr || m_sampleRate <= 0.0) {
        std::fill(output, output + numSamples, 0.0f);
        m_measuredRpm.store(0.0f, std::memory_order_relaxed);
        return;
    }

    applyControls(controls);

    int rendered = 0;
    while (rendered < numSamples) {
        const int chunk = std::min(kMaxChunkSamples, numSamples - rendered);
        renderChunk(chunk, output + rendered);
        rendered += chunk;
    }

    const float rpm = static_cast<float>(m_engine->getRpm());
    m_measuredRpm.store(rpm, std::memory_order_relaxed);
    if (m_crankUntilRunning && rpm > kStarterReleaseRpm)
        m_crankUntilRunning = false;
}
