#include "SegmentDisplay.h"

#include <array>
#include <cmath>

namespace {

// 16-segment cell. Bits, clockwise from the top, then the diagonals:
// a1 a2, upper-right, lower-right, d2 d1, lower-left, upper-left, g1 g2,
// upper-left diagonal, upper centre, upper-right diagonal,
// lower-left diagonal, lower centre, lower-right diagonal.
constexpr uint16_t kA1 = 1u << 0;
constexpr uint16_t kA2 = 1u << 1;
constexpr uint16_t kB = 1u << 2;
constexpr uint16_t kC = 1u << 3;
constexpr uint16_t kD2 = 1u << 4;
constexpr uint16_t kD1 = 1u << 5;
constexpr uint16_t kE = 1u << 6;
constexpr uint16_t kF = 1u << 7;
constexpr uint16_t kG1 = 1u << 8;
constexpr uint16_t kG2 = 1u << 9;
constexpr uint16_t kH = 1u << 10;
constexpr uint16_t kI = 1u << 11;
constexpr uint16_t kJ = 1u << 12;
constexpr uint16_t kK = 1u << 13;
constexpr uint16_t kL = 1u << 14;
constexpr uint16_t kM = 1u << 15;

constexpr uint16_t kTop = kA1 | kA2;
constexpr uint16_t kBottom = kD1 | kD2;
constexpr uint16_t kMid = kG1 | kG2;
constexpr uint16_t kLeft = kE | kF;
constexpr uint16_t kRight = kB | kC;
constexpr uint16_t kRing = kTop | kBottom | kLeft | kRight;

constexpr float kCellW = 16.0f;
constexpr float kCellH = 28.0f;
constexpr float kGap = 4.0f;
constexpr float kAdvance = kCellW + kGap;
constexpr float kPad = 10.0f;
constexpr int kHoldTicks = 16;

juce::String normalize(juce::String value) {
    value = value.toUpperCase();
    value = value.replaceCharacter(static_cast<juce::juce_wchar>(0x2014), '-');
    value = value.replaceCharacter(static_cast<juce::juce_wchar>(0x2013), '-');
    value = value.replaceCharacter(static_cast<juce::juce_wchar>(0x2212), '-');
    return value;
}

uint16_t maskFor(juce::juce_wchar c) {
    switch (c) {
    case '0': return kRing;
    case '1': return kB | kC;
    case '2': return kTop | kB | kMid | kE | kBottom;
    case '3': return kTop | kRight | kMid | kBottom;
    case '4': return kF | kMid | kRight;
    case '5': return kTop | kF | kMid | kC | kBottom;
    case '6': return kTop | kF | kMid | kE | kC | kBottom;
    case '7': return kTop | kRight;
    case '8': return kRing | kMid;
    case '9': return kTop | kF | kRight | kMid | kBottom;
    case 'A': return kTop | kLeft | kRight | kMid;
    case 'B': return kTop | kI | kL | kRight | kMid | kBottom;
    case 'C': return kTop | kLeft | kBottom;
    case 'D': return kTop | kI | kL | kRight | kBottom;
    case 'E': return kTop | kLeft | kMid | kBottom;
    case 'F': return kTop | kLeft | kMid;
    case 'G': return kTop | kLeft | kBottom | kC | kG2;
    case 'H': return kLeft | kRight | kMid;
    case 'I': return kTop | kBottom | kI | kL;
    case 'J': return kRight | kBottom | kE;
    case 'K': return kLeft | kG1 | kJ | kM;
    case 'L': return kLeft | kBottom;
    case 'M': return kLeft | kRight | kH | kJ;
    case 'N': return kLeft | kRight | kH | kM;
    case 'O': return kRing;
    case 'P': return kTop | kLeft | kB | kMid;
    case 'Q': return kRing | kM;
    case 'R': return kTop | kLeft | kB | kMid | kM;
    case 'S': return kTop | kF | kMid | kC | kBottom;
    case 'T': return kTop | kI | kL;
    case 'U': return kLeft | kRight | kBottom;
    case 'V': return kK | kM;
    case 'W': return kLeft | kRight | kK | kM;
    case 'X': return kH | kJ | kK | kM;
    case 'Y': return kH | kJ | kL;
    case 'Z': return kTop | kBottom | kJ | kK;
    case '-': return kMid;
    case '_': return kBottom;
    case '/': return kJ | kK;
    case '\\': return kH | kM;
    case '+': return kMid | kI | kL;
    case '=': return kMid | kBottom;
    case '(': return kA1 | kF | kE | kD1;
    case ')': return kA2 | kB | kC | kD2;
    case '\'': return kI;
    case '?': return kTop | kB | kG2 | kL;
    default: return 0;
    }
}

bool isDot(juce::juce_wchar c) {
    return c == '.' || c == ',';
}

bool isColon(juce::juce_wchar c) {
    return c == ':' || c == ';';
}

void addHorizontal(juce::Path &path, float x0, float x1, float y, float thickness) {
    if (x1 - x0 < thickness)
        return;

    const float ht = thickness * 0.5f;
    path.startNewSubPath(x0, y);
    path.lineTo(x0 + ht, y - ht);
    path.lineTo(x1 - ht, y - ht);
    path.lineTo(x1, y);
    path.lineTo(x1 - ht, y + ht);
    path.lineTo(x0 + ht, y + ht);
    path.closeSubPath();
}

void addVertical(juce::Path &path, float x, float y0, float y1, float thickness) {
    if (y1 - y0 < thickness)
        return;

    const float ht = thickness * 0.5f;
    path.startNewSubPath(x, y0);
    path.lineTo(x + ht, y0 + ht);
    path.lineTo(x + ht, y1 - ht);
    path.lineTo(x, y1);
    path.lineTo(x - ht, y1 - ht);
    path.lineTo(x - ht, y0 + ht);
    path.closeSubPath();
}

void addDiagonal(juce::Path &path, float x0, float y0, float x1, float y1, float thickness) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < thickness)
        return;

    const float nx = -dy / len * thickness * 0.5f;
    const float ny = dx / len * thickness * 0.5f;
    const float sx = dx / len * thickness * 0.45f;
    const float sy = dy / len * thickness * 0.45f;
    path.startNewSubPath(x0 + sx + nx, y0 + sy + ny);
    path.lineTo(x1 - sx + nx, y1 - sy + ny);
    path.lineTo(x1 - sx - nx, y1 - sy - ny);
    path.lineTo(x0 + sx - nx, y0 + sy - ny);
    path.closeSubPath();
}

struct CellPaths {
    juce::Path ghost;
    std::array<juce::Path, 16> segments;
};

const CellPaths &cellPaths() {
    static const CellPaths paths = [] {
        CellPaths built;
        const float t = 2.15f;
        const float left = 1.3f;
        const float right = kCellW - 1.3f;
        const float top = 1.3f;
        const float bottom = kCellH - 1.3f;
        const float mid = kCellH * 0.5f;
        const float cx = kCellW * 0.5f;
        const float gap = 1.15f;

        addHorizontal(built.segments[0], left, cx - gap, top + t * 0.15f, t);
        addHorizontal(built.segments[1], cx + gap, right, top + t * 0.15f, t);
        addVertical(built.segments[2], right - t * 0.15f, top + t, mid - gap, t);
        addVertical(built.segments[3], right - t * 0.15f, mid + gap, bottom - t, t);
        addHorizontal(built.segments[4], cx + gap, right, bottom - t * 0.15f, t);
        addHorizontal(built.segments[5], left, cx - gap, bottom - t * 0.15f, t);
        addVertical(built.segments[6], left + t * 0.15f, mid + gap, bottom - t, t);
        addVertical(built.segments[7], left + t * 0.15f, top + t, mid - gap, t);
        addHorizontal(built.segments[8], left + t * 0.3f, cx - gap, mid, t);
        addHorizontal(built.segments[9], cx + gap, right - t * 0.3f, mid, t);
        addDiagonal(built.segments[10], left + t, top + t, cx - gap, mid - gap, t * 0.85f);
        addVertical(built.segments[11], cx, top + t, mid - gap, t * 0.85f);
        addDiagonal(built.segments[12], right - t, top + t, cx + gap, mid - gap, t * 0.85f);
        addDiagonal(built.segments[13], left + t, bottom - t, cx - gap, mid + gap, t * 0.85f);
        addVertical(built.segments[14], cx, mid + gap, bottom - t, t * 0.85f);
        addDiagonal(built.segments[15], right - t, bottom - t, cx + gap, mid + gap, t * 0.85f);

        for (const auto &segment : built.segments)
            built.ghost.addPath(segment);

        return built;
    }();
    return paths;
}

float textWidth(const juce::String &text) {
    if (text.isEmpty())
        return 0.0f;

    return static_cast<float>(text.length()) * kAdvance - kGap;
}

void paintDot(juce::Graphics &g, float x, float y, float size) {
    g.fillRoundedRectangle({ x, y, size, size }, 0.6f);
}

} // namespace

SegmentDisplay::SegmentDisplay() = default;

SegmentDisplay::~SegmentDisplay() {
    stopTimer();
}

void SegmentDisplay::setText(const juce::String &text) {
    setTooltip(text);
    const auto normalized = normalize(text);
    if (normalized == m_text)
        return;

    m_text = normalized;
    m_scroll = 0.0f;
    m_phase = ScrollPhase::holdStart;
    m_holdTicks = kHoldTicks;
    updateMotion();
}

void SegmentDisplay::resized() {
    updateMotion();
}

void SegmentDisplay::updateMotion() {
    m_textWidth = textWidth(m_text);
    const float visible = static_cast<float>(getWidth()) - 2.0f * kPad;
    const bool shouldScroll = getWidth() > 0 && m_textWidth > visible;

    if (!shouldScroll) {
        m_scrolling = false;
        m_scroll = 0.0f;
        stopTimer();
        repaint();
        return;
    }

    m_scroll = juce::jlimit(0.0f, m_textWidth - visible, m_scroll);
    if (!m_scrolling) {
        m_scrolling = true;
        m_phase = ScrollPhase::holdStart;
        m_holdTicks = kHoldTicks;
    }

    if (!isTimerRunning())
        startTimerHz(20);

    repaint();
}

void SegmentDisplay::timerCallback() {
    const float visible = static_cast<float>(getWidth()) - 2.0f * kPad;
    const float travel = m_textWidth - visible;
    if (travel <= 0.0f) {
        updateMotion();
        return;
    }

    if (m_phase == ScrollPhase::holdStart || m_phase == ScrollPhase::holdEnd) {
        if (--m_holdTicks > 0)
            return;

        if (m_phase == ScrollPhase::holdStart) {
            m_phase = ScrollPhase::move;
        } else {
            m_scroll = 0.0f;
            m_phase = ScrollPhase::holdStart;
            m_holdTicks = kHoldTicks;
            repaint();
            return;
        }
    }

    m_scroll += 2.0f;
    if (m_scroll >= travel) {
        m_scroll = travel;
        m_phase = ScrollPhase::holdEnd;
        m_holdTicks = kHoldTicks;
    }

    repaint();
}

void SegmentDisplay::paint(juce::Graphics &g) {
    auto outer = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff050506));
    g.fillRoundedRectangle(outer, 5.0f);

    auto glass = outer.reduced(3.0f);
    g.setColour(juce::Colour(0xff030304));
    g.fillRoundedRectangle(glass, 3.0f);

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawLine(glass.getX() + 4.0f, glass.getY() + 1.0f, glass.getRight() - 4.0f, glass.getY() + 1.0f, 1.0f);

    const auto &paths = cellPaths();
    const float visible = static_cast<float>(getWidth()) - 2.0f * kPad;
    float origin = kPad;
    if (m_textWidth <= visible)
        origin = (static_cast<float>(getWidth()) - m_textWidth) * 0.5f;
    else
        origin = kPad - m_scroll;

    origin = static_cast<float>(juce::roundToInt(origin));
    const float baseline = static_cast<float>(juce::roundToInt((static_cast<float>(getHeight()) - kCellH) * 0.5f));
    const auto lit = juce::Colour(0xffffb03a);
    const auto ghost = lit.withAlpha(0.07f);

    {
        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(glass.toNearestInt());

        const int count = m_text.length();
        int first = 0;
        if (origin < 0.0f)
            first = static_cast<int>((-origin) / kAdvance);
        int last = count;
        if (kAdvance > 0.0f)
            last = static_cast<int>((static_cast<float>(getWidth()) - origin) / kAdvance) + 1;
        first = juce::jlimit(0, count, first);
        last = juce::jlimit(first, count, last);

        for (int i = first; i < last; ++i) {
            const float x = origin + static_cast<float>(i) * kAdvance;
            const auto transform = juce::AffineTransform::translation(x, baseline);
            const auto character = m_text[i];

            g.setColour(ghost);
            g.fillPath(paths.ghost, transform);

            const auto mask = maskFor(character);
            g.setColour(lit);
            for (int bit = 0; bit < 16; ++bit) {
                if ((mask & (1u << bit)) != 0)
                    g.fillPath(paths.segments[static_cast<size_t>(bit)], transform);
            }

            if (isDot(character))
                paintDot(g, x + kCellW * 0.5f - 1.3f, baseline + kCellH - 4.2f, 2.6f);
            else if (isColon(character)) {
                paintDot(g, x + kCellW * 0.5f - 1.2f, baseline + kCellH * 0.32f, 2.4f);
                paintDot(g, x + kCellW * 0.5f - 1.2f, baseline + kCellH * 0.62f, 2.4f);
            }
        }

        g.setColour(juce::Colours::black.withAlpha(0.18f));
        const int top = juce::roundToInt(glass.getY());
        const int bottom = juce::roundToInt(glass.getBottom());
        for (int y = top; y < bottom; y += 2)
            g.drawHorizontalLine(y, glass.getX(), glass.getRight());
    }

    g.setColour(juce::Colour(0xff000000));
    g.drawRoundedRectangle(outer.reduced(0.5f), 5.0f, 1.5f);
}
