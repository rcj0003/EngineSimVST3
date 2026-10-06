#include "EngineSimEditor.h"

#include <iterator>

namespace {

constexpr int kKnobCount = 4;

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
    "Gear",
    "Volume",
    "Convolution",
    "HF Gain",
    "Noise",
    "Sim Freq"
};

constexpr const char *kSliderTips[] = {
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

constexpr juce::uint32 kBackground = 0xff0c0c0d;
constexpr juce::uint32 kInk = 0xffd0d0d4;
constexpr juce::uint32 kAmber = 0xffffb03a;

void styleCaption(juce::Label &label, const juce::String &text, const juce::String &tip) {
    label.setText(text, juce::dontSendNotification);
    label.setTooltip(tip);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions(12.0f)));
    label.setMinimumHorizontalScale(0.65f);
    label.setColour(juce::Label::textColourId, juce::Colour(kInk));
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
}

} // namespace

EngineSimAudioProcessorEditor::EngineSimAudioProcessorEditor(EngineSimAudioProcessor &processor)
    : AudioProcessorEditor(processor)
    , m_processor(processor)
    , m_tooltip(this, 700)
{
    setLookAndFeel(&m_lookAndFeel);

    addAndMakeVisible(m_display);
    refreshStatus();

    m_loadButton.setButtonText("Load Engine");
    m_loadButton.onClick = [this] { chooseEngine(); };
    addAndMakeVisible(m_loadButton);

    m_resetButton.setButtonText("Reset");
    m_resetButton.onClick = [this] {
        if (auto *parameter = m_processor.parameters().getParameter("reset")) {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(1.0f);
            parameter->setValueNotifyingHost(0.0f);
            parameter->endChangeGesture();
        }
    };
    addAndMakeVisible(m_resetButton);

    m_rpmReadout.setJustificationType(juce::Justification::centred);
    m_rpmReadout.setFont(juce::Font(juce::FontOptions(13.0f)));
    m_rpmReadout.setMinimumHorizontalScale(0.7f);
    m_rpmReadout.setColour(juce::Label::textColourId, juce::Colour(kAmber));
    m_rpmReadout.setColour(juce::Label::backgroundColourId, juce::Colour(0xff070708));
    m_rpmReadout.setColour(juce::Label::outlineColourId, juce::Colour(0xff2a2a2e));
    m_rpmReadout.setText("RPM  0", juce::dontSendNotification);
    addAndMakeVisible(m_rpmReadout);

    m_ignition.setButtonText("Ignition");
    m_starter.setButtonText("Starter");
    m_hold.setButtonText("RPM Hold");
    m_ignition.getProperties().set(RadioLookAndFeel::pushStart, true);
    m_starter.getProperties().set(RadioLookAndFeel::pushStart, true);
    m_ignition.setColour(juce::ToggleButton::textColourId, juce::Colour(kInk));
    m_starter.setColour(juce::ToggleButton::textColourId, juce::Colour(kInk));
    m_hold.setColour(juce::ToggleButton::textColourId, juce::Colour(kInk));
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
        styleCaption(control->label, kSliderNames[i], kSliderTips[i]);
        const bool knob = i < kKnobCount;
        control->slider.setSliderStyle(knob ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearVertical);
        control->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 16);
        control->slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kInk));
        control->slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        control->slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        control->slider.setTooltip(kSliderTips[i]);
        if (knob) {
            control->slider.setRotaryParameters(
                juce::MathConstants<float>::pi * 1.25f,
                juce::MathConstants<float>::pi * 2.75f,
                true);
        }
        control->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            m_processor.parameters(), kSliderIds[i], control->slider);
        addAndMakeVisible(control->label);
        addAndMakeVisible(control->slider);
        m_sliders.push_back(std::move(control));
    }

    setSize(960, 560);
    startTimerHz(10);
}

EngineSimAudioProcessorEditor::~EngineSimAudioProcessorEditor() {
    setLookAndFeel(nullptr);
    stopTimer();
    m_alive = false;
    m_chooser.reset();
}

void EngineSimAudioProcessorEditor::paint(juce::Graphics &g) {
    g.fillAll(juce::Colour(kBackground));
    m_lookAndFeel.paintNoise(g, getLocalBounds().toFloat(), 0.16f);
    m_lookAndFeel.paintPanel(g, m_knobWell.toFloat());
    m_lookAndFeel.paintPanel(g, m_eqWell.toFloat());
}

void EngineSimAudioProcessorEditor::refreshStatus() {
    const juce::String text = m_processor.statusText();
    m_display.setText(text);
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
    const auto rpmText = "RPM  " + juce::String(rpm);
    if (rpmText != m_rpmReadout.getText())
        m_rpmReadout.setText(rpmText, juce::dontSendNotification);
}

void EngineSimAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    m_display.setBounds(area.removeFromTop(50));
    area.removeFromTop(10);

    auto buttonRow = area.removeFromTop(34);
    constexpr int loadW = 148;
    constexpr int resetW = 104;
    constexpr int rpmW = 124;
    constexpr int buttonGap = 12;
    const int buttonGroupW = loadW + resetW + rpmW + buttonGap * 2;
    auto buttons = buttonRow.withSizeKeepingCentre(juce::jmin(buttonGroupW, buttonRow.getWidth()), buttonRow.getHeight());
    m_loadButton.setBounds(buttons.removeFromLeft(loadW));
    buttons.removeFromLeft(buttonGap);
    m_resetButton.setBounds(buttons.removeFromLeft(resetW));
    buttons.removeFromLeft(buttonGap);
    m_rpmReadout.setBounds(buttons);

    area.removeFromTop(10);
    auto pushRow = area.removeFromTop(112);
    constexpr int pushW = 96;
    constexpr int holdW = 124;
    constexpr int pushGap = 22;
    const int pushGroupW = pushW * 2 + holdW + pushGap * 2;
    auto pushes = pushRow.withSizeKeepingCentre(juce::jmin(pushGroupW, pushRow.getWidth()), pushRow.getHeight());
    m_ignition.setBounds(pushes.removeFromLeft(pushW));
    pushes.removeFromLeft(pushGap);
    m_starter.setBounds(pushes.removeFromLeft(pushW));
    pushes.removeFromLeft(pushGap);
    m_hold.setBounds(pushes.withSizeKeepingCentre(juce::jmin(holdW, pushes.getWidth()), 36));

    area.removeFromTop(12);
    constexpr int gutter = 14;
    auto left = area.removeFromLeft((area.getWidth() - gutter) / 2);
    area.removeFromLeft(gutter);
    m_knobWell = left;
    m_eqWell = area;

    auto knobArea = m_knobWell.reduced(12, 14);
    const int knobCols = kKnobCount;
    const int knobColW = knobCols > 0 ? juce::jmin(108, knobArea.getWidth() / knobCols) : 0;
    constexpr int labelH = 18;
    const int knobBlockH = knobColW + 6 + labelH;
    auto knobBlock = knobArea.withSizeKeepingCentre(knobColW * knobCols, juce::jmin(knobBlockH, knobArea.getHeight()));
    for (int i = 0; i < knobCols && i < static_cast<int>(m_sliders.size()); ++i) {
        auto column = knobBlock.removeFromLeft(knobColW);
        m_sliders[static_cast<size_t>(i)]->label.setBounds(column.removeFromBottom(labelH));
        column.removeFromBottom(4);
        m_sliders[static_cast<size_t>(i)]->slider.setBounds(column.reduced(4, 0));
    }

    auto eqArea = m_eqWell.reduced(12, 14);
    const int eqCols = static_cast<int>(m_sliders.size()) - kKnobCount;
    const int eqColW = eqCols > 0 ? juce::jmin(88, eqArea.getWidth() / eqCols) : 0;
    auto eqBlock = eqArea.withSizeKeepingCentre(eqColW * eqCols, eqArea.getHeight());
    for (int i = 0; i < eqCols; ++i) {
        auto column = eqBlock.removeFromLeft(eqColW);
        auto &control = *m_sliders[static_cast<size_t>(kKnobCount + i)];
        control.label.setBounds(column.removeFromBottom(labelH));
        column.removeFromBottom(2);
        control.slider.setBounds(column.reduced(2, 0));
    }
}
