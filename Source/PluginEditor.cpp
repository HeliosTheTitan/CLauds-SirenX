/*
  ==============================================================================
    
    SirenX - Convolution Reverb Plugin
    Solar Productions
    
    PluginEditor.cpp - Main UI implementation
    
  ==============================================================================
*/

#include "PluginEditor.h"

//==============================================================================
// Spectrum Display Implementation
//==============================================================================
SpectrumDisplay::SpectrumDisplay(SirenXAudioProcessor& processor)
    : audioProcessor(processor),
      fft(12), // 2^12 = 4096
      window(4096, juce::dsp::WindowingFunction<float>::hann)
{
    inputTimeData.resize(4096);
    wetTimeData.resize(4096);
    inputFrequencyData.resize(8192); // FFT size * 2
    wetFrequencyData.resize(8192);
    inputSmoothedSpectrum.resize(2048, 0.0f); // Nyquist bins
    wetSmoothedSpectrum.resize(2048, 0.0f);
    xCoords.resize(2048, 0.0f);

    startTimerHz(30);
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
    double sampleRate = audioProcessor.getSampleRate();
    if (sampleRate <= 0.0) sampleRate = 44100.0;

    // Pre-calculate X coordinates for all bins
    for (size_t i = 0; i < xCoords.size(); ++i)
    {
        float freq = (float)i * (float)sampleRate / 4096.0f;
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
}

void SpectrumDisplay::performFFT(const std::vector<float>& timeDomain, std::vector<float>& frequencyDomain)
{
    std::fill(frequencyDomain.begin(), frequencyDomain.end(), 0.0f);

    // Copy time domain data to frequency domain buffer for processing
    // We use 4096 samples
    for (size_t i = 0; i < timeDomain.size() && i < 4096; ++i)
    {
        frequencyDomain[i] = timeDomain[i];
    }

    // Apply windowing
    window.multiplyWithWindowingTable(frequencyDomain.data(), 4096);

    // Perform FFT
    fft.performFrequencyOnlyForwardTransform(frequencyDomain.data());
}

void SpectrumDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    g.setColour(currentPalette.backgroundDark);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Grid lines
    g.setColour(currentPalette.metalBlue.withAlpha(0.3f));

    // Vertical log-scale grid
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

            // Frequency label
            g.setColour(currentPalette.textDim);
            juce::String labelText;
            if (f >= 1000.0f) labelText = juce::String(f / 1000.0f, 0) + "k";
            else labelText = juce::String((int)f);

            // Adjust justification to prevent cutoff
            juce::Justification justification = juce::Justification::centred;
            int xOffset = -15;

            if (f == 20.0f)
            {
                justification = juce::Justification::left;
                xOffset = 2; // Slight padding
            }
            else if (f == 20000.0f)
            {
                justification = juce::Justification::right;
                xOffset = -32;
            }

            g.drawText(labelText, static_cast<int>(x) + xOffset, static_cast<int>(bounds.getBottom()) - 12, 30, 12, justification);
        }
    }

    // Horizontal dB grid (optional, simplistic)
    g.drawHorizontalLine(static_cast<int>(bounds.getCentreY()), bounds.getX(), bounds.getRight());

    // Draw Spectrums
    // Use dynamic colors from current palette
    juce::Colour inputColor = currentPalette.accentMid;
    juce::Colour wetColor = currentPalette.accentBright;

    // Draw Wet (Ghost) first
    drawSpectrum(g, wetSmoothedSpectrum, wetColor, 0.4f);

    // Draw Input (Solid)
    drawSpectrum(g, inputSmoothedSpectrum, inputColor, 0.9f);

    // === Filter Indicators ===
    auto& apvts = audioProcessor.getAPVTS();
    float hpFreq = *apvts.getRawParameterValue("lowCut"); // Low Cut = High Pass
    float lpFreq = *apvts.getRawParameterValue("highCut"); // High Cut = Low Pass

    float hpNorm = std::log10(hpFreq / 20.0f) / std::log10(20000.0f / 20.0f);
    float lpNorm = std::log10(lpFreq / 20.0f) / std::log10(20000.0f / 20.0f);

    float hpX = bounds.getX() + hpNorm * bounds.getWidth();
    float lpX = bounds.getX() + lpNorm * bounds.getWidth();

    if (auto* lnf = dynamic_cast<SirenXLookAndFeel*>(&getLookAndFeel()))
        g.setFont(lnf->getConsolasFont(11.0f, true));
    else
        g.setFont(11.0f);

    juce::Colour textColor = currentPalette.textDim;

    // Shade Low Cut Area (High Pass)
    if (hpX > bounds.getX())
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(bounds.getX(), bounds.getY(), hpX - bounds.getX(), bounds.getHeight());
        g.setColour(textColor);
        g.drawVerticalLine(static_cast<int>(hpX), bounds.getY(), bounds.getBottom());

        // HP Frequency Label
        juce::String labelText;
        if (hpFreq >= 1000.0f) labelText = juce::String(hpFreq / 1000.0f, 1) + "k";
        else labelText = juce::String((int)hpFreq);
        g.drawText(labelText, static_cast<int>(hpX) + 5, static_cast<int>(bounds.getBottom()) - 25, 40, 15, juce::Justification::left);
    }

    // Shade High Cut Area (Low Pass)
    if (lpX < bounds.getRight())
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(lpX, bounds.getY(), bounds.getRight() - lpX, bounds.getHeight());
        g.setColour(textColor);
        g.drawVerticalLine(static_cast<int>(lpX), bounds.getY(), bounds.getBottom());

        // LP Frequency Label
        juce::String labelText;
        if (lpFreq >= 1000.0f) labelText = juce::String(lpFreq / 1000.0f, 1) + "k";
        else labelText = juce::String((int)lpFreq);
        g.drawText(labelText, static_cast<int>(lpX) - 45, static_cast<int>(bounds.getBottom()) - 25, 40, 15, juce::Justification::right);
    }

    // Border
    g.setColour(currentPalette.metalBlueBright.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.0f);

    // === Ducking Indicator ===
    // Draw a small meter in the top right corner
    float duckingGain = audioProcessor.getDuckingReduction(); // 1.0 = no reduction, 0.0 = full reduction
    if (duckingGain < 0.99f)
    {
        float indicatorSize = 12.0f;
        auto indicatorArea = bounds.reduced(10.0f).removeFromTop(indicatorSize).removeFromRight(100.0f);

        // Label
        g.setColour(currentPalette.textDim);
        g.setFont(10.0f);
        g.drawText("DUCKING", indicatorArea.removeFromLeft(50), juce::Justification::centredRight);

        indicatorArea.removeFromLeft(5); // Gap

        // Meter Bar
        g.setColour(currentPalette.backgroundDark.darker());
        g.fillRoundedRectangle(indicatorArea, 2.0f);

        // Filled part (inverse, showing reduction amount)
        float reduction = 1.0f - duckingGain; // 0.0 to 1.0
        auto fillArea = indicatorArea.removeFromLeft(indicatorArea.getWidth() * reduction);

        g.setColour(currentPalette.accentBright);
        g.fillRoundedRectangle(fillArea, 2.0f);

        // Glow
        g.setColour(currentPalette.accentBright.withAlpha(0.4f));
        g.fillRoundedRectangle(fillArea.expanded(1.0f), 3.0f);
    }
}

void SpectrumDisplay::drawSpectrum(juce::Graphics& g, const std::vector<float>& spectrum, juce::Colour color, float alpha)
{
    if (spectrum.empty() || xCoords.size() != 2048) return;

    auto bounds = getLocalBounds().toFloat();
    juce::Path path;
    bool started = false;
    float lastX = -10.0f;

    // FFT size is 4096, output is 2048 bins (Frequency only)
    int numBins = 2048;

    for (int i = 0; i < numBins; ++i)
    {
        float x = xCoords[i];

        // Skip if outside bounds or if this pixel column already has a point (decimation)
        if (x < bounds.getX()) continue;
        if (x > bounds.getRight()) break;
        if (std::abs(x - lastX) < 0.5f) continue;
        lastX = x;

        float magnitude = spectrum[static_cast<size_t>(i)];
        // Convert to dB
        float db = juce::Decibels::gainToDecibels(magnitude) - juce::Decibels::gainToDecibels((float)4096);

        // Scale to height: range -100dB to 0dB
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
}

void SpectrumDisplay::timerCallback()
{
    // Pull audio from FIFO
    // audioProcessor.audioFifo is public
    audioProcessor.audioFifo.pull(inputTimeData, wetTimeData, 4096);
    
    // Calculate FFT
    performFFT(inputTimeData, inputFrequencyData);
    performFFT(wetTimeData, wetFrequencyData);
    
    // Apply smoothing (slower decay)
    float decay = 0.85f;
    for (size_t i = 0; i < 2048; ++i)
    {
        inputSmoothedSpectrum[i] = std::max(inputFrequencyData[i], inputSmoothedSpectrum[i] * decay);
        wetSmoothedSpectrum[i] = std::max(wetFrequencyData[i], wetSmoothedSpectrum[i] * decay);
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
    
    // Update value label when slider changes
    slider.onValueChange = [this]()
    {
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
    };
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
    
    // Create spectrum display
    spectrumDisplay = std::make_unique<SpectrumDisplay>(audioProcessor);
    addAndMakeVisible(*spectrumDisplay);
    
    // Add all knobs
    addAndMakeVisible(decayKnob);
    addAndMakeVisible(preDelayKnob);
    addAndMakeVisible(sizeKnob);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(widthKnob);
    addAndMakeVisible(highCutKnob);
    addAndMakeVisible(lowCutKnob);
    addAndMakeVisible(duckingKnob); // Added Ducking Knob

    // Mode Selector
    addAndMakeVisible(modeSelector);
    modeSelector.addItemList({"Plate", "Vintage", "Modern"}, 1);
    modeSelector.setJustificationType(juce::Justification::centred);
    modeSelector.setTooltip("Select the reverb algorithm character.");

    // Preset Selector
    addAndMakeVisible(presetSelector);
    juce::StringArray presets = { "Init", "Hall 1", "Hall 2", "Hall 3", "Room 1", "Room 2", "Room 3",
                           "Plate 1", "Plate 2", "Plate 3", "Cathedral", "Canyon", "Space",
                           "Ambient", "Shimmer", "Dark", "Bright", "Ethereal", "Extreme 1", "Extreme 2" };
    presetSelector.addItemList(presets, 1);
    presetSelector.setJustificationType(juce::Justification::centred);
    presetSelector.setTooltip("Load a preset.");

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

    // Initialize Tooltip Window
    tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 700);
    
    // Create parameter attachments
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
    highCutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "highCut", highCutKnob.getSlider());
    lowCutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "lowCut", lowCutKnob.getSlider());
    duckingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "ducking", duckingKnob.getSlider());
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "mode", modeSelector);
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "preset", presetSelector);
    
    // Tooltips
    decayKnob.getSlider().setTooltip("Sets the reverb decay time.");
    preDelayKnob.getSlider().setTooltip("Sets the initial delay before reverb starts.");
    sizeKnob.getSlider().setTooltip("Controls the size/density of the reverb.");
    mixKnob.getSlider().setTooltip("Blends between the dry and wet signal.");
    widthKnob.getSlider().setTooltip("Adjusts the stereo width.");
    highCutKnob.getSlider().setTooltip("Cuts high frequencies from the reverb.");
    lowCutKnob.getSlider().setTooltip("Cuts low frequencies from the reverb.");
    duckingKnob.getSlider().setTooltip("Compresses the reverb tail when input signal is present.");
    tooltipToggle.setTooltip("Toggle tooltips on/off.");

    // Set size - matching Solar Productions style
    setSize(750, 520);

    startTimerHz(10);
}

SirenXAudioProcessorEditor::~SirenXAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SirenXAudioProcessorEditor::timerCallback()
{
}

//==============================================================================
void SirenXAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Draw cached background
    if (cachedBackground.isValid())
        g.drawImageAt(cachedBackground, 0, 0);
    else
        drawBackground(g);
}

void SirenXAudioProcessorEditor::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto palette = lookAndFeel.getPalette();
    
    // Main background gradient
    juce::ColourGradient bgGradient(
        palette.backgroundDark,
        bounds.getX(), bounds.getY(),
        palette.backgroundMid,
        bounds.getX(), bounds.getBottom(),
        false);
    g.setGradientFill(bgGradient);
    g.fillRect(bounds);
    
    // Brushed metal texture effect
    juce::Random random(42);
    g.setColour(juce::Colours::white.withAlpha(0.015f));
    for (int i = 0; i < 200; ++i)
    {
        float x = random.nextFloat() * bounds.getWidth();
        float y = random.nextFloat() * bounds.getHeight();
        float len = random.nextFloat() * 40.0f + 10.0f;
        g.drawLine(x, y, x + len, y + random.nextFloat() * 2.0f - 1.0f, 0.5f);
    }

    // Draw static parts of rails, header, footer
    drawSideRails(g);
    drawHeader(g);
    drawFooter(g);
}

void SirenXAudioProcessorEditor::drawHeader(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop(50).toFloat();
    auto palette = lookAndFeel.getPalette();
    
    // Header gradient bar
    juce::ColourGradient headerGradient(
        palette.steelDark,
        headerBounds.getX(), headerBounds.getY(),
        palette.steelLight,
        headerBounds.getX(), headerBounds.getBottom(),
        false);
    g.setGradientFill(headerGradient);
    g.fillRect(headerBounds.withLeft(30.0f).withRight(headerBounds.getRight() - 30.0f));
    
    // Glow line at bottom of header
    g.setColour(palette.accentBright);
    g.fillRect(headerBounds.withLeft(30.0f).withRight(headerBounds.getRight() - 30.0f)
               .removeFromBottom(2.0f));
    
    // Title
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
    
    // Footer gradient bar
    juce::ColourGradient footerGradient(
        palette.steelLight,
        footerBounds.getX(), footerBounds.getY(),
        palette.steelDark,
        footerBounds.getX(), footerBounds.getBottom(),
        false);
    g.setGradientFill(footerGradient);
    g.fillRect(footerBounds.withLeft(30.0f).withRight(footerBounds.getRight() - 30.0f));
    
    // Glow line at top of footer
    g.setColour(palette.accentBright);
    g.fillRect(footerBounds.withLeft(30.0f).withRight(footerBounds.getRight() - 30.0f)
               .removeFromTop(2.0f));
    
    // "SOLAR" in accent, "PRODUCTIONS" in white
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
    
    // Left rail
    auto leftRail = bounds.removeFromLeft(static_cast<int>(railWidth)).toFloat();
    juce::ColourGradient leftGradient(
        palette.steelLight,
        leftRail.getX(), leftRail.getY(),
        palette.steelDark,
        leftRail.getRight(), leftRail.getY(),
        false);
    g.setGradientFill(leftGradient);
    g.fillRect(leftRail);
    
    // Right rail
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
    
    // Screws on left rail
    float screwSize = 8.0f;
    float screwX = leftRail.getCentreX();
    drawScrew(g, screwX, 70.0f, screwSize);
    drawScrew(g, screwX, getHeight() / 2.0f, screwSize);
    drawScrew(g, screwX, getHeight() - 70.0f, screwSize);
    
    // Screws on right rail
    screwX = rightRail.getCentreX();
    drawScrew(g, screwX, 70.0f, screwSize);
    drawScrew(g, screwX, getHeight() / 2.0f, screwSize);
    drawScrew(g, screwX, getHeight() - 70.0f, screwSize);
}

void SirenXAudioProcessorEditor::drawScrew(juce::Graphics& g, float x, float y, float size)
{
    auto palette = lookAndFeel.getPalette();

    // Screw base
    g.setColour(palette.steelDark);
    g.fillEllipse(x - size / 2.0f, y - size / 2.0f, size, size);
    
    // Highlight
    g.setColour(palette.screwHighlight);
    g.fillEllipse(x - size / 2.0f + 1.0f, y - size / 2.0f + 1.0f, size * 0.6f, size * 0.6f);
    
    // Cross slot
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

    // Create cached background
    if (getWidth() > 0 && getHeight() > 0)
    {
        cachedBackground = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics g(cachedBackground);
        drawBackground(g);
    }
    
    // Account for rails
    auto railLeft = bounds.removeFromLeft(30);
    bounds.removeFromRight(30);
    bounds.removeFromBottom(35);
    
    // Inner padding
    bounds.reduce(15, 10);
    auto faceplateArea = bounds;
    
    // Spectrum display at top (Pro-R style) - Larger
    spectrumDisplay->setBounds(faceplateArea.removeFromTop(180).reduced(0, 5));
    
    faceplateArea.removeFromTop(25);
    
    // Controls
    // Let's layout knobs in a nice arc or 2 rows
    auto mainControls = faceplateArea;

    // Place Preset Selector in Header area (below spectrum or integrated?)
    // Let's put it at the very top of controls, below spectrum
    auto controlsHeader = mainControls.removeFromTop(30);
    presetSelector.setBounds(controlsHeader.removeFromLeft(150));

    // Mode selector near controls
    modeSelector.setBounds(controlsHeader.removeFromRight(120));

    mainControls.removeFromTop(10);

    int knobWidth = mainControls.getWidth() / 4;
    int knobHeight = 90;

    auto row1 = mainControls.removeFromTop(knobHeight);
    decayKnob.setBounds(row1.removeFromLeft(knobWidth));
    preDelayKnob.setBounds(row1.removeFromLeft(knobWidth));
    sizeKnob.setBounds(row1.removeFromLeft(knobWidth));
    mixKnob.setBounds(row1.removeFromLeft(knobWidth));

    mainControls.removeFromTop(20);

    auto row2 = mainControls.removeFromTop(knobHeight);
    // 4 knobs on bottom row now
    widthKnob.setBounds(row2.removeFromLeft(knobWidth));
    highCutKnob.setBounds(row2.removeFromLeft(knobWidth));
    lowCutKnob.setBounds(row2.removeFromLeft(knobWidth));
    duckingKnob.setBounds(row2.removeFromLeft(knobWidth));
}
