#pragma once

#include "EngineSimProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

class EngineSimAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit EngineSimAudioProcessorEditor(EngineSimAudioProcessor &);
    ~EngineSimAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void timerCallback() override;

private:
    void refreshStatus();
    void chooseEngine();
    struct SliderControl {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    EngineSimAudioProcessor &m_processor;
    juce::Label m_status;
    juce::TextButton m_loadButton;
    std::unique_ptr<juce::FileChooser> m_chooser;
    bool m_alive = true;
    juce::Label m_rpmReadout;
    juce::ToggleButton m_ignition;
    juce::ToggleButton m_starter;
    juce::ToggleButton m_hold;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_ignitionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_starterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_holdAttachment;
    std::vector<std::unique_ptr<SliderControl>> m_sliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EngineSimAudioProcessorEditor)
};
