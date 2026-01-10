/*
  ==============================================================================
    
    SirenX - Algorithmic Reverb Plugin
    Solar Productions
    
    PluginProcessor.h - Audio processing and parameter management
    
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ReverbEngine.h"
#include <atomic>
#include <array>

//==============================================================================
class SirenXAudioProcessor : public juce::AudioProcessor,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    SirenXAudioProcessor();
    ~SirenXAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    
    //==============================================================================
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    
    float getInputLevel() const { return inputLevel.load(); }
    float getOutputLevel() const { return outputLevel.load(); }
    float getDuckingGain() const { return lastDuckingGain.load(); }
    
    // Optimized FIFO for visualization
    static constexpr int fftSize = 2048;

    struct AudioFifo
    {
        void push(float input, float wet)
        {
            int idx = writeIndex.load();
            inputBuffer[idx] = input;
            wetBuffer[idx] = wet;
            writeIndex = (idx + 1) % bufferSize;
        }

        bool pull(std::vector<float>& inputBlock, std::vector<float>& wetBlock, int numSamples)
        {
            if (numSamples > bufferSize) numSamples = bufferSize;
            if (static_cast<size_t>(numSamples) > inputBlock.size()) return false;
            if (static_cast<size_t>(numSamples) > wetBlock.size()) return false;
            
            int currentWrite = writeIndex.load();
            int startRead = (currentWrite - numSamples + bufferSize) % bufferSize;

            for (int i = 0; i < numSamples; ++i)
            {
                int idx = (startRead + i) % bufferSize;
                inputBlock[static_cast<size_t>(i)] = inputBuffer[idx].load();
                wetBlock[static_cast<size_t>(i)] = wetBuffer[idx].load();
            }
            return true;
        }

        static constexpr int bufferSize = 4096; // Increased for 2048 FFT
        std::array<std::atomic<float>, bufferSize> inputBuffer {};
        std::array<std::atomic<float>, bufferSize> wetBuffer {};
        std::atomic<int> writeIndex { 0 };
    };

    // Reverb character types (Pro-R2 style)
    enum class ReverbCharacter { Plate = 0, Vintage = 1, Modern = 2 };
    
    void setCharacter(ReverbCharacter character);
    ReverbCharacter getCharacter() const { return currentCharacter; }

    AudioFifo audioFifo;

    double getBPM() const { return currentBPM; }

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    ReverbEngine reverbEngine;
    juce::AudioProcessorValueTreeState apvts;
    
    juce::AudioBuffer<float> tempInputBuffer;

    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> lastDuckingGain { 1.0f };
    
    double currentBPM = 120.0;
    ReverbCharacter currentCharacter = ReverbCharacter::Modern;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SirenXAudioProcessor)
};
