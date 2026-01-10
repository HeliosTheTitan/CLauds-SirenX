/*
  ==============================================================================

    SirenX - Convolution Reverb Plugin
    Solar Productions

    ReverbEngine.h - Hybrid Convolution/Algorithmic Reverb Engine

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

//==============================================================================
// Background Thread for Early Reflections (Convolution)
//==============================================================================
class IRGeneratorThread : public juce::Thread
{
public:
    IRGeneratorThread(juce::dsp::Convolution& c)
        : juce::Thread("IR Generator"), convolution(c), rng(42)
    {
        startThread();
    }

    ~IRGeneratorThread() override
    {
        signalThreadShouldExit();
        triggerUpdate();
        stopThread(2000);
    }

    void triggerUpdate()
    {
        updatePending.store(true);
        notify();
    }

    void setParameters(double sampleRate, float size)
    {
        currentSampleRate.store(sampleRate);
        targetSize.store(size);
        triggerUpdate();
    }

    void run() override
    {
        while (!threadShouldExit())
        {
            if (updatePending.exchange(false))
            {
                generateAndLoad();
            }
            wait(500);
        }
    }

private:
    void generateAndLoad()
    {
        double sampleRate = currentSampleRate.load();
        if (sampleRate <= 0.0) return;

        // Fixed short length for Early Reflections (0.3s)
        // This provides the "body" and "color" without the massive CPU cost of long tails
        float lengthSec = 0.3f;
        int lengthSamples = static_cast<int>(lengthSec * sampleRate);

        juce::AudioBuffer<float> irBuffer(2, lengthSamples);
        irBuffer.clear();

        // Deterministic noise
        rng.setSeed(12345);

        float sizeParam = targetSize.load(); // 0.0 to 1.0

        float* l = irBuffer.getWritePointer(0);
        float* r = irBuffer.getWritePointer(1);

        // Generate sparse reflections for ER
        // Density increases with 'size'
        int reflections = 50 + static_cast<int>(200 * sizeParam);

        for (int i = 0; i < reflections; ++i)
        {
            int idx = rng.nextInt(lengthSamples);
            float gain = (rng.nextFloat() * 2.0f - 1.0f) * (1.0f - (float)idx / lengthSamples);

            // Apply simple LPF to later reflections to simulate absorption
            gain *= 0.8f;

            if (idx < lengthSamples)
            {
                l[idx] += gain;
                // De-correlate right channel
                int idxR = idx + rng.nextInt(500) - 250;
                if (idxR >= 0 && idxR < lengthSamples)
                    r[idxR] += gain;
            }
        }

        convolution.loadImpulseResponse(std::move(irBuffer), sampleRate,
                                        juce::dsp::Convolution::Stereo::yes,
                                        juce::dsp::Convolution::Trim::no,
                                        juce::dsp::Convolution::Normalise::yes); // Normalize for consistent level
    }

    juce::dsp::Convolution& convolution;
    juce::Random rng;

    std::atomic<bool> updatePending { false };
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<float> targetSize { 1.0f };
};

//==============================================================================
// Reverb Engine
//==============================================================================
class ReverbEngine
{
public:
    ReverbEngine() : irThread(convolution)
    {
        duckingEnvelope.setCoefficient(0.9f);
    }

    void prepare(double newSampleRate, int samplesPerBlock)
    {
        sampleRate = newSampleRate;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = newSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
        spec.numChannels = 2;

        // 1. Convolution (Early Reflections)
        convolution.prepare(spec);

        // 2. Algorithmic Reverb (Tail)
        reverb.prepare(spec);

        // 3. Pre-Delay
        // Max 500ms
        int maxDelaySamples = static_cast<int>(0.5 * sampleRate) + 1024;
        preDelayBuffer.setSize(2, maxDelaySamples);
        preDelayBuffer.clear();
        preDelayWritePos = 0;

        // 4. Filters
        lowCutFilter.prepare(spec);
        lowCutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        highCutFilter.prepare(spec);
        highCutFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

        // Buffers
        dryBuffer.setSize(2, samplesPerBlock);
        convBuffer.setSize(2, samplesPerBlock);

        // Initial parameters
        updateParams();
        irThread.setParameters(sampleRate, size);

        prepared = true;
    }

    void reset()
    {
        convolution.reset();
        reverb.reset();
        lowCutFilter.reset();
        highCutFilter.reset();
        dryBuffer.clear();
        convBuffer.clear();
        preDelayBuffer.clear();
        duckingEnvelope.reset();
    }

    void setDecayTime(float seconds)
    {
        if (std::abs(decayTimeSec - seconds) > 0.01f) {
            decayTimeSec = seconds;
            shouldUpdateParams = true;
        }
    }
    void setPreDelay(float ms)
    {
        preDelayMs = ms;
        // No heavy update needed, just atomic read in process
    }
    void setSize(float s)
    {
        if (std::abs(size - s) > 0.01f) {
            size = s;
            shouldUpdateParams = true;
            irThread.setParameters(sampleRate, size);
        }
    }
    void setMix(float m) { mix = juce::jlimit(0.0f, 1.0f, m); }
    void setLowCut(float freq) { lowCutFilter.setCutoffFrequency(freq); }
    void setHighCut(float freq) { highCutFilter.setCutoffFrequency(freq); }
    void setStereoWidth(float w) { stereoWidth = juce::jlimit(0.0f, 2.0f, w); }
    void setDucking(float d) { duckingAmount = juce::jlimit(0.0f, 1.0f, d); }

    void setMode(int mode)
    {
        if (currentMode != mode)
        {
            currentMode = mode;
            shouldUpdateParams = true;
        }
    }

    // Returns the current gain reduction (0.0 = full reduction, 1.0 = no reduction)
    float getDuckingGain() const { return currentDuckingGain.load(); }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (!prepared) return;
        juce::ScopedNoDenormals noDenormals;

        if (shouldUpdateParams)
        {
            updateParams();
            shouldUpdateParams = false;
        }

        int numSamples = buffer.getNumSamples();

        // Safety check
        if (dryBuffer.getNumSamples() < numSamples || convBuffer.getNumSamples() < numSamples) return;

        // 1. Store Dry Input for Ducking and Mix
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        // 2. Apply Pre-Delay (Circular Buffer)
        applyPreDelay(buffer);

        // 3. Split processing
        // Path A: Convolution (Early Reflections)
        // Copy buffer to convBuffer
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            convBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> convBlock(convBuffer);
        juce::dsp::ProcessContextReplacing<float> convContext(convBlock);
        convolution.process(convContext);

        // Path B: Algorithmic Reverb (Tail)
        // In-place on 'buffer'
        juce::dsp::AudioBlock<float> reverbBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> reverbContext(reverbBlock);
        reverb.process(reverbContext);

        // 4. Mix Early (Conv) and Late (Reverb)
        // Add Conv output to Reverb output (Parallel)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, convBuffer, ch, 0, numSamples);

        // 5. Post-Processing (Filters)
        juce::dsp::AudioBlock<float> outBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> outContext(outBlock);
        lowCutFilter.process(outContext);
        highCutFilter.process(outContext);

        // Capture for visualizer
        lastWetL = buffer.getSample(0, 0);
        lastWetR = buffer.getSample(1, 0);

        // 6. Stereo Width
        if (std::abs(stereoWidth - 1.0f) > 0.01f)
        {
            float* l = buffer.getWritePointer(0);
            float* r = buffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i)
            {
                float mid = (l[i] + r[i]) * 0.5f;
                float side = (l[i] - r[i]) * 0.5f * stereoWidth;
                l[i] = mid + side;
                r[i] = mid - side;
            }
        }

        // 7. Ducking
        float currentGain = 1.0f;
        if (duckingAmount > 0.0f)
        {
            float inputLevel = dryBuffer.getMagnitude(0, numSamples);
            float env = duckingEnvelope.process(inputLevel);
            float threshold = 0.1f;
            if (env > threshold)
            {
                float reduction = (env - threshold) * duckingAmount * 2.0f;
                currentGain = std::max(0.0f, 1.0f - reduction);
                buffer.applyGain(currentGain);
            }
        }
        currentDuckingGain.store(currentGain);

        // 8. Final Mix (Dry + Wet)
        float dryGain = 1.0f - mix;
        float wetGain = mix;

        buffer.applyGain(wetGain);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, dryBuffer, ch, 0, numSamples, dryGain);
    }

    float getLastWetL() const { return lastWetL; }
    float getLastWetR() const { return lastWetR; }

private:
    void applyPreDelay(juce::AudioBuffer<float>& buffer)
    {
        int numSamples = buffer.getNumSamples();
        int delaySamples = static_cast<int>(preDelayMs * sampleRate / 1000.0f);
        int bufferLength = preDelayBuffer.getNumSamples();

        if (delaySamples >= bufferLength - numSamples)
            delaySamples = bufferLength - numSamples - 1;
        if (delaySamples < 0) delaySamples = 0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* input = buffer.getReadPointer(ch);
            auto* output = buffer.getWritePointer(ch);
            auto* delayData = preDelayBuffer.getWritePointer(ch);

            int localWritePos = preDelayWritePos;

            for (int i = 0; i < numSamples; ++i)
            {
                delayData[localWritePos] = input[i];

                // Read from past
                int readPos = (localWritePos - delaySamples + bufferLength) % bufferLength;
                output[i] = delayData[readPos];

                localWritePos = (localWritePos + 1) % bufferLength;
            }
        }

        // Advance write pointer once per block
        preDelayWritePos = (preDelayWritePos + numSamples) % bufferLength;
    }

    void updateParams()
    {
        juce::dsp::Reverb::Parameters params;

        // Map Decay (0.1s - 30.0s) to Room Size (0.0 - 1.0)
        // Non-linear mapping for better feel
        // Adjusted for longer decay
        params.roomSize = juce::jlimit(0.0f, 0.99f, 0.3f + (decayTimeSec / 30.0f) * 0.69f);

        // Adjust parameters based on Mode
        // 0: Plate (Bright, Dense)
        // 1: Vintage (Darker, warmer)
        // 2: Modern (Clean, Wide)

        switch (currentMode)
        {
            case 0: // Plate
                params.damping = 0.2f;
                break;
            case 1: // Vintage
                params.damping = 0.7f;
                break;
            case 2: // Modern
                params.damping = 0.4f;
                break;
            default:
                params.damping = 0.5f;
                break;
        }

        params.width = juce::jlimit(0.0f, 1.0f, size); // Use size for width
        params.wetLevel = 1.0f; // Handled by our mix
        params.dryLevel = 0.0f;
        params.freezeMode = 0.0f;

        reverb.setParameters(params);

        // Also update IR Thread params if needed (simulate ER difference)
        // Ideally we would change the IR generation strategy based on mode
    }

    double sampleRate = 44100.0;
    bool prepared = false;
    bool shouldUpdateParams = true;

    // DSP Modules
    juce::dsp::Convolution convolution;
    juce::dsp::Reverb reverb;
    IRGeneratorThread irThread;

    juce::dsp::StateVariableTPTFilter<float> lowCutFilter;
    juce::dsp::StateVariableTPTFilter<float> highCutFilter;

    // Buffers
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> convBuffer; // Temp buffer for convolution path
    juce::AudioBuffer<float> preDelayBuffer;
    int preDelayWritePos = 0;

    // Helpers
    class OnePole
    {
    public:
        void setCoefficient(float c) { coeff = c; }
        float process(float input)
        {
            current = current + coeff * (input - current);
            return current;
        }
        void reset() { current = 0.0f; }
    private:
        float coeff = 0.5f;
        float current = 0.0f;
    } duckingEnvelope;

    // Parameters
    float preDelayMs = 0.0f;
    float decayTimeSec = 2.0f;
    float size = 1.0f;
    float mix = 0.5f;
    float stereoWidth = 1.0f;
    float duckingAmount = 0.0f;
    int currentMode = 2; // Default to Modern

    float lastWetL = 0.0f;
    float lastWetR = 0.0f;

    std::atomic<float> currentDuckingGain { 1.0f };
};
