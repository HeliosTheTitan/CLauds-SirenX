/*
  ==============================================================================
    
    SirenX - Algorithmic Reverb Plugin
    Solar Productions
    
    CustomLookAndFeel.cpp - UI styling implementation
    
  ==============================================================================
*/

#include "CustomLookAndFeel.h"

//==============================================================================
SirenXLookAndFeel::SirenXLookAndFeel()
{
    setPalette(SirenXPalette::getNeonBlue());
}

void SirenXLookAndFeel::setPalette(const SirenXPalette& newPalette)
{
    palette = newPalette;

    setColour(juce::ComboBox::backgroundColourId, palette.backgroundMid);
    setColour(juce::ComboBox::textColourId, palette.textBright);
    setColour(juce::ComboBox::arrowColourId, palette.accentMid);
    setColour(juce::ComboBox::outlineColourId, palette.metalBlueBright);
    
    setColour(juce::PopupMenu::backgroundColourId, palette.backgroundDark);
    setColour(juce::PopupMenu::textColourId, palette.textBright);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette.accentMid);
    setColour(juce::PopupMenu::highlightedTextColourId, palette.backgroundDark);
    
    setColour(juce::Label::textColourId, palette.textBright);
    
    setColour(juce::Slider::textBoxTextColourId, palette.textBright);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

//==============================================================================
juce::Font SirenXLookAndFeel::getConsolasFont(float height, bool bold)
{
    // Compatible with older JUCE versions (pre-FontOptions)
    juce::Font font("Consolas", height, bold ? juce::Font::bold : juce::Font::plain);
    font.setExtraKerningFactor(0.15f);
    return font;
}

juce::Font SirenXLookAndFeel::getLabelFont(juce::Label& label)
{
    return getConsolasFont(static_cast<float>(label.getFont().getHeight()), false);
}

//==============================================================================
void SirenXLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider& slider)
{
    bool isAuto = slider.getProperties().getWithDefault("isAuto", false);

    if (isAuto)
        sliderPosProportional = 1.0f;

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    auto center = bounds.getCentre();
    float cx = center.x;
    float cy = center.y;
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 8.0f;
    
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    
    juce::Colour accentColor = palette.getInterpolatedAccent(sliderPosProportional);
    
    // Outer Glow
    if (sliderPosProportional > 0.01f)
    {
        float glowRadius = radius + 8.0f;
        juce::ColourGradient glowGrad(
            accentColor.withAlpha(0.15f * sliderPosProportional),
            cx, cy,
            accentColor.withAlpha(0.0f),
            cx, cy - glowRadius, true);
        g.setGradientFill(glowGrad);
        g.fillEllipse(cx - glowRadius, cy - glowRadius, glowRadius * 2, glowRadius * 2);
    }
    
    // Outer Ring Shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(cx - radius - 2, cy - radius - 2, (radius + 2) * 2, (radius + 2) * 2);
    
    // Outer Metallic Ring
    float outerRadius = radius;
    juce::ColourGradient outerRingGrad(
        palette.steelLight, cx, cy - outerRadius,
        palette.steelDark, cx, cy + outerRadius, false);
    g.setGradientFill(outerRingGrad);
    g.fillEllipse(cx - outerRadius, cy - outerRadius, outerRadius * 2, outerRadius * 2);
    
    // Ring Highlight
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawEllipse(cx - outerRadius + 1, cy - outerRadius + 1, 
                  (outerRadius - 1) * 2, (outerRadius - 1) * 2, 1.0f);
    
    // Value Arc Background
    float arcRadius = radius * 0.82f;
    float arcThickness = 4.0f;
    
    juce::Path arcBg;
    arcBg.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(palette.backgroundDark.withAlpha(0.8f));
    g.strokePath(arcBg, juce::PathStrokeType(arcThickness + 2.0f, 
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(palette.metalBlue.withAlpha(0.4f));
    g.strokePath(arcBg, juce::PathStrokeType(arcThickness, 
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    
    // Value Arc
    if (sliderPosProportional > 0.01f)
    {
        juce::Path arcValue;
        arcValue.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, angle, true);
        
        juce::ColourGradient arcGrad(
            palette.accentMid,
            cx + arcRadius * std::sin(rotaryStartAngle), 
            cy - arcRadius * std::cos(rotaryStartAngle),
            accentColor,
            cx + arcRadius * std::sin(angle),
            cy - arcRadius * std::cos(angle), false);
        
        g.setGradientFill(arcGrad);
        g.strokePath(arcValue, juce::PathStrokeType(arcThickness, 
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        g.setColour(accentColor.withAlpha(0.3f));
        g.strokePath(arcValue, juce::PathStrokeType(arcThickness + 4.0f, 
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    
    // Inner Knob Body
    float innerRadius = radius * 0.65f;
    
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(cx - innerRadius + 2, cy - innerRadius + 2, innerRadius * 2, innerRadius * 2);
    
    juce::ColourGradient bodyGrad(
        SirenXColors::metalBlueBright, cx, cy - innerRadius,
        SirenXColors::backgroundDark, cx, cy + innerRadius, false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(cx - innerRadius, cy - innerRadius, innerRadius * 2, innerRadius * 2);
    
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawEllipse(cx - innerRadius, cy - innerRadius, innerRadius * 2, innerRadius * 2, 1.0f);
    
    // Concentric rings
    for (int i = 1; i <= 3; ++i)
    {
        float ringRadius = innerRadius * (0.4f + i * 0.15f);
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.drawEllipse(cx - ringRadius, cy - ringRadius, ringRadius * 2, ringRadius * 2, 0.5f);
    }
    
    // Center Cap
    float capRadius = innerRadius * 0.35f;
    
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(cx - capRadius + 1, cy - capRadius + 1, capRadius * 2, capRadius * 2);
    
    juce::ColourGradient capGrad(
        SirenXColors::metalBlue, cx, cy - capRadius,
        SirenXColors::backgroundDark, cx, cy + capRadius, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - capRadius, cy - capRadius, capRadius * 2, capRadius * 2);
    
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillEllipse(cx - capRadius * 0.6f, cy - capRadius * 0.8f, capRadius * 1.2f, capRadius * 0.5f);
    
    // Pointer
    juce::Path pointer;
    float pointerLength = innerRadius * 0.55f;
    float pointerWidth = 3.5f;
    pointer.addRoundedRectangle(-pointerWidth * 0.5f, -innerRadius + 4.0f, 
                                 pointerWidth, pointerLength, 1.5f);
    
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(cx + 1, cy + 1));
    
    juce::ColourGradient pointerGrad(
        accentColor, 0, -innerRadius + 4.0f,
        accentColor.darker(0.3f), 0, -innerRadius + 4.0f + pointerLength, false);
    g.setGradientFill(pointerGrad);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(cx, cy));
    
    g.setColour(accentColor.withAlpha(0.4f));
    g.strokePath(pointer, juce::PathStrokeType(1.5f),
                 juce::AffineTransform::rotation(angle).translated(cx, cy));
}

//==============================================================================
void SirenXLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                         const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    bool isHorizontal = style == juce::Slider::LinearHorizontal;
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    
    float trackThickness = 6.0f;
    float thumbSize = 14.0f;
    
    float normalizedValue = (sliderPos - (isHorizontal ? static_cast<float>(x) : static_cast<float>(y + height))) /
                            (isHorizontal ? static_cast<float>(width) : static_cast<float>(-height));
    normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
    
    juce::Colour accentColor = palette.getInterpolatedAccent(normalizedValue);
    
    juce::Rectangle<float> track;
    if (isHorizontal)
        track = bounds.withSizeKeepingCentre(bounds.getWidth() - thumbSize, trackThickness);
    else
        track = bounds.withSizeKeepingCentre(trackThickness, bounds.getHeight() - thumbSize);
    
    g.setColour(palette.backgroundDark);
    g.fillRoundedRectangle(track, trackThickness * 0.5f);
    g.setColour(palette.metalBlue.withAlpha(0.5f));
    g.drawRoundedRectangle(track, trackThickness * 0.5f, 1.0f);
    
    juce::Rectangle<float> filledTrack;
    if (isHorizontal)
        filledTrack = track.withWidth(track.getWidth() * normalizedValue);
    else
    {
        float filledHeight = track.getHeight() * normalizedValue;
        filledTrack = track.withY(track.getBottom() - filledHeight).withHeight(filledHeight);
    }
    
    if (normalizedValue > 0.01f)
    {
        g.setColour(accentColor);
        g.fillRoundedRectangle(filledTrack, trackThickness * 0.5f);
        
        g.setColour(accentColor.withAlpha(0.3f));
        g.fillRoundedRectangle(filledTrack.expanded(2.0f), trackThickness * 0.5f + 2.0f);
    }
    
    float thumbX = isHorizontal ? sliderPos - thumbSize * 0.5f : bounds.getCentreX() - thumbSize * 0.5f;
    float thumbY = isHorizontal ? bounds.getCentreY() - thumbSize * 0.5f : sliderPos - thumbSize * 0.5f;
    
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(thumbX + 1, thumbY + 1, thumbSize, thumbSize);
    
    juce::ColourGradient thumbGrad(
        palette.metalBlueBright, thumbX, thumbY,
        palette.backgroundDark, thumbX, thumbY + thumbSize, false);
    g.setGradientFill(thumbGrad);
    g.fillEllipse(thumbX, thumbY, thumbSize, thumbSize);
    
    g.setColour(accentColor);
    g.drawEllipse(thumbX, thumbY, thumbSize, thumbSize, 2.0f);
    
    juce::ignoreUnused(slider);
}

//==============================================================================
void SirenXLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(4.0f);
    float cornerSize = 4.0f;
    
    bool isOn = button.getToggleState();
    juce::Colour fillColor = isOn ? palette.accentMid : palette.metalBlue;
    
    if (shouldDrawButtonAsHighlighted)
        fillColor = fillColor.brighter(0.1f);
    
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.translated(1, 1), cornerSize);
    
    juce::ColourGradient buttonGrad(
        fillColor.brighter(0.1f), bounds.getX(), bounds.getY(),
        fillColor.darker(0.2f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(buttonGrad);
    g.fillRoundedRectangle(bounds, cornerSize);
    
    g.setColour(isOn ? palette.accentBright.withAlpha(0.5f) : palette.textDim.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    
    if (isOn)
    {
        g.setColour(palette.glowColor.withAlpha(0.2f));
        g.drawRoundedRectangle(bounds.expanded(2.0f), cornerSize + 2.0f, 2.0f);
    }

    float pulseAlpha = button.getProperties().getWithDefault("pulseAlpha", 0.0f);
    if (pulseAlpha > 0.01f)
    {
        float expansion = 5.0f * pulseAlpha;
        g.setColour(palette.accentBright.withAlpha(pulseAlpha * 0.6f));
        g.drawRoundedRectangle(bounds.expanded(2.0f + expansion), cornerSize + 2.0f + expansion, 2.0f);

        g.setColour(palette.accentHighlight.withAlpha(pulseAlpha * 0.3f));
        g.fillRoundedRectangle(bounds.expanded(2.0f + expansion), cornerSize + 2.0f + expansion);
    }
    
    g.setColour(isOn ? palette.textBright : palette.textDim);
    g.setFont(getConsolasFont(12.0f, true));
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred);
}

//==============================================================================
void SirenXLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                     bool /*isButtonDown*/, int /*buttonX*/, int /*buttonY*/,
                                     int /*buttonW*/, int /*buttonH*/, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
    float cornerSize = 4.0f;
    
    g.setColour(palette.backgroundMid);
    g.fillRoundedRectangle(bounds, cornerSize);
    
    g.setColour(palette.metalBlueBright);
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);
    
    juce::Path arrow;
    float arrowX = static_cast<float>(width) - 18.0f;
    float arrowY = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(arrowX, arrowY - 3.0f, arrowX + 8.0f, arrowY - 3.0f, arrowX + 4.0f, arrowY + 3.0f);
    g.setColour(palette.accentMid);
    g.fillPath(arrow);
    
    juce::ignoreUnused(box);
}

void SirenXLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool /*isSeparator*/, bool /*isActive*/, bool isHighlighted,
                                          bool isTicked, bool /*hasSubMenu*/,
                                          const juce::String& text, const juce::String& /*shortcutKeyText*/,
                                          const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    auto bounds = area.toFloat().reduced(2.0f);
    
    if (isHighlighted)
    {
        g.setColour(palette.accentMid);
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(palette.backgroundDark);
    }
    else
    {
        g.setColour(palette.textBright);
    }
    
    g.setFont(getConsolasFont(13.0f, false));
    g.drawText(text, bounds.reduced(8.0f, 0.0f), juce::Justification::centredLeft);
    
    if (isTicked)
    {
        auto tickBounds = bounds.removeFromRight(bounds.getHeight()).reduced(6.0f);
        g.setColour(isHighlighted ? palette.backgroundDark : palette.accentBright);
        g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x93"), tickBounds, juce::Justification::centred);
    }
}

//==============================================================================
juce::PopupMenu::Options SirenXLookAndFeel::getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label& label)
{
    // Configure popup menu options
    return juce::PopupMenu::Options().withTargetComponent(&box)
                                     .withMinimumNumColumns(1)
                                     .withMaximumNumColumns(1)
                                     .withStandardItemHeight(label.getHeight());
}

//==============================================================================
juce::Rectangle<int> SirenXLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                                         juce::Rectangle<int> parentArea)
{
    juce::ignoreUnused(screenPos, parentArea);

    juce::Font font = getConsolasFont(14.0f);
    juce::GlyphArrangement ga;
    ga.addLineOfText(font, tipText, 0.0f, 0.0f);

    int w = static_cast<int>(std::ceil(ga.getBoundingBox(0, -1, true).getWidth())) + 20;
    int h = 30;
    return juce::Rectangle<int>(w, h);
}

void SirenXLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);

    g.setColour(palette.backgroundDark.withAlpha(0.95f));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(palette.accentMid);
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.5f);

    g.setColour(palette.textBright);
    g.setFont(getConsolasFont(14.0f));
    g.drawText(text, bounds, juce::Justification::centred);
}

//==============================================================================
void SirenXLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getConsolasFont(static_cast<float>(label.getFont().getHeight()), false));
    
    auto textArea = label.getLocalBounds().toFloat();
    g.drawText(label.getText(), textArea, label.getJustificationType(), false);
}

//==============================================================================
void SirenXLookAndFeel::drawKnobGlow(juce::Graphics& g, juce::Rectangle<float> bounds,
                                     float normalizedValue, float glowIntensity)
{
    if (normalizedValue < 0.01f || glowIntensity < 0.01f)
        return;
    
    juce::Colour accentColor = palette.getInterpolatedAccent(normalizedValue);
    float glowRadius = bounds.getWidth() * 0.6f;
    
    juce::ColourGradient glow(
        accentColor.withAlpha(0.3f * glowIntensity * normalizedValue),
        bounds.getCentreX(), bounds.getCentreY(),
        accentColor.withAlpha(0.0f),
        bounds.getCentreX(), bounds.getCentreY() - glowRadius, true);
    
    g.setGradientFill(glow);
    g.fillEllipse(bounds.expanded(glowRadius * 0.5f));
}
