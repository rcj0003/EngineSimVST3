#include "RadioLookAndFeel.h"

namespace {

constexpr juce::uint32 kFace = 0xff121214;
constexpr juce::uint32 kFaceLit = 0xff1a1a1d;
constexpr juce::uint32 kRim = 0xff2c2c30;
constexpr juce::uint32 kInk = 0xffd0d0d4;
constexpr juce::uint32 kSlot = 0xff050506;

juce::Font faceFont(float height) {
    return juce::Font(juce::FontOptions(height));
}

void paintRaisedFace(juce::Graphics &g, juce::Rectangle<float> face, float corner, bool down, bool highlighted) {
    const auto top = juce::Colour(down ? 0xff101012 : (highlighted ? kFaceLit : kFace));
    const auto bottom = top.darker(down ? 0.15f : 0.45f);
    juce::ColourGradient gradient(top.brighter(0.08f), face.getX(), face.getY(), bottom, face.getX(), face.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(face, corner);

    g.setColour(juce::Colours::white.withAlpha(down ? 0.03f : 0.07f));
    auto sheen = face.reduced(1.5f, 1.0f);
    sheen.setHeight(juce::jmax(2.0f, face.getHeight() * 0.42f));
    g.fillRoundedRectangle(sheen, juce::jmax(1.0f, corner - 1.0f));

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(face.reduced(0.5f), corner, 1.0f);
    g.setColour(juce::Colour(kRim).withAlpha(highlighted ? 0.9f : 0.55f));
    g.drawRoundedRectangle(face, corner, 1.0f);
}

void paintLed(juce::Graphics &g, juce::Rectangle<float> led, bool on) {
    g.setColour(juce::Colour(0xff050506));
    g.fillEllipse(led.expanded(1.4f));

    const auto glow = on ? juce::Colour(0xffff9a2a) : juce::Colour(0xff2a2418);
    const auto core = on ? juce::Colour(0xfffff1d2) : juce::Colour(0xff3a3428);
    juce::ColourGradient gradient(
        core,
        led.getCentreX(),
        led.getCentreY() - led.getHeight() * 0.15f,
        glow,
        led.getCentreX(),
        led.getBottom(),
        true);
    g.setGradientFill(gradient);
    g.fillEllipse(led);

    g.setColour(juce::Colour(0xff1a140c));
    g.drawEllipse(led, 1.0f);
}

void paintCentredCaption(juce::Graphics &g, const juce::String &text, juce::Rectangle<float> area, juce::Font font, bool down) {
    if (down)
        area = area.translated(0.0f, 1.0f);

    g.setFont(font);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawFittedText(text, area.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1);
    g.setColour(juce::Colour(kInk));
    g.drawFittedText(text, area.toNearestInt(), juce::Justification::centred, 1);
}

} // namespace

RadioLookAndFeel::RadioLookAndFeel() {
    m_noise = juce::Image(juce::Image::ARGB, 128, 128, true);
    {
        juce::Image::BitmapData pixels(m_noise, juce::Image::BitmapData::writeOnly);
        juce::Random rng(0x51A7E);
        for (int y = 0; y < pixels.height; ++y) {
            for (int x = 0; x < pixels.width; ++x) {
                const auto shade = static_cast<juce::uint8>(140 + rng.nextInt(90));
                const auto alpha = static_cast<juce::uint8>(20 + rng.nextInt(36));
                pixels.setPixelColour(x, y, juce::Colour(shade, shade, shade, alpha));
            }
        }
    }

    setColour(juce::Slider::textBoxTextColourId, juce::Colour(kInk));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xff2a2a2e));
    setColour(juce::Label::textColourId, juce::Colour(kInk));
    setColour(juce::TextButton::textColourOffId, juce::Colour(kInk));
    setColour(juce::TextButton::textColourOnId, juce::Colour(kInk));
    setColour(juce::TextEditor::textColourId, juce::Colour(kInk));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff101012));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(kRim));
    setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff2a2a2e));
    setColour(juce::CaretComponent::caretColourId, juce::Colour(kInk));
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff101012));
    setColour(juce::TooltipWindow::textColourId, juce::Colour(kInk));
    setColour(juce::TooltipWindow::outlineColourId, juce::Colour(kRim));
}

void RadioLookAndFeel::paintNoise(juce::Graphics &g, juce::Rectangle<float> area, float opacity) const {
    if (area.isEmpty() || opacity <= 0.0f)
        return;

    g.setTiledImageFill(m_noise, area.getX(), area.getY(), opacity);
    g.fillRect(area);
}

void RadioLookAndFeel::paintPanel(juce::Graphics &g, juce::Rectangle<float> bounds) const {
    if (bounds.isEmpty())
        return;

    g.setColour(juce::Colour(0xff070708));
    g.fillRoundedRectangle(bounds, 8.0f);

    auto face = bounds.reduced(1.0f);
    juce::ColourGradient gradient(
        juce::Colour(0xff101012),
        face.getX(),
        face.getY(),
        juce::Colour(0xff080809),
        face.getX(),
        face.getBottom(),
        false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(face, 7.0f);

    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(face, 7.0f);
        g.reduceClipRegion(clip);
        paintNoise(g, face, 0.22f);

        g.setColour(juce::Colours::white.withAlpha(0.018f));
        const int top = juce::roundToInt(face.getY());
        const int bottom = juce::roundToInt(face.getBottom());
        for (int y = top; y < bottom; y += 3)
            g.drawHorizontalLine(y, face.getX(), face.getRight());
    }

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(face.reduced(0.5f), 7.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.5f);
}

void RadioLookAndFeel::drawButtonBackground(
    juce::Graphics &g,
    juce::Button &button,
    const juce::Colour &,
    bool highlighted,
    bool down) {
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 4.0f);

    auto face = bounds.reduced(1.5f);
    if (down)
        face = face.reduced(0.5f).translated(0.0f, 1.0f);

    paintRaisedFace(g, face, 3.0f, down, highlighted);

    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(face.reduced(1.0f), 2.5f);
    g.reduceClipRegion(clip);
    paintNoise(g, face, 0.28f);
}

void RadioLookAndFeel::drawButtonText(juce::Graphics &g, juce::TextButton &button, bool, bool down) {
    paintCentredCaption(g, button.getButtonText(), button.getLocalBounds().toFloat(), getTextButtonFont(button, button.getHeight()), down);
}

void RadioLookAndFeel::drawToggleButton(juce::Graphics &g, juce::ToggleButton &button, bool highlighted, bool down) {
    const bool on = button.getToggleState();
    const bool pushed = down || on;
    auto bounds = button.getLocalBounds().toFloat();

    if (button.getProperties()[pushStart]) {
        const float ledD = 9.0f;
        const float textH = 15.0f;
        const float gap = 4.0f;
        const float circleD = juce::jmin(bounds.getWidth() - 8.0f, bounds.getHeight() - ledD - textH - gap * 2.0f);
        const float stackH = ledD + gap + circleD + gap + textH;
        float y = bounds.getCentreY() - stackH * 0.5f;
        const float cx = bounds.getCentreX();

        paintLed(g, { cx - ledD * 0.5f, y, ledD, ledD }, on);
        y += ledD + gap;

        auto circle = juce::Rectangle<float>(cx - circleD * 0.5f, y, circleD, circleD);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillEllipse(circle.translated(0.0f, 2.0f));
        if (pushed)
            circle = circle.reduced(1.0f).translated(0.0f, 1.0f);

        juce::ColourGradient ring(
            juce::Colour(highlighted ? 0xff3a3a3e : 0xff2a2a2e),
            circle.getCentreX(),
            circle.getY(),
            juce::Colour(0xff050506),
            circle.getCentreX(),
            circle.getBottom(),
            false);
        g.setGradientFill(ring);
        g.fillEllipse(circle);

        auto face = circle.reduced(circleD * 0.11f);
        juce::ColourGradient faceGradient(
            juce::Colour(pushed ? 0xff101012 : 0xff1a1a1d),
            face.getCentreX(),
            face.getY(),
            juce::Colour(0xff080809),
            face.getCentreX(),
            face.getBottom(),
            false);
        g.setGradientFill(faceGradient);
        g.fillEllipse(face);

        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addEllipse(face);
            g.reduceClipRegion(clip);
            paintNoise(g, face, 0.3f);
        }

        g.setColour(juce::Colours::white.withAlpha(pushed ? 0.04f : 0.09f));
        auto sheen = face.reduced(face.getWidth() * 0.16f, face.getHeight() * 0.16f);
        sheen.setHeight(sheen.getHeight() * 0.42f);
        g.fillEllipse(sheen);

        for (int i = 1; i <= 2; ++i) {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.drawEllipse(face.reduced(3.0f + static_cast<float>(i) * 3.5f), 0.8f);
        }

        y += circleD + gap;
        paintCentredCaption(
            g,
            button.getButtonText(),
            { bounds.getX(), y, bounds.getWidth(), textH },
            faceFont(12.0f),
            pushed);
        return;
    }

    drawButtonBackground(g, button, {}, highlighted, pushed);

    auto content = bounds.reduced(8.0f, 2.0f);
    if (pushed)
        content = content.translated(0.0f, 1.0f);

    const float ledD = 8.0f;
    auto font = faceFont(12.0f);
    const float textW = juce::GlyphArrangement::getStringWidth(font, button.getButtonText());
    const float groupW = ledD + 6.0f + textW;
    float x = content.getCentreX() - groupW * 0.5f;
    const float ledY = content.getCentreY() - ledD * 0.5f;
    paintLed(g, { x, ledY, ledD, ledD }, on);
    x += ledD + 6.0f;

    g.setFont(font);
    g.setColour(juce::Colour(kInk));
    g.drawFittedText(
        button.getButtonText(),
        juce::Rectangle<float>(x, content.getY(), textW + 2.0f, content.getHeight()).toNearestInt(),
        juce::Justification::centredLeft,
        1);
}

void RadioLookAndFeel::drawLinearSlider(
    juce::Graphics &g,
    int x,
    int y,
    int width,
    int height,
    float sliderPos,
    float,
    float,
    juce::Slider::SliderStyle style,
    juce::Slider &slider) {
    if (style != juce::Slider::LinearVertical) {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, sliderPos, sliderPos, style, slider);
        return;
    }

    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
    const float capH = static_cast<float>(getSliderThumbRadius(slider) * 2);
    const float capW = juce::jmin(28.0f, bounds.getWidth() - 8.0f);
    const float centreX = bounds.getCentreX();

    auto slot = juce::Rectangle<float>(centreX - 4.0f, bounds.getY() + capH * 0.5f, 8.0f, juce::jmax(0.0f, bounds.getHeight() - capH));
    g.setColour(juce::Colour(0xff000000));
    g.fillRoundedRectangle(slot.expanded(1.5f), 3.0f);
    g.setColour(juce::Colour(kSlot));
    g.fillRoundedRectangle(slot, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawRoundedRectangle(slot, 2.0f, 1.0f);

    g.setColour(juce::Colour(0xff3a3a3e));
    for (int i = 0; i < 5; ++i) {
        const float t = static_cast<float>(i) / 4.0f;
        const float tickY = slot.getY() + t * slot.getHeight();
        g.drawLine(slot.getX() - 7.0f, tickY, slot.getX() - 2.0f, tickY, 1.0f);
        g.drawLine(slot.getRight() + 2.0f, tickY, slot.getRight() + 7.0f, tickY, 1.0f);
    }

    const float capY = juce::jlimit(slot.getY() - capH * 0.5f, slot.getBottom() - capH * 0.5f, sliderPos - capH * 0.5f);
    auto cap = juce::Rectangle<float>(centreX - capW * 0.5f, capY, capW, capH);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillRoundedRectangle(cap.translated(0.0f, 1.5f), 2.5f);

    juce::ColourGradient capGradient(
        juce::Colour(0xff242428),
        cap.getX(),
        cap.getY(),
        juce::Colour(0xff0c0c0e),
        cap.getX(),
        cap.getBottom(),
        false);
    g.setGradientFill(capGradient);
    g.fillRoundedRectangle(cap, 2.5f);

    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(cap, 2.5f);
        g.reduceClipRegion(clip);
        paintNoise(g, cap, 0.35f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.7f));
    for (int i = 0; i < 3; ++i) {
        const float ribY = cap.getY() + cap.getHeight() * (0.32f + 0.18f * static_cast<float>(i));
        g.drawLine(cap.getX() + 3.0f, ribY, cap.getRight() - 3.0f, ribY, 1.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawLine(cap.getX() + 2.0f, cap.getY() + 1.2f, cap.getRight() - 2.0f, cap.getY() + 1.2f, 1.0f);
    g.setColour(juce::Colour(kRim).withAlpha(0.8f));
    g.drawRoundedRectangle(cap, 2.5f, 1.0f);
}

void RadioLookAndFeel::drawRotarySlider(
    juce::Graphics &g,
    int x,
    int y,
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider &) {
    auto bounds = juce::Rectangle<float>(
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(width),
        static_cast<float>(height));
    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 6.0f;
    if (diameter <= 4.0f)
        return;

    auto knob = bounds.withSizeKeepingCentre(diameter, diameter);
    const auto centre = knob.getCentre();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.fillEllipse(knob.translated(0.0f, 2.0f));

    juce::ColourGradient bezel(
        juce::Colour(0xff343438),
        centre.x,
        knob.getY(),
        juce::Colour(0xff050506),
        centre.x,
        knob.getBottom(),
        false);
    g.setGradientFill(bezel);
    g.fillEllipse(knob);

    auto face = knob.reduced(diameter * 0.08f);
    juce::ColourGradient faceGradient(
        juce::Colour(0xff1c1c1f),
        centre.x,
        face.getY(),
        juce::Colour(0xff09090b),
        centre.x,
        face.getBottom(),
        false);
    g.setGradientFill(faceGradient);
    g.fillEllipse(face);

    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addEllipse(face);
        g.reduceClipRegion(clip);
        paintNoise(g, face, 0.32f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.45f));
    for (int i = 1; i <= 3; ++i)
        g.drawEllipse(face.reduced(3.0f + static_cast<float>(i) * (diameter * 0.055f)), 0.9f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    auto sheen = face.reduced(diameter * 0.14f, diameter * 0.14f);
    sheen.setHeight(sheen.getHeight() * 0.4f);
    g.fillEllipse(sheen);

    const float radius = face.getWidth() * 0.5f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-1.6f, -radius * 0.78f, 3.2f, radius * 0.46f, 1.2f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
    g.setColour(juce::Colour(0xfff0f0f2));
    g.fillPath(pointer);
}

int RadioLookAndFeel::getSliderThumbRadius(juce::Slider &slider) {
    if (slider.isVertical())
        return 11;

    return juce::LookAndFeel_V4::getSliderThumbRadius(slider);
}

juce::Font RadioLookAndFeel::getTextButtonFont(juce::TextButton &, int) {
    return faceFont(13.0f);
}

juce::Label *RadioLookAndFeel::createSliderTextBox(juce::Slider &slider) {
    auto *label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setJustificationType(juce::Justification::centred);
    label->setFont(faceFont(12.0f));
    label->setMinimumHorizontalScale(0.6f);
    return label;
}

void RadioLookAndFeel::fillTextEditorBackground(juce::Graphics &g, int width, int height, juce::TextEditor &) {
    g.setColour(juce::Colour(0xff101012));
    g.fillRect(0, 0, width, height);
}

void RadioLookAndFeel::drawTextEditorOutline(juce::Graphics &g, int width, int height, juce::TextEditor &) {
    g.setColour(juce::Colour(kRim));
    g.drawRect(0, 0, width, height);
}
