/*
  ==============================================================================
    
    SirenX - Convolution Reverb Plugin
    Solar Productions
    
    PluginEditor.h - Main UI with spectrum display
    
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

//==============================================================================
// Spectrum Display Component
//==============================================================================
class SpectrumDisplay : public juce::Component, public juce::Timer
{
public:
    SpectrumDisplay(SirenXAudioProcessor& processor);
    ~SpectrumDisplay() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setPalette(const SirenXPalette& palette);
    
private:
    void drawSpectrum(juce::Graphics& g, const std::vector<float>& spectrum, juce::Colour color, float alpha);
    void performFFT(const std::vector<float>& timeDomain, std::vector<float>& frequencyDomain);

    SirenXAudioProcessor& audioProcessor;
    SirenXPalette currentPalette = SirenXPalette::getNeonBlue();
    
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::vector<float> inputTimeData;
    std::vector<float> wetTimeData;
    std::vector<float> inputFrequencyData;
    std::vector<float> wetFrequencyData;
    std::vector<float> inputSmoothedSpectrum;
    std::vector<float> wetSmoothedSpectrum;

    std::vector<float> xCoords;
    void recalculateXCoords();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumDisplay)
};

//==============================================================================
// Custom Knob Component with Label
//==============================================================================
class SirenXKnob : public juce::Component
{
public:
    SirenXKnob(const juce::String& labelText, const juce::String& suffix = "");
    
    void resized() override;
    
    juce::Slider& getSlider() { return slider; }
    
    void setLabelText(const juce::String& text) { label.setText(text, juce::dontSendNotification); }
    
    void setPalette(const SirenXPalette& palette);

private:
    juce::Slider slider;
    juce::Label label;
    juce::Label valueLabel;
    juce::String suffixText;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SirenXKnob)
};

//==============================================================================
// Main Editor
//==============================================================================
class SirenXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Timer
{
public:
    SirenXAudioProcessorEditor(SirenXAudioProcessor&);
    ~SirenXAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void drawBackground(juce::Graphics& g);
    void drawHeader(juce::Graphics& g);
    void drawFooter(juce::Graphics& g);
    void drawSideRails(juce::Graphics& g);
    void drawScrew(juce::Graphics& g, float x, float y, float size);
    
    SirenXAudioProcessor& audioProcessor;
    SirenXLookAndFeel lookAndFeel;
    
    // Spectrum display
    std::unique_ptr<SpectrumDisplay> spectrumDisplay;
    
    // Controls
    SirenXKnob decayKnob      { "DECAY", "s" };
    SirenXKnob preDelayKnob   { "PRE-DELAY", "ms" };
    SirenXKnob sizeKnob       { "SIZE", "%" };
    SirenXKnob mixKnob        { "MIX", "%" };
    
    SirenXKnob widthKnob      { "WIDTH", "%" };
    SirenXKnob highCutKnob    { "HIGH CUT", "Hz" };
    SirenXKnob lowCutKnob     { "LOW CUT", "Hz" };
    SirenXKnob duckingKnob    { "DUCKING", "%" };

    juce::ComboBox modeSelector;
    juce::ComboBox presetSelector;

    juce::ToggleButton tooltipToggle { "HINTS" };
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    
    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preDelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highCutAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowCutAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> duckingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;
    
    juce::Image cachedBackground;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SirenXAudioProcessorEditor)
};
