/*
  ==============================================================================
    
    SirenX - Convolution Reverb Plugin
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
    
    // Parameter tree
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    
    // Get waveform data for visualization
    float getInputLevel() const { return inputLevel.load(); }
    float getOutputLevel() const { return outputLevel.load(); }
    
    // Audio Capture for Visualization
    static constexpr int fftSize = 4096;

    // Simple lock-free queue for audio samples
    struct AudioFifo
    {
        void push(float input, float wet)
        {
            inputBuffer[writeIndex] = input;
            wetBuffer[writeIndex] = wet;
            writeIndex = (writeIndex + 1) % bufferSize;
        }

        bool pull(std::vector<float>& inputBlock, std::vector<float>& wetBlock, int numSamples)
        {
            int currentWrite = writeIndex.load();
            int startRead = (currentWrite - numSamples + bufferSize) % bufferSize;

            for (int i = 0; i < numSamples; ++i)
            {
                int idx = (startRead + i) % bufferSize;
                inputBlock[static_cast<size_t>(i)] = inputBuffer[static_cast<size_t>(idx)];
                wetBlock[static_cast<size_t>(i)] = wetBuffer[static_cast<size_t>(idx)];
            }
            return true;
        }

        static constexpr int bufferSize = 4096;
        std::array<std::atomic<float>, bufferSize> inputBuffer;
        std::array<std::atomic<float>, bufferSize> wetBuffer;
        std::atomic<int> writeIndex { 0 };
    };

    AudioFifo audioFifo;

    // Helpers
    double getBPM() const { return currentBPM; }

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    ReverbEngine reverbEngine;
    juce::AudioProcessorValueTreeState apvts;
    
    juce::AudioBuffer<float> tempInputBuffer; // For visualizer

    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    
    double currentBPM = 120.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SirenXAudioProcessor)
};
