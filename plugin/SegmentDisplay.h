#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Single-line 16-segment readout. Text that fits is centered. Longer text
// pauses, then scrolls horizontally until the end is visible, then repeats.
class SegmentDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    SegmentDisplay();
    ~SegmentDisplay() override;

    void setText(const juce::String &text);

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    enum class ScrollPhase { holdStart, move, holdEnd };

    void timerCallback() override;
    void updateMotion();

    juce::String m_text;
    float m_textWidth = 0.0f;
    float m_scroll = 0.0f;
    ScrollPhase m_phase = ScrollPhase::holdStart;
    int m_holdTicks = 0;
    bool m_scrolling = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SegmentDisplay)
};
