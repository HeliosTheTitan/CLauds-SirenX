/*
  ==============================================================================
    
    SirenX - Algorithmic Reverb Plugin
    Solar Productions
    
    CustomLookAndFeel.h - Neon Blue Theme
    
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
// SirenX Color Palette
//==============================================================================
struct SirenXPalette
{
    juce::Colour backgroundDark;
    juce::Colour backgroundMid;
    juce::Colour backgroundLight;
    juce::Colour metalBlue;
    juce::Colour metalBlueBright;

    juce::Colour accentDark;
    juce::Colour accentMid;
    juce::Colour accentBright;
    juce::Colour accentHighlight;

    juce::Colour textBright;
    juce::Colour textDim;

    juce::Colour steelDark;
    juce::Colour steelMid;
    juce::Colour steelLight;
    juce::Colour screwHighlight;

    juce::Colour glowColor;

    // Neon Blue
    static SirenXPalette getNeonBlue()
    {
        return {
            juce::Colour(0xFF050510), juce::Colour(0xFF0F1224), juce::Colour(0xFF1A2235),
            juce::Colour(0xFF203050), juce::Colour(0xFF304060),

            juce::Colour(0xFF004080), juce::Colour(0xFF0080FF), juce::Colour(0xFF40A0FF), juce::Colour(0xFF80C0FF),

            juce::Colour(0xFFE0F0FF), juce::Colour(0xFF8090A0),

            juce::Colour(0xFF101520), juce::Colour(0xFF202530), juce::Colour(0xFF303540), juce::Colour(0xFF404550),
            juce::Colour(0x600080FF)
        };
    }

    juce::Colour getInterpolatedAccent(float normalizedValue) const
    {
        if (normalizedValue < 0.33f)
            return accentDark.interpolatedWith(accentMid, normalizedValue / 0.33f);
        else if (normalizedValue < 0.66f)
            return accentMid.interpolatedWith(accentBright, (normalizedValue - 0.33f) / 0.33f);
        else
            return accentBright.interpolatedWith(accentHighlight, (normalizedValue - 0.66f) / 0.34f);
    }
};

namespace SirenXColors
{
    static const auto backgroundDark = SirenXPalette::getNeonBlue().backgroundDark;
    static const auto backgroundMid = SirenXPalette::getNeonBlue().backgroundMid;
    static const auto backgroundLight = SirenXPalette::getNeonBlue().backgroundLight;
    static const auto metalBlue = SirenXPalette::getNeonBlue().metalBlue;
    static const auto metalBlueBright = SirenXPalette::getNeonBlue().metalBlueBright;

    static const auto accentDark = SirenXPalette::getNeonBlue().accentDark;
    static const auto accent = SirenXPalette::getNeonBlue().accentMid;
    static const auto accentBright = SirenXPalette::getNeonBlue().accentBright;
    static const auto accentHighlight = SirenXPalette::getNeonBlue().accentHighlight;

    static const auto textBright = SirenXPalette::getNeonBlue().textBright;
    static const auto textDim = SirenXPalette::getNeonBlue().textDim;

    static const auto steelDark = SirenXPalette::getNeonBlue().steelDark;
    static const auto steelMid = SirenXPalette::getNeonBlue().steelMid;
    static const auto steelLight = SirenXPalette::getNeonBlue().steelLight;
    static const auto screwHighlight = SirenXPalette::getNeonBlue().screwHighlight;

    static const auto glowColor = SirenXPalette::getNeonBlue().glowColor;
}

//==============================================================================
// Custom LookAndFeel for SirenX
//==============================================================================
class SirenXLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SirenXLookAndFeel();
    ~SirenXLookAndFeel() override = default;
    
    void setPalette(const SirenXPalette& newPalette);
    const SirenXPalette& getPalette() const { return palette; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, 
                          bool shouldDrawButtonAsDown) override;
    
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;
    
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label& label) override;

    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;
    
    void drawLabel(juce::Graphics& g, juce::Label& label) override;
    
    juce::Font getLabelFont(juce::Label& label) override;
    
    juce::Font getConsolasFont(float height, bool bold = false);
    
private:
    void drawKnobGlow(juce::Graphics& g, juce::Rectangle<float> bounds, 
                      float normalizedValue, float glowIntensity);

    SirenXPalette palette = SirenXPalette::getNeonBlue();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SirenXLookAndFeel)
};
