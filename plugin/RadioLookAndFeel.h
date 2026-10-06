#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class RadioLookAndFeel : public juce::LookAndFeel_V4 {
public:
    static constexpr const char *pushStart = "pushStart";

    RadioLookAndFeel();

    void paintNoise(juce::Graphics &g, juce::Rectangle<float> area, float opacity) const;
    void paintPanel(juce::Graphics &g, juce::Rectangle<float> bounds) const;

    void drawButtonBackground(
        juce::Graphics &,
        juce::Button &,
        const juce::Colour &backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;
    void drawButtonText(
        juce::Graphics &,
        juce::TextButton &,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;
    void drawToggleButton(
        juce::Graphics &,
        juce::ToggleButton &,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;
    void drawLinearSlider(
        juce::Graphics &,
        int x,
        int y,
        int width,
        int height,
        float sliderPos,
        float minSliderPos,
        float maxSliderPos,
        juce::Slider::SliderStyle,
        juce::Slider &) override;
    void drawRotarySlider(
        juce::Graphics &,
        int x,
        int y,
        int width,
        int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider &) override;
    int getSliderThumbRadius(juce::Slider &) override;
    juce::Font getTextButtonFont(juce::TextButton &, int buttonHeight) override;
    juce::Label *createSliderTextBox(juce::Slider &) override;
    void fillTextEditorBackground(juce::Graphics &, int width, int height, juce::TextEditor &) override;
    void drawTextEditorOutline(juce::Graphics &, int width, int height, juce::TextEditor &) override;

private:
    juce::Image m_noise;
};
