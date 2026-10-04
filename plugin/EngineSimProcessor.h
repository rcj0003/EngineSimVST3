#pragma once

#include "engine_session.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

class EngineSimAudioProcessor : public juce::AudioProcessor {
public:
    EngineSimAudioProcessor();
    ~EngineSimAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Engine Sim"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState &parameters() { return m_parameters; }
    // Compiles outside the audio lock, then installs under it. applyDefaults
    // copies the script's mix settings into the parameters; preset restore leaves them.
    bool loadScript(const juce::String &path, bool applyDefaults);
    juce::String scriptPath() const;
    juce::String assetDirectory() const;
    juce::String statusText() const;
    float measuredRpm() const { return m_session.measuredRpm(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void bindParameters();
    void applyEngineDefaults();
    EngineSimSession::BlockControls readControls() const;
    void applyMidiMessage(const juce::MidiMessage &message);
    void applyMidiBuffer(const juce::MidiBuffer &midi);
    // True when the block is entirely held or entirely silent. active is that held state.
    bool gateIsUniform(const juce::MidiBuffer &midi, int numSamples, bool &active) const;
    void writeSegment(
        juce::AudioBuffer<float> &buffer,
        int start,
        int count,
        bool active,
        const EngineSimSession::BlockControls &controls);

    EngineSimSession m_session;
    juce::CriticalSection m_engineLock;
    juce::AudioProcessorValueTreeState m_parameters;
    juce::String m_loadError;
    juce::String m_scriptPath;
    juce::String m_warnings;
    juce::String m_assetDirectory;
    double m_sampleRate = 0.0;

    juce::AudioParameterFloat *m_throttle = nullptr;
    juce::AudioParameterBool *m_ignition = nullptr;
    juce::AudioParameterBool *m_starter = nullptr;
    juce::AudioParameterFloat *m_clutch = nullptr;
    juce::AudioParameterInt *m_gear = nullptr;
    juce::AudioParameterFloat *m_volume = nullptr;
    juce::AudioParameterFloat *m_convolution = nullptr;
    juce::AudioParameterFloat *m_highFrequencyGain = nullptr;
    juce::AudioParameterFloat *m_noise = nullptr;
    juce::AudioParameterFloat *m_simulationFrequency = nullptr;
    juce::AudioParameterBool *m_hold = nullptr;
    juce::AudioParameterInt *m_rpm = nullptr;
    std::array<bool, 128> m_noteDown{};
    int m_activeNotes = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EngineSimAudioProcessor)
};
