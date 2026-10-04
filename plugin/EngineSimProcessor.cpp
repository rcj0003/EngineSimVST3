#include "EngineSimProcessor.h"

#include "EngineSimEditor.h"

#include <array>

namespace {

void applyNoteMessage(std::array<bool, 128> &noteDown, int &activeNotes, const juce::MidiMessage &message) {
    if (message.isAllNotesOff() || message.isAllSoundOff()) {
        if (activeNotes != 0) {
            noteDown.fill(false);
            activeNotes = 0;
        }
        return;
    }

    if (!message.isNoteOn() && !message.isNoteOff())
        return;

    const int note = message.getNoteNumber();
    if (note < 0 || note >= 128)
        return;

    const size_t index = static_cast<size_t>(note);
    if (message.isNoteOn()) {
        if (!noteDown[index]) {
            noteDown[index] = true;
            ++activeNotes;
        }
        return;
    }

    if (noteDown[index]) {
        noteDown[index] = false;
        --activeNotes;
    }
}

juce::AudioParameterFloat *floatParameter(juce::AudioProcessorValueTreeState &state, const char *id) {
    return dynamic_cast<juce::AudioParameterFloat *>(state.getParameter(id));
}

juce::AudioParameterBool *boolParameter(juce::AudioProcessorValueTreeState &state, const char *id) {
    return dynamic_cast<juce::AudioParameterBool *>(state.getParameter(id));
}

juce::AudioParameterInt *intParameter(juce::AudioProcessorValueTreeState &state, const char *id) {
    return dynamic_cast<juce::AudioParameterInt *>(state.getParameter(id));
}

} // namespace

EngineSimAudioProcessor::EngineSimAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , m_parameters(*this, nullptr, juce::Identifier("EngineSim"), createParameterLayout())
{
    bindParameters();

    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    if (!m_session.loadNear(executable.getFullPathName().toStdString()))
        m_loadError = juce::String(m_session.error());
    else
        applyEngineDefaults();

    m_scriptPath = juce::String(m_session.scriptPath());
    m_warnings = juce::String(m_session.warnings());
    m_assetDirectory = juce::String(m_session.assetDirectory());
}

EngineSimAudioProcessor::~EngineSimAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout EngineSimAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"throttle", 1},
        "Throttle",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"ignition", 1},
        "Ignition",
        true));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"starter", 1},
        "Starter",
        false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"clutch", 1},
        "Clutch",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.0f));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"gear", 1},
        "Gear",
        0,
        12,
        0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"volume", 1},
        "Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"convolution", 1},
        "Convolution",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hfGain", 1},
        "High Frequency Gain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f, 0.25f),
        0.01f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"noise", 1},
        "Noise",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"simFrequency", 1},
        "Simulation Frequency",
        juce::NormalisableRange<float>(400.0f, 400000.0f, 1.0f, 0.25f),
        10000.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"hold", 1},
        "RPM Hold",
        false));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"rpm", 1},
        "RPM",
        0,
        20000,
        3000));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"reset", 1},
        "Reset",
        false));
    return layout;
}

void EngineSimAudioProcessor::bindParameters() {
    m_throttle = floatParameter(m_parameters, "throttle");
    m_ignition = boolParameter(m_parameters, "ignition");
    m_starter = boolParameter(m_parameters, "starter");
    m_clutch = floatParameter(m_parameters, "clutch");
    m_gear = intParameter(m_parameters, "gear");
    m_volume = floatParameter(m_parameters, "volume");
    m_convolution = floatParameter(m_parameters, "convolution");
    m_highFrequencyGain = floatParameter(m_parameters, "highFrequencyGain");
    m_noise = floatParameter(m_parameters, "noise");
    m_simulationFrequency = floatParameter(m_parameters, "simFrequency");
    m_hold = boolParameter(m_parameters, "hold");
    m_rpm = intParameter(m_parameters, "rpm");
    m_reset = boolParameter(m_parameters, "reset");
}

void EngineSimAudioProcessor::applyEngineDefaults() {
    if (m_volume != nullptr)
        *m_volume = m_session.volume();
    if (m_convolution != nullptr)
        *m_convolution = m_session.convolution();
    if (m_highFrequencyGain != nullptr)
        *m_highFrequencyGain = m_session.highFrequencyGain();
    if (m_noise != nullptr)
        *m_noise = m_session.noise();
    if (m_simulationFrequency != nullptr)
        *m_simulationFrequency = static_cast<float>(m_session.simulationFrequency());
}

EngineSimSession::BlockControls EngineSimAudioProcessor::readControls() const {
    EngineSimSession::BlockControls controls;
    controls.throttle = m_throttle != nullptr ? m_throttle->get() : 0.0f;
    controls.ignition = m_ignition != nullptr ? m_ignition->get() : true;
    controls.starter = m_starter != nullptr ? m_starter->get() : false;
    controls.clutch = m_clutch != nullptr ? m_clutch->get() : 0.0f;
    controls.gear = m_gear != nullptr ? m_gear->get() - 1 : -1;
    controls.volume = m_volume != nullptr ? m_volume->get() : 1.0f;
    controls.convolution = m_convolution != nullptr ? m_convolution->get() : 1.0f;
    controls.highFrequencyGain = m_highFrequencyGain != nullptr ? m_highFrequencyGain->get() : 0.01f;
    controls.noise = m_noise != nullptr ? m_noise->get() : 1.0f;
    controls.simulationFrequency = m_simulationFrequency != nullptr
        ? static_cast<int>(m_simulationFrequency->get())
        : 10000;
    controls.hold = m_hold != nullptr ? m_hold->get() : false;
    controls.rpm = m_rpm != nullptr ? m_rpm->get() : 3000;
    controls.reset = m_reset != nullptr ? m_reset->get() : false;
    return controls;
}

bool EngineSimAudioProcessor::loadScript(const juce::String &path, bool applyDefaults) {
    EngineSimSession::CompiledEngine compiled;
    std::string error;
    if (!m_session.compileScript(path.toStdString(), compiled, error)) {
        const juce::ScopedLock lock(m_engineLock);
        m_loadError = juce::String(error);
        return false;
    }

    const juce::ScopedLock lock(m_engineLock);
    if (!m_session.install(compiled)) {
        m_loadError = juce::String(m_session.error());
        return false;
    }

    if (m_sampleRate > 0.0)
        m_session.prepare(m_sampleRate);

    m_scriptPath = juce::String(m_session.scriptPath());
    m_warnings = juce::String(m_session.warnings());
    m_loadError.clear();
    m_parameters.state.setProperty("engineScript", m_scriptPath, nullptr);

    if (applyDefaults)
        applyEngineDefaults();
    return true;
}

juce::String EngineSimAudioProcessor::scriptPath() const {
    const juce::ScopedLock lock(m_engineLock);
    return m_scriptPath;
}

juce::String EngineSimAudioProcessor::assetDirectory() const {
    return m_assetDirectory;
}

juce::String EngineSimAudioProcessor::statusText() const {
    const juce::ScopedLock lock(m_engineLock);
    if (m_loadError.isNotEmpty())
        return m_loadError;

    juce::String text = m_scriptPath.isNotEmpty() ? m_scriptPath : juce::String("Engine Sim");
    if (m_warnings.isNotEmpty())
        text += " — " + m_warnings;
    return text;
}

void EngineSimAudioProcessor::prepareToPlay(double sampleRate, int) {
    const juce::ScopedLock lock(m_engineLock);
    m_sampleRate = sampleRate;
    m_session.prepare(sampleRate);
    setLatencySamples(0);
}

void EngineSimAudioProcessor::releaseResources() {
}

bool EngineSimAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void EngineSimAudioProcessor::applyMidiMessage(const juce::MidiMessage &message) {
    applyNoteMessage(m_noteDown, m_activeNotes, message);
}

void EngineSimAudioProcessor::applyMidiBuffer(const juce::MidiBuffer &midi) {
    for (const auto metadata : midi)
        applyMidiMessage(metadata.getMessage());
}

bool EngineSimAudioProcessor::gateIsUniform(const juce::MidiBuffer &midi, int numSamples, bool &active) const {
    auto noteDown = m_noteDown;
    int activeNotes = m_activeNotes;
    bool decided = false;
    bool uniformActive = false;
    int cursor = 0;

    const auto consumeRange = [&](int end) {
        if (end <= cursor)
            return true;

        const bool rangeActive = activeNotes > 0;
        if (!decided) {
            uniformActive = rangeActive;
            decided = true;
        }
        else if (rangeActive != uniformActive) {
            return false;
        }

        cursor = end;
        return true;
    };

    for (const auto metadata : midi) {
        const int eventPos = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (!consumeRange(eventPos))
            return false;
        applyNoteMessage(noteDown, activeNotes, metadata.getMessage());
    }

    if (!consumeRange(numSamples))
        return false;

    active = decided && uniformActive;
    return true;
}

void EngineSimAudioProcessor::writeSegment(
    juce::AudioBuffer<float> &buffer,
    int start,
    int count,
    bool active,
    const EngineSimSession::BlockControls &controls) {
    if (count <= 0)
        return;

    if (!active) {
        buffer.clear(start, count);
        return;
    }

    m_session.process(controls, count, buffer.getWritePointer(0) + start);
    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.copyFrom(channel, start, buffer, 0, start, count);
}

void EngineSimAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi) {
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0) {
        applyMidiBuffer(midi);
        buffer.clear();
        return;
    }

    const juce::ScopedTryLock lock(m_engineLock);
    if (!lock.isLocked() || !m_session.loaded()) {
        applyMidiBuffer(midi);
        buffer.clear();
        return;
    }

    const EngineSimSession::BlockControls controls = readControls();
    m_session.applyResetEdge(controls.reset);

    bool active = false;
    if (gateIsUniform(midi, numSamples, active)) {
        applyMidiBuffer(midi);
        if (!active) {
            buffer.clear();
            return;
        }

        m_session.process(controls, numSamples, buffer.getWritePointer(0));
        for (int channel = 1; channel < numChannels; ++channel)
            buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
        return;
    }

    int cursor = 0;
    for (const auto metadata : midi) {
        const int eventPos = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (eventPos > cursor) {
            writeSegment(buffer, cursor, eventPos - cursor, m_activeNotes > 0, controls);
            cursor = eventPos;
        }
        applyMidiMessage(metadata.getMessage());
    }

    if (cursor < numSamples)
        writeSegment(buffer, cursor, numSamples - cursor, m_activeNotes > 0, controls);
}

juce::AudioProcessorEditor *EngineSimAudioProcessor::createEditor() {
    return new EngineSimAudioProcessorEditor(*this);
}

void EngineSimAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
    juce::ValueTree state = m_parameters.copyState();
    state.setProperty("engineScript", scriptPath(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void EngineSimAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        if (xml->hasTagName(m_parameters.state.getType())) {
            juce::ValueTree state = juce::ValueTree::fromXml(*xml);
            const juce::String path = state.getProperty("engineScript").toString();
            m_parameters.replaceState(state);
            if (path.isNotEmpty() && path != scriptPath())
                loadScript(path, false);
        }
    }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new EngineSimAudioProcessor();
}
