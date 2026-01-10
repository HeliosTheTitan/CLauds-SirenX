/*
  ==============================================================================
    
    SirenX - Algorithmic Reverb Plugin
    Solar Productions
    
    PluginEditor.cpp - Main UI implementation (Optimized)
    
  ==============================================================================
*/

#include "PluginEditor.h"

//==============================================================================
// Spectrum Display Implementation (Optimized)
//==============================================================================
SpectrumDisplay::SpectrumDisplay(SirenXAudioProcessor& processor)
    : audioProcessor(processor),
      fft(fftOrder), // 2^11 = 2048
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    inputTimeData.resize(fftSize, 0.0f);
    wetTimeData.resize(fftSize, 0.0f);
    inputFrequencyData.resize(fftSize * 2, 0.0f);
    wetFrequencyData.resize(fftSize * 2, 0.0f);
    inputSmoothedSpectrum.resize(numBins, 0.0f);
    wetSmoothedSpectrum.resize(numBins, 0.0f);
    xCoords.resize(numBins, 0.0f);

    // Don't start timer until component is ready
}

SpectrumDisplay::~SpectrumDisplay()
{
    stopTimer();
}

void SpectrumDisplay::setPalette(const SirenXPalette& palette)
{
    currentPalette = palette;
    repaint();
}

void SpectrumDisplay::recalculateXCoords()
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;
    
    double sampleRate = audioProcessor.getSampleRate();
    if (sampleRate <= 0.0) sampleRate = 44100.0;

    for (size_t i = 0; i < numBins && i < xCoords.size(); ++i)
    {
        float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        if (freq < 20.0f)
        {
            xCoords[i] = bounds.getX();
        }
        else if (freq > 20000.0f)
        {
            xCoords[i] = bounds.getRight();
        }
        else
        {
            float normX = std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f);
            xCoords[i] = bounds.getX() + normX * bounds.getWidth();
        }
    }
    
    initialized = true;
}

void SpectrumDisplay::performFFT(const std::vector<float>& timeDomain, std::vector<float>& frequencyDomain)
{
    if (timeDomain.size() < fftSize || frequencyDomain.size() < fftSize * 2)
        return;
        
    std::fill(frequencyDomain.begin(), frequencyDomain.end(), 0.0f);

    for (size_t i = 0; i < fftSize; ++i)
    {
        frequencyDomain[i] = timeDomain[i];
    }

    window.multiplyWithWindowingTable(frequencyDomain.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(frequencyDomain.data());
}

void SpectrumDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    g.setColour(currentPalette.backgroundDark);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    g.setColour(currentPalette.metalBlue.withAlpha(0.3f));

    float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    if (auto* lnf = dynamic_cast<SirenXLookAndFeel*>(&getLookAndFeel()))
        g.setFont(lnf->getConsolasFont(10.0f));
    else
        g.setFont(10.0f);

    for (float f : freqs)
    {
        float normX = std::log10(f / 20.0f) / std::log10(20000.0f / 20.0f);
        float x = bounds.getX() + normX * bounds.getWidth();
        if (x >= bounds.getX() && x <= bounds.getRight())
        {
            g.setColour(currentPalette.metalBlue.withAlpha(0.3f));
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

            g.setColour(currentPalette.textDim);
            juce::String labelText;
            if (f >= 1000.0f) labelText = juce::String(f / 1000.0f, 0) + "k";
            else labelText = juce::String((int)f);

            juce::Justification justification = juce::Justification::centred;
            int xOffset = -15;

            if (f == 20.0f)
            {
                justification = juce::Justification::left;
                xOffset = 2;
            }
            else if (f == 20000.0f)
            {
                justification = juce::Justification::right;
                xOffset = -32;
            }

            g.drawText(labelText, static_cast<int>(x) + xOffset, static_cast<int>(bounds.getBottom()) - 12, 30, 12, justification);
        }
    }

    g.drawHorizontalLine(static_cast<int>(bounds.getCentreY()), bounds.getX(), bounds.getRight());

    juce::Colour inputColor = currentPalette.accentMid;
    juce::Colour wetColor = currentPalette.accentBright;

    drawSpectrum(g, wetSmoothedSpectrum, wetColor, 0.4f);
    drawSpectrum(g, inputSmoothedSpectrum, inputColor, 0.9f);

    // Filter Indicators
    auto& apvts = audioProcessor.getAPVTS();
    float hpFreq = *apvts.getRawParameterValue("lowCut");
    float lpFreq = *apvts.getRawParameterValue("highCut");

    float hpNorm = std::log10(hpFreq / 20.0f) / std::log10(20000.0f / 20.0f);
    float lpNorm = std::log10(lpFreq / 20.0f) / std::log10(20000.0f / 20.0f);

    float hpX = bounds.getX() + hpNorm * bounds.getWidth();
    float lpX = bounds.getX() + lpNorm * bounds.getWidth();

    if (auto* lnf = dynamic_cast<SirenXLookAndFeel*>(&getLookAndFeel()))
        g.setFont(lnf->getConsolasFont(11.0f, true));
    else
        g.setFont(11.0f);

    juce::Colour textColor = currentPalette.textDim;

    if (hpX > bounds.getX())
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(bounds.getX(), bounds.getY(), hpX - bounds.getX(), bounds.getHeight());
        g.setColour(textColor);
        g.drawVerticalLine(static_cast<int>(hpX), bounds.getY(), bounds.getBottom());

        juce::String labelText;
        if (hpFreq >= 1000.0f) labelText = juce::String(hpFreq / 1000.0f, 1) + "k";
        else labelText = juce::String((int)hpFreq);
        g.drawText(labelText, static_cast<int>(hpX) + 5, static_cast<int>(bounds.getBottom()) - 25, 40, 15, juce::Justification::left);
    }

    if (lpX < bounds.getRight())
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(lpX, bounds.getY(), bounds.getRight() - lpX, bounds.getHeight());
        g.setColour(textColor);
        g.drawVerticalLine(static_cast<int>(lpX), bounds.getY(), bounds.getBottom());

        juce::String labelText;
        if (lpFreq >= 1000.0f) labelText = juce::String(lpFreq / 1000.0f, 1) + "k";
        else labelText = juce::String((int)lpFreq);
        g.drawText(labelText, static_cast<int>(lpX) - 45, static_cast<int>(bounds.getBottom()) - 25, 40, 15, juce::Justification::right);
    }

    g.setColour(currentPalette.metalBlueBright.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.0f);
}

void SpectrumDisplay::drawSpectrum(juce::Graphics& g, const std::vector<float>& spectrum, juce::Colour color, float alpha)
{
    if (!initialized || spectrum.size() < numBins || xCoords.size() < numBins) 
        return;

    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;
    
    juce::Path path;
    bool started = false;
    float lastX = -10.0f;

    for (size_t i = 0; i < numBins; ++i)
    {
        float x = xCoords[i];

        if (x < bounds.getX()) continue;
        if (x > bounds.getRight()) break;
        if (std::abs(x - lastX) < 1.0f) continue;
        lastX = x;

        float magnitude = spectrum[i];
        float db = juce::Decibels::gainToDecibels(magnitude + 1e-10f) - juce::Decibels::gainToDecibels(static_cast<float>(fftSize));

        float normY = juce::jmap(db, -100.0f, 0.0f, 0.0f, 1.0f);
        float y = bounds.getBottom() - normY * bounds.getHeight();
        
        if (!started)
        {
            path.startNewSubPath(x, bounds.getBottom());
            path.lineTo(x, y);
            started = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }
    
    if (started)
    {
        path.lineTo(bounds.getRight(), bounds.getBottom());
        path.closeSubPath();

        g.setColour(color.withAlpha(alpha * 0.3f));
        g.fillPath(path);
        g.setColour(color.withAlpha(alpha));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
}

void SpectrumDisplay::resized()
{
    recalculateXCoords();
    
    // Start timer only after first resize (when component has valid bounds)
    if (!isTimerRunning() && getWidth() > 0 && getHeight() > 0)
        startTimerHz(24);
}

void SpectrumDisplay::timerCallback()
{
    if (!initialized) return;
    
    // Safe pull from FIFO
    if (inputTimeData.size() >= fftSize && wetTimeData.size() >= fftSize)
    {
        audioProcessor.audioFifo.pull(inputTimeData, wetTimeData, static_cast<int>(fftSize));
        
        performFFT(inputTimeData, inputFrequencyData);
        performFFT(wetTimeData, wetFrequencyData);
        
        // Apply smoothing with bounds check
        float decay = 0.8f;
        for (size_t i = 0; i < numBins && i < inputSmoothedSpectrum.size() && i < inputFrequencyData.size(); ++i)
        {
            inputSmoothedSpectrum[i] = std::max(inputFrequencyData[i], inputSmoothedSpectrum[i] * decay);
            wetSmoothedSpectrum[i] = std::max(wetFrequencyData[i], wetSmoothedSpectrum[i] * decay);
        }
    }

    repaint();
}

//==============================================================================
// Knob Component Implementation
//==============================================================================
SirenXKnob::SirenXKnob(const juce::String& labelText, const juce::String& suffix)
    : suffixText(suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(slider);
    
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, SirenXColors::textDim);
    addAndMakeVisible(label);
    
    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setColour(juce::Label::textColourId, SirenXColors::textBright);
    addAndMakeVisible(valueLabel);
    
    slider.onValueChange = [this]() { updateValueLabel(); };
}

void SirenXKnob::setAutoMode(bool shouldBeAuto)
{
    isAutoMode = shouldBeAuto;
    updateValueLabel();
}

void SirenXKnob::updateValueLabel()
{
    if (isAutoMode)
    {
        valueLabel.setText("auto", juce::dontSendNotification);
        return;
    }

    juce::String text;
    double value = slider.getValue();

    if (value >= 1000.0)
        text = juce::String(value / 1000.0, 2) + "k";
    else if (value >= 100.0)
        text = juce::String(static_cast<int>(value));
    else if (value >= 10.0)
        text = juce::String(value, 1);
    else
        text = juce::String(value, 2);

    if (suffixText.isNotEmpty())
        text += " " + suffixText;

    valueLabel.setText(text, juce::dontSendNotification);
}

void SirenXKnob::setPalette(const SirenXPalette& palette)
{
    label.setColour(juce::Label::textColourId, palette.textDim);
    valueLabel.setColour(juce::Label::textColourId, palette.textBright);
}

void SirenXKnob::resized()
{
    auto bounds = getLocalBounds();
    
    int labelHeight = 16;
    int valueHeight = 14;

    int knobSize = bounds.getHeight() - labelHeight - valueHeight - 4;
    
    label.setBounds(bounds.removeFromTop(labelHeight));
    
    auto knobBounds = bounds.removeFromTop(knobSize);
    int knobDim = juce::jmin(knobBounds.getWidth(), knobBounds.getHeight());
    slider.setBounds(knobBounds.withSizeKeepingCentre(knobDim, knobDim));
    
    valueLabel.setBounds(bounds);
}

//==============================================================================
// Main Editor Implementation
//==============================================================================
SirenXAudioProcessorEditor::SirenXAudioProcessorEditor(SirenXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lookAndFeel);
    
    spectrumDisplay = std::make_unique<SpectrumDisplay>(audioProcessor);
    addAndMakeVisible(*spectrumDisplay);
    
    addAndMakeVisible(decayKnob);
    addAndMakeVisible(preDelayKnob);
    addAndMakeVisible(sizeKnob);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(widthKnob);
    addAndMakeVisible(highPassKnob);
    addAndMakeVisible(lowPassKnob);
    addAndMakeVisible(duckingKnob);

    // Character buttons setup
    auto setupCharacterButton = [this](juce::TextButton& button, const juce::String& tooltip) {
        button.setClickingTogglesState(false);
        button.setColour(juce::TextButton::buttonColourId, SirenXColors::metalBlue);
        button.setColour(juce::TextButton::buttonOnColourId, SirenXColors::accent);
        button.setColour(juce::TextButton::textColourOffId, SirenXColors::textDim);
        button.setColour(juce::TextButton::textColourOnId, SirenXColors::textBright);
        button.setTooltip(tooltip);
        addAndMakeVisible(button);
    };
    
    setupCharacterButton(plateButton, "Plate: Bright, dense, fast diffusion with metallic shimmer");
    setupCharacterButton(vintageButton, "Vintage: Warm, colored, modulated classic character");
    setupCharacterButton(modernButton, "Modern: Clean, transparent, smooth neutral response");
    
    plateButton.onClick = [this] { 
        audioProcessor.setCharacter(SirenXAudioProcessor::ReverbCharacter::Plate);
        updateCharacterButtons();
    };
    vintageButton.onClick = [this] { 
        audioProcessor.setCharacter(SirenXAudioProcessor::ReverbCharacter::Vintage);
        updateCharacterButtons();
    };
    modernButton.onClick = [this] { 
        audioProcessor.setCharacter(SirenXAudioProcessor::ReverbCharacter::Modern);
        updateCharacterButtons();
    };

    autoButton.setColour(juce::ToggleButton::textColourId, SirenXColors::textDim);
    autoButton.setColour(juce::ToggleButton::tickColourId, SirenXColors::accentBright);
    autoButton.setToggleState(false, juce::dontSendNotification);
    autoButton.onClick = [this] {
        preDelayKnob.setAutoMode(autoButton.getToggleState());
    };
    autoButton.setTooltip("Toggle auto pre-delay sync (simulation)");
    addAndMakeVisible(autoButton);

    tooltipToggle.setColour(juce::ToggleButton::textColourId, SirenXColors::textDim);
    tooltipToggle.setColour(juce::ToggleButton::tickColourId, SirenXColors::accentBright);
    tooltipToggle.setToggleState(true, juce::dontSendNotification);
    tooltipToggle.onClick = [this] {
        if (tooltipToggle.getToggleState())
            tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 700);
        else
            tooltipWindow.reset();
    };
    addAndMakeVisible(tooltipToggle);

    tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 700);
    
    auto& apvts = audioProcessor.getAPVTS();
    
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "decayTime", decayKnob.getSlider());
    preDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "preDelay", preDelayKnob.getSlider());
    sizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "size", sizeKnob.getSlider());
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "mix", mixKnob.getSlider());
    widthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "stereoWidth", widthKnob.getSlider());
    highPassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "lowCut", highPassKnob.getSlider());
    lowPassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "highCut", lowPassKnob.getSlider());
    duckingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "ducking", duckingKnob.getSlider());
    
    decayKnob.getSlider().setTooltip("Controls reverb tail length (0.1s - 30s)");
    preDelayKnob.getSlider().setTooltip("Initial delay before reverb (0 - 500ms)");
    sizeKnob.getSlider().setTooltip("Room size / density of reflections");
    mixKnob.getSlider().setTooltip("Dry/Wet blend");
    widthKnob.getSlider().setTooltip("Stereo width (0% mono, 100% normal, 200% wide)");
    highPassKnob.getSlider().setTooltip("High-pass filter on reverb (cuts low frequencies)");
    lowPassKnob.getSlider().setTooltip("Low-pass filter on reverb (cuts high frequencies)");
    duckingKnob.getSlider().setTooltip("Reduces reverb when input signal is present");
    tooltipToggle.setTooltip("Toggle tooltips on/off");

    // Initialize character button states
    updateCharacterButtons();

    setSize(750, 560); // Slightly taller to fit buttons

    startTimerHz(10);
}

SirenXAudioProcessorEditor::~SirenXAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SirenXAudioProcessorEditor::timerCallback()
{
    // Update character button states periodically in case parameter changed externally
    updateCharacterButtons();
}

void SirenXAudioProcessorEditor::updateCharacterButtons()
{
    auto character = audioProcessor.getCharacter();
    auto palette = lookAndFeel.getPalette();
    
    auto setButtonState = [&palette](juce::TextButton& button, bool isSelected) {
        if (isSelected)
        {
            button.setColour(juce::TextButton::buttonColourId, palette.accentMid);
            button.setColour(juce::TextButton::textColourOffId, palette.textBright);
        }
        else
        {
            button.setColour(juce::TextButton::buttonColourId, palette.metalBlue);
            button.setColour(juce::TextButton::textColourOffId, palette.textDim);
        }
    };
    
    setButtonState(plateButton, character == SirenXAudioProcessor::ReverbCharacter::Plate);
    setButtonState(vintageButton, character == SirenXAudioProcessor::ReverbCharacter::Vintage);
    setButtonState(modernButton, character == SirenXAudioProcessor::ReverbCharacter::Modern);
    
    plateButton.repaint();
    vintageButton.repaint();
    modernButton.repaint();
}

//==============================================================================
void SirenXAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (cachedBackground.isValid())
        g.drawImageAt(cachedBackground, 0, 0);
    else
        drawBackground(g);
}

void SirenXAudioProcessorEditor::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto palette = lookAndFeel.getPalette();
    
    juce::ColourGradient bgGradient(
        palette.backgroundDark,
        bounds.getX(), bounds.getY(),
        palette.backgroundMid,
        bounds.getX(), bounds.getBottom(),
        false);
    g.setGradientFill(bgGradient);
    g.fillRect(bounds);
    
    juce::Random random(42);
    g.setColour(juce::Colours::white.withAlpha(0.015f));
    for (int i = 0; i < 200; ++i)
    {
        float x = random.nextFloat() * bounds.getWidth();
        float y = random.nextFloat() * bounds.getHeight();
        float len = random.nextFloat() * 40.0f + 10.0f;
        g.drawLine(x, y, x + len, y + random.nextFloat() * 2.0f - 1.0f, 0.5f);
    }

    drawSideRails(g);
    drawHeader(g);
    drawFooter(g);
}

void SirenXAudioProcessorEditor::drawHeader(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop(50).toFloat();
    auto palette = lookAndFeel.getPalette();
    
    juce::ColourGradient headerGradient(
        palette.steelDark,
        headerBounds.getX(), headerBounds.getY(),
        palette.steelLight,
        headerBounds.getX(), headerBounds.getBottom(),
        false);
    g.setGradientFill(headerGradient);
    g.fillRect(headerBounds.withLeft(30.0f).withRight(headerBounds.getRight() - 30.0f));
    
    g.setColour(palette.accentBright);
    g.fillRect(headerBounds.withLeft(30.0f).withRight(headerBounds.getRight() - 30.0f)
               .removeFromBottom(2.0f));
    
    juce::Font titleFont = lookAndFeel.getConsolasFont(28.0f, true);
    
    g.setFont(titleFont);
    
    juce::String siren = "SIREN";
    juce::String x = "X";
    
    juce::GlyphArrangement glyphsSiren;
    glyphsSiren.addLineOfText(titleFont, siren, 0.0f, 0.0f);
    float sirenWidth = glyphsSiren.getBoundingBox(0, -1, true).getWidth();
    
    juce::GlyphArrangement glyphsX;
    glyphsX.addLineOfText(titleFont, x, 0.0f, 0.0f);
    float xWidth = glyphsX.getBoundingBox(0, -1, true).getWidth();
    
    float totalWidth = sirenWidth + xWidth;
    float startX = (headerBounds.getWidth() - totalWidth) / 2.0f;
    
    g.setColour(palette.accentBright);
    g.drawText(siren, static_cast<int>(startX), static_cast<int>(headerBounds.getY() + 10),
               static_cast<int>(sirenWidth + 10), 30, juce::Justification::left);
    
    g.setColour(palette.textBright);
    g.drawText(x, static_cast<int>(startX + sirenWidth), static_cast<int>(headerBounds.getY() + 10),
               static_cast<int>(xWidth + 10), 30, juce::Justification::left);
}

void SirenXAudioProcessorEditor::drawFooter(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto footerBounds = bounds.removeFromBottom(35).toFloat();
    auto palette = lookAndFeel.getPalette();
    
    juce::ColourGradient footerGradient(
        palette.steelLight,
        footerBounds.getX(), footerBounds.getY(),
        palette.steelDark,
        footerBounds.getX(), footerBounds.getBottom(),
        false);
    g.setGradientFill(footerGradient);
    g.fillRect(footerBounds.withLeft(30.0f).withRight(footerBounds.getRight() - 30.0f));
    
    g.setColour(palette.accentBright);
    g.fillRect(footerBounds.withLeft(30.0f).withRight(footerBounds.getRight() - 30.0f)
               .removeFromTop(2.0f));
    
    juce::Font footerFont = lookAndFeel.getConsolasFont(12.0f, true);
    g.setFont(footerFont);
    
    juce::String solar = "SOLAR";
    juce::String productions = " PRODUCTIONS";
    
    juce::GlyphArrangement glyphsSolar;
    glyphsSolar.addLineOfText(footerFont, solar, 0.0f, 0.0f);
    float solarWidth = glyphsSolar.getBoundingBox(0, -1, true).getWidth();
    
    juce::GlyphArrangement glyphsProd;
    glyphsProd.addLineOfText(footerFont, productions, 0.0f, 0.0f);
    float prodWidth = glyphsProd.getBoundingBox(0, -1, true).getWidth();
    
    float totalWidth = solarWidth + prodWidth;
    float startX = (footerBounds.getWidth() - totalWidth) / 2.0f;
    
    g.setColour(palette.accentBright);
    g.drawText(solar, static_cast<int>(startX), static_cast<int>(footerBounds.getY() + 8), 
               static_cast<int>(solarWidth + 5), 20, juce::Justification::left);
    
    g.setColour(palette.textBright);
    g.drawText(productions, static_cast<int>(startX + solarWidth), static_cast<int>(footerBounds.getY() + 8), 
               static_cast<int>(prodWidth + 5), 20, juce::Justification::left);
}

void SirenXAudioProcessorEditor::drawSideRails(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    float railWidth = 30.0f;
    auto palette = lookAndFeel.getPalette();
    
    auto leftRail = bounds.removeFromLeft(static_cast<int>(railWidth)).toFloat();
    juce::ColourGradient leftGradient(
        palette.steelLight,
        leftRail.getX(), leftRail.getY(),
        palette.steelDark,
        leftRail.getRight(), leftRail.getY(),
        false);
    g.setGradientFill(leftGradient);
    g.fillRect(leftRail);
    
    bounds = getLocalBounds();
    auto rightRail = bounds.removeFromRight(static_cast<int>(railWidth)).toFloat();
    juce::ColourGradient rightGradient(
        palette.steelDark,
        rightRail.getX(), rightRail.getY(),
        palette.steelLight,
        rightRail.getRight(), rightRail.getY(),
        false);
    g.setGradientFill(rightGradient);
    g.fillRect(rightRail);
    
    float screwSize = 8.0f;
    float screwX = leftRail.getCentreX();
    drawScrew(g, screwX, 70.0f, screwSize);
    drawScrew(g, screwX, getHeight() / 2.0f, screwSize);
    drawScrew(g, screwX, getHeight() - 70.0f, screwSize);
    
    screwX = rightRail.getCentreX();
    drawScrew(g, screwX, 70.0f, screwSize);
    drawScrew(g, screwX, getHeight() / 2.0f, screwSize);
    drawScrew(g, screwX, getHeight() - 70.0f, screwSize);
}

void SirenXAudioProcessorEditor::drawScrew(juce::Graphics& g, float x, float y, float size)
{
    auto palette = lookAndFeel.getPalette();

    g.setColour(palette.steelDark);
    g.fillEllipse(x - size / 2.0f, y - size / 2.0f, size, size);
    
    g.setColour(palette.screwHighlight);
    g.fillEllipse(x - size / 2.0f + 1.0f, y - size / 2.0f + 1.0f, size * 0.6f, size * 0.6f);
    
    g.setColour(palette.backgroundDark);
    float slotWidth = size * 0.15f;
    float slotLength = size * 0.7f;
    g.fillRect(x - slotLength / 2.0f, y - slotWidth / 2.0f, slotLength, slotWidth);
    g.fillRect(x - slotWidth / 2.0f, y - slotLength / 2.0f, slotWidth, slotLength);
}

void SirenXAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop(50);

    tooltipToggle.setBounds(headerBounds.removeFromLeft(120).withTrimmedLeft(35).reduced(0, 15).withWidth(80));

    if (getWidth() > 0 && getHeight() > 0)
    {
        cachedBackground = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics g(cachedBackground);
        drawBackground(g);
    }
    
    auto railLeft = bounds.removeFromLeft(30);
    bounds.removeFromRight(30);
    bounds.removeFromBottom(35);
    
    bounds.reduce(15, 10);
    auto faceplateArea = bounds;
    
    spectrumDisplay->setBounds(faceplateArea.removeFromTop(170).reduced(0, 5));
    
    faceplateArea.removeFromTop(20);
    
    auto mainControls = faceplateArea;
    int knobHeight = 85;

    // Use FlexBox for even spacing of knobs
    auto layoutKnobRow = [&](juce::Rectangle<int> area, std::initializer_list<juce::Component*> knobs) {
        juce::FlexBox flex;
        flex.flexDirection = juce::FlexBox::Direction::row;
        flex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
        flex.alignContent = juce::FlexBox::AlignContent::stretch;

        for (auto* knob : knobs)
            flex.items.add(juce::FlexItem(*knob).withFlex(1.0f).withMargin({ 0.0f, 5.0f, 0.0f, 5.0f }));

        flex.performLayout(area);
    };

    auto row1 = mainControls.removeFromTop(knobHeight);
    layoutKnobRow(row1, { &decayKnob, &preDelayKnob, &sizeKnob, &mixKnob });

    mainControls.removeFromTop(15);

    auto row2 = mainControls.removeFromTop(knobHeight);
    layoutKnobRow(row2, { &widthKnob, &highPassKnob, &lowPassKnob, &duckingKnob });

    // Character buttons row
    mainControls.removeFromTop(15);
    auto buttonRow = mainControls.removeFromTop(28);
    
    juce::FlexBox buttonFlex;
    buttonFlex.flexDirection = juce::FlexBox::Direction::row;
    buttonFlex.justifyContent = juce::FlexBox::JustifyContent::center;
    buttonFlex.alignContent = juce::FlexBox::AlignContent::center;

    float buttonW = 100.0f;
    float buttonH = 28.0f;
    float margin = 5.0f;

    buttonFlex.items.add(juce::FlexItem(plateButton).withWidth(buttonW).withHeight(buttonH).withMargin({0, margin, 0, margin}));
    buttonFlex.items.add(juce::FlexItem(vintageButton).withWidth(buttonW).withHeight(buttonH).withMargin({0, margin, 0, margin}));
    buttonFlex.items.add(juce::FlexItem(modernButton).withWidth(buttonW).withHeight(buttonH).withMargin({0, margin, 0, margin}));

    // Add Auto button beside Modern button (MB Mode)
    buttonFlex.items.add(juce::FlexItem(autoButton).withWidth(60.0f).withHeight(buttonH).withMargin({0, margin + 10.0f, 0, margin}));

    buttonFlex.performLayout(buttonRow);
}
