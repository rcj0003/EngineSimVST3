#include "EngineSimEditor.h"

#include <iterator>

namespace {

constexpr const char *kSliderIds[] = {
    "throttle",
    "rpm",
    "clutch",
    "gear",
    "volume",
    "convolution",
    "hfGain",
    "noise",
    "simFrequency"
};

constexpr const char *kSliderNames[] = {
    "Throttle",
    "RPM",
    "Clutch",
    "Gear (0 = N)",
    "Volume",
    "Convolution",
    "High Frequency Gain",
    "Noise",
    "Simulation Frequency"
};

} // namespace

EngineSimAudioProcessorEditor::EngineSimAudioProcessorEditor(EngineSimAudioProcessor &processor)
    : AudioProcessorEditor(processor)
    , m_processor(processor)
{
    m_status.setJustificationType(juce::Justification::centredLeft);
    m_status.setColour(juce::Label::textColourId, juce::Colours::white);
    m_status.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(m_status);
    refreshStatus();

    m_loadButton.setButtonText("Load Engine...");
    m_loadButton.onClick = [this] { chooseEngine(); };
    addAndMakeVisible(m_loadButton);

    m_rpmReadout.setJustificationType(juce::Justification::centredLeft);
    m_rpmReadout.setColour(juce::Label::textColourId, juce::Colours::white);
    m_rpmReadout.setText("RPM  0", juce::dontSendNotification);
    addAndMakeVisible(m_rpmReadout);

    m_ignition.setButtonText("Ignition");
    m_starter.setButtonText("Starter");
    m_hold.setButtonText("RPM Hold");
    m_ignition.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    m_starter.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    m_hold.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(m_ignition);
    addAndMakeVisible(m_starter);
    addAndMakeVisible(m_hold);
    m_ignitionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_processor.parameters(), "ignition", m_ignition);
    m_starterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_processor.parameters(), "starter", m_starter);
    m_holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_processor.parameters(), "hold", m_hold);

    for (int i = 0; i < static_cast<int>(std::size(kSliderIds)); ++i) {
        auto control = std::make_unique<SliderControl>();
        control->label.setText(kSliderNames[i], juce::dontSendNotification);
        control->label.setJustificationType(juce::Justification::centredLeft);
        control->label.setColour(juce::Label::textColourId, juce::Colours::white);
        control->slider.setSliderStyle(juce::Slider::LinearHorizontal);
        control->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
        control->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            m_processor.parameters(), kSliderIds[i], control->slider);
        addAndMakeVisible(control->label);
        addAndMakeVisible(control->slider);
        m_sliders.push_back(std::move(control));
    }

    setSize(640, 580);
    startTimerHz(10);
}

EngineSimAudioProcessorEditor::~EngineSimAudioProcessorEditor() {
    stopTimer();
    m_alive = false;
    m_chooser.reset();
}

void EngineSimAudioProcessorEditor::paint(juce::Graphics &g) {
    g.fillAll(juce::Colour(0xff1c1f22));
}

void EngineSimAudioProcessorEditor::refreshStatus() {
    const juce::String text = m_processor.statusText();
    m_status.setTooltip(text);
    if (text != m_status.getText())
        m_status.setText(text, juce::dontSendNotification);
}

void EngineSimAudioProcessorEditor::chooseEngine() {
    juce::File start;
    const juce::String current = m_processor.scriptPath();
    if (current.isNotEmpty())
        start = juce::File(current).getParentDirectory();

    if (!start.isDirectory()) {
        const juce::String assets = m_processor.assetDirectory();
        if (assets.isNotEmpty())
            start = juce::File(assets).getChildFile("engines");
    }

    if (!start.isDirectory())
        start = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    m_chooser = std::make_unique<juce::FileChooser>(
        "Select an engine script",
        start,
        "*.mr",
        true,
        false,
        this);
    juce::Component::SafePointer<EngineSimAudioProcessorEditor> safe(this);
    m_chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser &chooser) {
            if (safe == nullptr || !safe->m_alive)
                return;

            const juce::File file = chooser.getResult();
            if (file == juce::File())
                return;

            safe->m_processor.loadScript(file.getFullPathName(), true);
            if (safe != nullptr)
                safe->refreshStatus();
        });
}

void EngineSimAudioProcessorEditor::timerCallback() {
    refreshStatus();
    const int rpm = juce::roundToInt(m_processor.measuredRpm());
    m_rpmReadout.setText("RPM  " + juce::String(rpm), juce::dontSendNotification);
}

void EngineSimAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(12);
    m_status.setBounds(area.removeFromTop(36));
    area.removeFromTop(4);
    m_loadButton.setBounds(area.removeFromTop(28).removeFromLeft(160));
    area.removeFromTop(4);
    m_rpmReadout.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);

    auto toggles = area.removeFromTop(28);
    const int toggleWidth = toggles.getWidth() / 3;
    m_ignition.setBounds(toggles.removeFromLeft(toggleWidth));
    m_starter.setBounds(toggles.removeFromLeft(toggleWidth));
    m_hold.setBounds(toggles);
    area.removeFromTop(8);

    const int rowCount = static_cast<int>(m_sliders.size());
    const int rowHeight = rowCount > 0 ? area.getHeight() / rowCount : 0;
    for (auto &control : m_sliders) {
        auto row = area.removeFromTop(rowHeight);
        control->label.setBounds(row.removeFromLeft(180));
        control->slider.setBounds(row);
    }
}
