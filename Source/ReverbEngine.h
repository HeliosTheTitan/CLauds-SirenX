/*
  ==============================================================================

    SirenX - Algorithmic Reverb Plugin
    Solar Productions

    ReverbEngine.h - High-Quality Algorithmic Reverb (Pro-R2 Style)
    
    Character Modes:
    - Plate:   Bright, dense, fast diffusion, metallic shimmer
    - Vintage: Warm, colored, modulated, classic character
    - Modern:  Clean, transparent, smooth, neutral response

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

//==============================================================================
// Utility: Simple Delay Line with Safety
//==============================================================================
class DelayLine
{
public:
    void prepare(int maxSamples)
    {
        if (maxSamples < 1) maxSamples = 1;
        buffer.resize(static_cast<size_t>(maxSamples), 0.0f);
        writePos = 0;
        maxLength = maxSamples;
        prepared = true;
    }

    void clear()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }

    void setDelay(int samples)
    {
        delaySamples = juce::jlimit(0, juce::jmax(1, maxLength - 1), samples);
    }

    float read() const
    {
        if (!prepared || buffer.empty()) return 0.0f;
        int readPos = writePos - delaySamples;
        if (readPos < 0) readPos += maxLength;
        readPos = juce::jlimit(0, static_cast<int>(buffer.size()) - 1, readPos);
        return buffer[static_cast<size_t>(readPos)];
    }

    float readInterpolated(float delaySamplesFloat) const
    {
        if (!prepared || buffer.empty()) return 0.0f;
        
        float readPosFloat = static_cast<float>(writePos) - delaySamplesFloat;
        while (readPosFloat < 0.0f) readPosFloat += static_cast<float>(maxLength);
        
        int idx0 = static_cast<int>(readPosFloat) % maxLength;
        int idx1 = (idx0 + 1) % maxLength;
        float frac = readPosFloat - std::floor(readPosFloat);
        
        idx0 = juce::jlimit(0, static_cast<int>(buffer.size()) - 1, idx0);
        idx1 = juce::jlimit(0, static_cast<int>(buffer.size()) - 1, idx1);
        
        return buffer[static_cast<size_t>(idx0)] * (1.0f - frac) + 
               buffer[static_cast<size_t>(idx1)] * frac;
    }

    void write(float sample)
    {
        if (!prepared || buffer.empty()) return;
        buffer[static_cast<size_t>(writePos)] = sample;
        writePos = (writePos + 1) % maxLength;
    }

    float process(float input)
    {
        float output = read();
        write(input);
        return output;
    }

private:
    std::vector<float> buffer;
    int writePos = 0;
    int delaySamples = 0;
    int maxLength = 1;
    bool prepared = false;
};

//==============================================================================
// Utility: All-Pass Filter (for diffusion)
//==============================================================================
class AllPassFilter
{
public:
    void prepare(int maxSamples)
    {
        delay.prepare(maxSamples);
    }

    void clear()
    {
        delay.clear();
    }

    void setDelay(int samples)
    {
        delay.setDelay(samples);
    }

    void setCoefficient(float g)
    {
        coeff = juce::jlimit(-0.99f, 0.99f, g);
    }

    float process(float input)
    {
        float delayed = delay.read();
        float v = input - coeff * delayed;
        delay.write(v);
        return delayed + coeff * v;
    }

    float processModulated(float input, float modulatedDelay)
    {
        float delayed = delay.readInterpolated(modulatedDelay);
        float v = input - coeff * delayed;
        delay.write(v);
        return delayed + coeff * v;
    }

private:
    DelayLine delay;
    float coeff = 0.5f;
};

//==============================================================================
// Utility: One-Pole Filter (for damping)
//==============================================================================
class OnePoleLP
{
public:
    void setCoefficient(float c)
    {
        coeff = juce::jlimit(0.0001f, 0.9999f, c);
    }

    void setCutoff(float freq, float sampleRate)
    {
        if (sampleRate <= 0.0f) return;
        float w = juce::MathConstants<float>::twoPi * freq / sampleRate;
        coeff = 1.0f - std::exp(-w);
        coeff = juce::jlimit(0.0001f, 0.9999f, coeff);
    }

    float process(float input)
    {
        z1 = z1 + coeff * (input - z1);
        return z1;
    }

    void clear() { z1 = 0.0f; }

private:
    float coeff = 0.5f;
    float z1 = 0.0f;
};

//==============================================================================
// Utility: DC Blocker
//==============================================================================
class DCBlocker
{
public:
    float process(float input)
    {
        float output = input - xm1 + 0.9975f * ym1;
        xm1 = input;
        ym1 = output;
        return output;
    }

    void clear() { xm1 = ym1 = 0.0f; }

private:
    float xm1 = 0.0f;
    float ym1 = 0.0f;
};

//==============================================================================
// Utility: LFO for modulation
//==============================================================================
class LFO
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        phase = 0.0f;
    }

    void setFrequency(float hz)
    {
        freq = hz;
    }

    float process()
    {
        phase += freq / sr;
        if (phase >= 1.0f) phase -= 1.0f;
        return std::sin(juce::MathConstants<float>::twoPi * phase);
    }

    float processTriangle()
    {
        phase += freq / sr;
        if (phase >= 1.0f) phase -= 1.0f;
        return 4.0f * std::abs(phase - 0.5f) - 1.0f;
    }

    void clear() { phase = 0.0f; }

private:
    float sr = 44100.0f;
    float freq = 1.0f;
    float phase = 0.0f;
};

//==============================================================================
// Character Settings Structure
//==============================================================================
struct CharacterSettings
{
    // Diffusion
    float inputDiffusion1 = 0.75f;
    float inputDiffusion2 = 0.625f;
    float tankDiffusion1 = 0.7f;
    float tankDiffusion2 = 0.5f;
    
    // Modulation
    float modRate1 = 0.5f;
    float modRate2 = 0.7f;
    float modDepth = 8.0f;
    
    // Damping
    float dampingFreq = 8000.0f;
    float bandwidthFreq = 10000.0f;
    float bassFreq = 200.0f;
    float bassMult = 1.0f;
    
    // Decay shape
    float decayMultiplier = 1.0f;
    float crossfeed = 0.4f;
    
    // Early reflections
    float erDensity = 1.0f;
    float erLevel = 0.4f;
    
    // Tone
    float brightness = 1.0f;
    float warmth = 1.0f;
    
    static CharacterSettings getPlate()
    {
        CharacterSettings s;
        // Plate: Bright, dense, fast diffusion, metallic shimmer
        s.inputDiffusion1 = 0.78f;
        s.inputDiffusion2 = 0.68f;
        s.tankDiffusion1 = 0.75f;
        s.tankDiffusion2 = 0.62f;
        
        s.modRate1 = 0.8f;
        s.modRate2 = 1.1f;
        s.modDepth = 6.0f;
        
        s.dampingFreq = 12000.0f;
        s.bandwidthFreq = 14000.0f;
        s.bassFreq = 150.0f;
        s.bassMult = 0.9f;
        
        s.decayMultiplier = 0.95f;
        s.crossfeed = 0.35f;
        
        s.erDensity = 1.3f;
        s.erLevel = 0.35f;
        
        s.brightness = 1.2f;
        s.warmth = 0.85f;
        return s;
    }
    
    static CharacterSettings getVintage()
    {
        CharacterSettings s;
        // Vintage: Warm, colored, modulated, classic character
        s.inputDiffusion1 = 0.68f;
        s.inputDiffusion2 = 0.55f;
        s.tankDiffusion1 = 0.62f;
        s.tankDiffusion2 = 0.45f;
        
        s.modRate1 = 0.35f;
        s.modRate2 = 0.5f;
        s.modDepth = 12.0f;
        
        s.dampingFreq = 5500.0f;
        s.bandwidthFreq = 7000.0f;
        s.bassFreq = 300.0f;
        s.bassMult = 1.15f;
        
        s.decayMultiplier = 1.05f;
        s.crossfeed = 0.5f;
        
        s.erDensity = 0.85f;
        s.erLevel = 0.5f;
        
        s.brightness = 0.8f;
        s.warmth = 1.3f;
        return s;
    }
    
    static CharacterSettings getModern()
    {
        CharacterSettings s;
        // Modern: Clean, transparent, smooth, neutral response
        s.inputDiffusion1 = 0.75f;
        s.inputDiffusion2 = 0.625f;
        s.tankDiffusion1 = 0.7f;
        s.tankDiffusion2 = 0.5f;
        
        s.modRate1 = 0.5f;
        s.modRate2 = 0.7f;
        s.modDepth = 8.0f;
        
        s.dampingFreq = 9000.0f;
        s.bandwidthFreq = 11000.0f;
        s.bassFreq = 200.0f;
        s.bassMult = 1.0f;
        
        s.decayMultiplier = 1.0f;
        s.crossfeed = 0.4f;
        
        s.erDensity = 1.0f;
        s.erLevel = 0.4f;
        
        s.brightness = 1.0f;
        s.warmth = 1.0f;
        return s;
    }
};

//==============================================================================
// Early Reflections (Multi-tap delay)
//==============================================================================
class EarlyReflections
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        int maxDelay = static_cast<int>(0.15 * sampleRate) + 1;
        delayL.prepare(maxDelay);
        delayR.prepare(maxDelay);
        
        lpFilter[0].setCutoff(8000.0f, static_cast<float>(sampleRate));
        lpFilter[1].setCutoff(8000.0f, static_cast<float>(sampleRate));
        
        updateTapSamples();
    }

    void setSize(float s)
    {
        size = juce::jlimit(0.1f, 1.0f, s);
        updateTapSamples();
    }
    
    void setDensity(float d)
    {
        density = juce::jlimit(0.5f, 1.5f, d);
    }
    
    void setDamping(float freq)
    {
        lpFilter[0].setCutoff(freq, static_cast<float>(sr));
        lpFilter[1].setCutoff(freq, static_cast<float>(sr));
    }

    void clear()
    {
        delayL.clear();
        delayR.clear();
        lpFilter[0].clear();
        lpFilter[1].clear();
    }

    void process(float inputL, float inputR, float& outL, float& outR)
    {
        delayL.write(inputL);
        delayR.write(inputR);

        outL = 0.0f;
        outR = 0.0f;

        for (size_t i = 0; i < numTaps; ++i)
        {
            int tapSample = static_cast<int>(tapTimesMs[i] * size * density * sr / 1000.0);
            tapSample = juce::jlimit(1, static_cast<int>(0.15 * sr), tapSample);
            
            delayL.setDelay(tapSample);
            delayR.setDelay(tapSample);
            
            float tapL = delayL.read() * tapGains[i];
            float tapR = delayR.read() * tapGains[i];
            
            float panL = 1.0f - tapPans[i];
            float panR = tapPans[i];
            
            outL += tapL * panL + tapR * (1.0f - panR) * 0.25f;
            outR += tapR * panR + tapL * (1.0f - panL) * 0.25f;
        }
        
        outL = lpFilter[0].process(outL * 0.12f);
        outR = lpFilter[1].process(outR * 0.12f);
    }

private:
    void updateTapSamples()
    {
        tapTimesMs = { 5.0f, 11.0f, 17.0f, 23.0f, 31.0f, 37.0f, 43.0f, 53.0f, 
                       61.0f, 71.0f, 79.0f, 89.0f, 97.0f, 107.0f, 113.0f, 127.0f };
        
        tapPans = { 0.25f, 0.75f, 0.35f, 0.65f, 0.2f, 0.8f, 0.45f, 0.55f,
                    0.6f, 0.4f, 0.7f, 0.3f, 0.8f, 0.2f, 0.5f, 0.5f };
        
        tapGains = { 0.9f, 0.8f, 0.72f, 0.65f, 0.58f, 0.52f, 0.47f, 0.42f,
                     0.38f, 0.34f, 0.3f, 0.27f, 0.24f, 0.21f, 0.19f, 0.17f };
    }

    static constexpr size_t numTaps = 16;
    double sr = 44100.0;
    float size = 1.0f;
    float density = 1.0f;
    
    DelayLine delayL, delayR;
    OnePoleLP lpFilter[2];
    
    std::array<float, numTaps> tapTimesMs {};
    std::array<float, numTaps> tapPans {};
    std::array<float, numTaps> tapGains {};
};

//==============================================================================
// Dattorro-Style Plate Reverb Tank with Character Support
//==============================================================================
class DattorroTank
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;

        int maxDiff = static_cast<int>(0.05 * sampleRate) + 1;
        for (int i = 0; i < 4; ++i)
            inputDiffusion[i].prepare(maxDiff);
        
        setInputDiffusionDelays();

        int maxTankAP = static_cast<int>(0.1 * sampleRate) + 1;
        int maxTankDelay = static_cast<int>(0.25 * sampleRate) + 1;
        
        for (int i = 0; i < 2; ++i)
        {
            tankAPF1[i].prepare(maxTankAP);
            tankDelay1[i].prepare(maxTankDelay);
            tankAPF2[i].prepare(maxTankAP);
            tankDelay2[i].prepare(maxTankDelay);
            
            damping[i].setCutoff(8000.0f, static_cast<float>(sampleRate));
            bandwidth[i].setCutoff(10000.0f, static_cast<float>(sampleRate));
            bassFilter[i].setCutoff(200.0f, static_cast<float>(sampleRate));
        }

        setTankDelays();

        for (int i = 0; i < 4; ++i)
            lfo[i].prepare(static_cast<float>(sampleRate));
        
        lfo[0].setFrequency(0.5f);
        lfo[1].setFrequency(0.7f);
        lfo[2].setFrequency(0.6f);
        lfo[3].setFrequency(0.8f);

        dcBlocker[0].clear();
        dcBlocker[1].clear();

        applyCharacter(CharacterSettings::getModern());
        clear();
    }

    void setDecay(float d) { decay = juce::jlimit(0.0f, 0.995f, d); }

    void setDamping(float freq)
    {
        targetDampingFreq = freq;
        damping[0].setCutoff(freq * charSettings.brightness, static_cast<float>(sr));
        damping[1].setCutoff(freq * charSettings.brightness, static_cast<float>(sr));
    }
    
    void setBandwidth(float freq)
    {
        bandwidth[0].setCutoff(freq, static_cast<float>(sr));
        bandwidth[1].setCutoff(freq, static_cast<float>(sr));
    }

    void setModulation(float amount) { modDepth = amount; }
    
    void setCharacter(const CharacterSettings& settings)
    {
        charSettings = settings;
        applyCharacter(settings);
    }

    void clear()
    {
        for (int i = 0; i < 4; ++i)
            inputDiffusion[i].clear();
        
        for (int i = 0; i < 2; ++i)
        {
            tankAPF1[i].clear();
            tankDelay1[i].clear();
            tankAPF2[i].clear();
            tankDelay2[i].clear();
            damping[i].clear();
            bandwidth[i].clear();
            bassFilter[i].clear();
            dcBlocker[i].clear();
            tankState[i] = 0.0f;
        }
        
        for (int i = 0; i < 4; ++i)
            lfo[i].clear();
    }

    void process(float inputL, float inputR, float& outL, float& outR)
    {
        float input = (inputL + inputR) * 0.5f;
        input = bandwidth[0].process(input);

        float diffused = input;
        for (int i = 0; i < 4; ++i)
            diffused = inputDiffusion[i].process(diffused);

        float inL = diffused + tankState[1] * decay * charSettings.decayMultiplier;
        float inR = diffused + tankState[0] * decay * charSettings.decayMultiplier;

        float mod0 = lfo[0].process() * modDepth * charSettings.modDepth / 8.0f;
        float mod1 = lfo[1].process() * modDepth * charSettings.modDepth / 8.0f;
        float mod2 = lfo[2].processTriangle() * modDepth * charSettings.modDepth / 12.0f;
        float mod3 = lfo[3].processTriangle() * modDepth * charSettings.modDepth / 12.0f;

        // Left tank
        float baseDelay1L = msToSamplesF(22.58f);
        float sideL = tankAPF1[0].processModulated(inL, baseDelay1L + mod0);
        sideL = tankDelay1[0].process(sideL);
        sideL = damping[0].process(sideL);
        
        float bassL = bassFilter[0].process(sideL);
        sideL = sideL + bassL * (charSettings.bassMult - 1.0f);
        
        float baseDelay2L = msToSamplesF(60.48f);
        sideL = tankAPF2[0].processModulated(sideL, baseDelay2L + mod2);
        sideL = tankDelay2[0].process(sideL);
        tankState[0] = dcBlocker[0].process(sideL);

        // Right tank
        float baseDelay1R = msToSamplesF(30.51f);
        float sideR = tankAPF1[1].processModulated(inR, baseDelay1R + mod1);
        sideR = tankDelay1[1].process(sideR);
        sideR = damping[1].process(sideR);
        
        float bassR = bassFilter[1].process(sideR);
        sideR = sideR + bassR * (charSettings.bassMult - 1.0f);
        
        float baseDelay2R = msToSamplesF(89.24f);
        sideR = tankAPF2[1].processModulated(sideR, baseDelay2R + mod3);
        sideR = tankDelay2[1].process(sideR);
        tankState[1] = dcBlocker[1].process(sideR);

        float cf = charSettings.crossfeed;
        outL = tankState[0] * (1.0f - cf) + tankState[1] * cf;
        outR = tankState[1] * (1.0f - cf) + tankState[0] * cf;
    }

private:
    void setInputDiffusionDelays()
    {
        inputDiffusion[0].setDelay(msToSamples(4.77f));
        inputDiffusion[1].setDelay(msToSamples(3.60f));
        inputDiffusion[2].setDelay(msToSamples(12.73f));
        inputDiffusion[3].setDelay(msToSamples(9.31f));
    }
    
    void setTankDelays()
    {
        tankAPF1[0].setDelay(msToSamples(22.58f));
        tankAPF1[1].setDelay(msToSamples(30.51f));
        tankDelay1[0].setDelay(msToSamples(149.63f));
        tankDelay1[1].setDelay(msToSamples(141.70f));
        tankAPF2[0].setDelay(msToSamples(60.48f));
        tankAPF2[1].setDelay(msToSamples(89.24f));
        tankDelay2[0].setDelay(msToSamples(125.0f));
        tankDelay2[1].setDelay(msToSamples(106.28f));
    }
    
    void applyCharacter(const CharacterSettings& settings)
    {
        inputDiffusion[0].setCoefficient(settings.inputDiffusion1);
        inputDiffusion[1].setCoefficient(settings.inputDiffusion1);
        inputDiffusion[2].setCoefficient(settings.inputDiffusion2);
        inputDiffusion[3].setCoefficient(settings.inputDiffusion2);
        
        tankAPF1[0].setCoefficient(settings.tankDiffusion1);
        tankAPF1[1].setCoefficient(settings.tankDiffusion1);
        tankAPF2[0].setCoefficient(settings.tankDiffusion2);
        tankAPF2[1].setCoefficient(settings.tankDiffusion2);
        
        lfo[0].setFrequency(settings.modRate1);
        lfo[1].setFrequency(settings.modRate2);
        lfo[2].setFrequency(settings.modRate1 * 0.8f);
        lfo[3].setFrequency(settings.modRate2 * 0.9f);
        
        damping[0].setCutoff(settings.dampingFreq, static_cast<float>(sr));
        damping[1].setCutoff(settings.dampingFreq, static_cast<float>(sr));
        bandwidth[0].setCutoff(settings.bandwidthFreq, static_cast<float>(sr));
        bandwidth[1].setCutoff(settings.bandwidthFreq, static_cast<float>(sr));
        bassFilter[0].setCutoff(settings.bassFreq, static_cast<float>(sr));
        bassFilter[1].setCutoff(settings.bassFreq, static_cast<float>(sr));
    }

    int msToSamples(float ms) const { return juce::jmax(1, static_cast<int>(ms * sr / 1000.0)); }
    float msToSamplesF(float ms) const { return juce::jmax(1.0f, static_cast<float>(ms * sr / 1000.0)); }

    double sr = 44100.0;
    float decay = 0.5f;
    float modDepth = 0.5f;
    float targetDampingFreq = 8000.0f;
    CharacterSettings charSettings;

    std::array<AllPassFilter, 4> inputDiffusion;
    std::array<AllPassFilter, 2> tankAPF1;
    std::array<DelayLine, 2> tankDelay1;
    std::array<OnePoleLP, 2> damping;
    std::array<OnePoleLP, 2> bandwidth;
    std::array<OnePoleLP, 2> bassFilter;
    std::array<AllPassFilter, 2> tankAPF2;
    std::array<DelayLine, 2> tankDelay2;
    std::array<LFO, 4> lfo;
    std::array<DCBlocker, 2> dcBlocker;
    std::array<float, 2> tankState = { 0.0f, 0.0f };
};

//==============================================================================
// Main Reverb Engine
//==============================================================================
class ReverbEngine
{
public:
    ReverbEngine() = default;

    void prepare(double newSampleRate, int samplesPerBlock)
    {
        sampleRate = newSampleRate;

        int maxPreDelay = static_cast<int>(0.5 * sampleRate) + 1024;
        preDelayL.prepare(maxPreDelay);
        preDelayR.prepare(maxPreDelay);

        earlyReflections.prepare(sampleRate);
        tank.prepare(sampleRate);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
        spec.numChannels = 2;

        lowCutFilter.prepare(spec);
        lowCutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        lowCutFilter.setCutoffFrequency(20.0f);

        highCutFilter.prepare(spec);
        highCutFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        highCutFilter.setCutoffFrequency(20000.0f);

        duckingEnvelope = 0.0f;
        setCharacter(2);
        prepared = true;
    }

    void reset()
    {
        preDelayL.clear();
        preDelayR.clear();
        earlyReflections.clear();
        tank.clear();
        lowCutFilter.reset();
        highCutFilter.reset();
        duckingEnvelope = 0.0f;
    }

    void setDecayTime(float seconds)
    {
        decayTimeSec = juce::jlimit(0.1f, 30.0f, seconds);
        updateTankDecay();
    }

    void setPreDelay(float ms)
    {
        preDelayMs = juce::jlimit(0.0f, 500.0f, ms);
        int samples = static_cast<int>(preDelayMs * sampleRate / 1000.0);
        preDelayL.setDelay(samples);
        preDelayR.setDelay(samples);
    }

    void setSize(float s)
    {
        size = juce::jlimit(0.0f, 1.0f, s);
        earlyReflections.setSize(0.3f + size * 0.7f);
        updateTankDecay();
    }

    void setMix(float m) { mix = juce::jlimit(0.0f, 1.0f, m); }

    void setLowCut(float freq)
    {
        lowCutFilter.setCutoffFrequency(juce::jlimit(20.0f, 2000.0f, freq));
    }

    void setHighCut(float freq)
    {
        highCutFreq = juce::jlimit(200.0f, 20000.0f, freq);
        highCutFilter.setCutoffFrequency(highCutFreq);
        // Note: Tank damping is now controlled independently by character settings
        // This filter only affects the output wet signal
    }

    void setStereoWidth(float w) { stereoWidth = juce::jlimit(0.0f, 2.0f, w); }
    void setDucking(float d) { duckingAmount = juce::jlimit(0.0f, 1.0f, d); }
    
    void setCharacter(int characterIndex)
    {
        currentCharacter = juce::jlimit(0, 2, characterIndex);
        
        switch (currentCharacter)
        {
            case 0: charSettings = CharacterSettings::getPlate(); break;
            case 1: charSettings = CharacterSettings::getVintage(); break;
            case 2: 
            default: charSettings = CharacterSettings::getModern(); break;
        }
        
        tank.setCharacter(charSettings);
        earlyReflections.setDensity(charSettings.erDensity);
        erLevel = charSettings.erLevel;
        lateLevel = 1.0f - erLevel * 0.5f;
        updateTankDecay();
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (!prepared) return;
        juce::ScopedNoDenormals noDenormals;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numChannels < 2) return;

        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            float dryL = leftChannel[i];
            float dryR = rightChannel[i];

            float inputLevel = std::abs(dryL) + std::abs(dryR);
            float attackCoeff = 0.002f;
            float releaseCoeff = 0.9997f;
            
            if (inputLevel > duckingEnvelope)
                duckingEnvelope = duckingEnvelope + attackCoeff * (inputLevel - duckingEnvelope);
            else
                duckingEnvelope = duckingEnvelope * releaseCoeff;

            float preL = preDelayL.process(dryL);
            float preR = preDelayR.process(dryR);

            float erL, erR;
            earlyReflections.process(preL, preR, erL, erR);

            float lateL, lateR;
            tank.process(preL + erL * 0.4f, preR + erR * 0.4f, lateL, lateR);

            float wetL = erL * erLevel + lateL * lateLevel;
            float wetR = erR * erLevel + lateR * lateLevel;

            wetL *= charSettings.warmth;
            wetR *= charSettings.warmth;

            // Apply filters to WET signal only (high-pass then low-pass)
            wetL = lowCutFilter.processSample(0, wetL);
            wetR = lowCutFilter.processSample(1, wetR);
            wetL = highCutFilter.processSample(0, wetL);
            wetR = highCutFilter.processSample(1, wetR);

            if (std::abs(stereoWidth - 1.0f) > 0.01f)
            {
                float mid = (wetL + wetR) * 0.5f;
                float side = (wetL - wetR) * 0.5f * stereoWidth;
                wetL = mid + side;
                wetR = mid - side;
            }

            lastWetL = wetL;
            lastWetR = wetR;

            if (duckingAmount > 0.0f)
            {
                float duckGain = 1.0f - duckingEnvelope * duckingAmount * 3.0f;
                duckGain = juce::jlimit(0.0f, 1.0f, duckGain);
                wetL *= duckGain;
                wetR *= duckGain;
                currentDuckingGain = duckGain;
            }
            else
            {
                currentDuckingGain = 1.0f;
            }

            // Mix: dry signal is untouched, only wet signal is filtered
            leftChannel[i] = dryL * (1.0f - mix) + wetL * mix;
            rightChannel[i] = dryR * (1.0f - mix) + wetR * mix;
        }
    }

    float getLastWetL() const { return lastWetL; }
    float getLastWetR() const { return lastWetR; }

    float getDuckingGain() const { return currentDuckingGain; }

private:
    void updateTankDecay()
    {
        float normalizedDecay = std::log10(decayTimeSec + 0.1f) / std::log10(30.1f);
        float tankDecay = 0.2f + normalizedDecay * 0.795f;
        tankDecay *= charSettings.decayMultiplier;
        tankDecay += size * 0.05f;
        tankDecay = juce::jlimit(0.2f, 0.995f, tankDecay);
        tank.setDecay(tankDecay);

        float modAmount = 0.3f + (decayTimeSec / 30.0f) * 0.5f + size * 0.2f;
        tank.setModulation(modAmount);
    }

    double sampleRate = 44100.0;
    bool prepared = false;

    float preDelayMs = 0.0f;
    float decayTimeSec = 2.0f;
    float size = 0.5f;
    float mix = 0.5f;
    float stereoWidth = 1.0f;
    float duckingAmount = 0.0f;
    float highCutFreq = 12000.0f;

    float erLevel = 0.4f;
    float lateLevel = 0.6f;
    
    int currentCharacter = 2;
    CharacterSettings charSettings;

    DelayLine preDelayL, preDelayR;
    EarlyReflections earlyReflections;
    DattorroTank tank;

    juce::dsp::StateVariableTPTFilter<float> lowCutFilter;
    juce::dsp::StateVariableTPTFilter<float> highCutFilter;

    float duckingEnvelope = 0.0f;
    float lastWetL = 0.0f;
    float lastWetR = 0.0f;
    float currentDuckingGain = 1.0f;
};
