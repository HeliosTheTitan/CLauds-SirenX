/*
  ==============================================================================
    
    SirenX - Algorithmic Reverb Plugin
    Solar Productions
    
    PluginProcessor.cpp - Audio processing implementation
    
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SirenXAudioProcessor::SirenXAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    apvts.addParameterListener("decayTime", this);
    apvts.addParameterListener("preDelay", this);
    apvts.addParameterListener("size", this);
    apvts.addParameterListener("mix", this);
    apvts.addParameterListener("stereoWidth", this);
    apvts.addParameterListener("highCut", this);
    apvts.addParameterListener("lowCut", this);
    apvts.addParameterListener("ducking", this);
    apvts.addParameterListener("character", this);
}

SirenXAudioProcessor::~SirenXAudioProcessor()
{
    apvts.removeParameterListener("decayTime", this);
    apvts.removeParameterListener("preDelay", this);
    apvts.removeParameterListener("size", this);
    apvts.removeParameterListener("mix", this);
    apvts.removeParameterListener("stereoWidth", this);
    apvts.removeParameterListener("highCut", this);
    apvts.removeParameterListener("lowCut", this);
    apvts.removeParameterListener("ducking", this);
    apvts.removeParameterListener("character", this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SirenXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Decay Time (0.1s to 10.0s) - Extended range for algorithmic reverb
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "decayTime", 1 }, "Decay",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.1f, 0.4f),
        2.0f,
        juce::AudioParameterFloatAttributes().withLabel("s")));
    
    // Pre Delay (0ms to 500ms)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "preDelay", 1 }, "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 500.0f, 1.0f, 0.5f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // Size (0% to 100%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "size", 1 }, "Size",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    
    // Mix (0% to 100%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "mix", 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    
    // Stereo Width (0% to 200%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "stereoWidth", 1 }, "Width",
        juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    
    // High Cut Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "highCut", 1 }, "High Cut",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.3f),
        12000.0f, // Lower default for warmer sound
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    
    // Low Cut Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "lowCut", 1 }, "Low Cut",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f),
        20.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // Ducking
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ducking", 1 }, "Ducking",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Character (0 = Plate, 1 = Vintage, 2 = Modern)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "character", 1 }, "Character",
        juce::StringArray{ "Plate", "Vintage", "Modern" },
        2)); // Default to Modern
    
    return { params.begin(), params.end() };
}

//==============================================================================
void SirenXAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "decayTime")
        reverbEngine.setDecayTime(newValue);
    else if (parameterID == "preDelay")
        reverbEngine.setPreDelay(newValue);
    else if (parameterID == "size")
        reverbEngine.setSize(newValue / 100.0f);
    else if (parameterID == "mix")
        reverbEngine.setMix(newValue / 100.0f);
    else if (parameterID == "stereoWidth")
        reverbEngine.setStereoWidth(newValue / 100.0f);
    else if (parameterID == "highCut")
        reverbEngine.setHighCut(newValue);
    else if (parameterID == "lowCut")
        reverbEngine.setLowCut(newValue);
    else if (parameterID == "ducking")
        reverbEngine.setDucking(newValue / 100.0f);
    else if (parameterID == "character")
    {
        currentCharacter = static_cast<ReverbCharacter>(static_cast<int>(newValue));
        reverbEngine.setCharacter(static_cast<int>(newValue));
    }
}

void SirenXAudioProcessor::setCharacter(ReverbCharacter character)
{
    currentCharacter = character;
    if (auto* param = apvts.getParameter("character"))
        param->setValueNotifyingHost(static_cast<float>(static_cast<int>(character)) / 2.0f);
}

//==============================================================================
const juce::String SirenXAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SirenXAudioProcessor::acceptsMidi() const { return false; }
bool SirenXAudioProcessor::producesMidi() const { return false; }
bool SirenXAudioProcessor::isMidiEffect() const { return false; }
double SirenXAudioProcessor::getTailLengthSeconds() const { return 10.0; }

int SirenXAudioProcessor::getNumPrograms() { return 1; }
int SirenXAudioProcessor::getCurrentProgram() { return 0; }
void SirenXAudioProcessor::setCurrentProgram(int) {}
const juce::String SirenXAudioProcessor::getProgramName(int) { return {}; }
void SirenXAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void SirenXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverbEngine.prepare(sampleRate, samplesPerBlock);
    
    // Initialize all parameters
    reverbEngine.setDecayTime(*apvts.getRawParameterValue("decayTime"));
    reverbEngine.setPreDelay(*apvts.getRawParameterValue("preDelay"));
    reverbEngine.setSize(*apvts.getRawParameterValue("size") / 100.0f);
    reverbEngine.setMix(*apvts.getRawParameterValue("mix") / 100.0f);
    reverbEngine.setStereoWidth(*apvts.getRawParameterValue("stereoWidth") / 100.0f);
    reverbEngine.setHighCut(*apvts.getRawParameterValue("highCut"));
    reverbEngine.setLowCut(*apvts.getRawParameterValue("lowCut"));
    reverbEngine.setDucking(*apvts.getRawParameterValue("ducking") / 100.0f);
    
    // Initialize character
    int charIndex = static_cast<int>(*apvts.getRawParameterValue("character"));
    currentCharacter = static_cast<ReverbCharacter>(charIndex);
    reverbEngine.setCharacter(charIndex);
    
    tempInputBuffer.setSize(2, samplesPerBlock);
}

void SirenXAudioProcessor::releaseResources()
{
    reverbEngine.reset();
}

bool SirenXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    
    return true;
}

void SirenXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    // Store input for visualization
    if (buffer.getNumChannels() >= 2)
    {
        if (tempInputBuffer.getNumSamples() < buffer.getNumSamples())
            tempInputBuffer.setSize(2, buffer.getNumSamples(), false, false, true);
        
        for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
            tempInputBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }

    // Process reverb (all DSP happens inside)
    reverbEngine.processBlock(buffer);
    
    // Push to visualizer FIFO
    if (buffer.getNumChannels() >= 2)
    {
        auto* lIn = tempInputBuffer.getReadPointer(0);
        auto* rIn = tempInputBuffer.getReadPointer(1);
        auto* lOut = buffer.getReadPointer(0);
        auto* rOut = buffer.getReadPointer(1);

        // Downsample for visualizer (every 4th sample) to reduce CPU
        for (int i = 0; i < buffer.getNumSamples(); i += 4)
        {
            float monoIn = (lIn[i] + rIn[i]) * 0.5f;
            float monoOut = (lOut[i] + rOut[i]) * 0.5f;
            audioFifo.push(monoIn, monoOut);
        }
    }
    
    // Update level meters (use RMS for smoother display)
    inputLevel.store(tempInputBuffer.getRMSLevel(0, 0, buffer.getNumSamples()));
    outputLevel.store(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));

    // Update ducking gain for visualizer
    lastDuckingGain.store(reverbEngine.getDuckingGain());
}

//==============================================================================
bool SirenXAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SirenXAudioProcessor::createEditor()
{
    return new SirenXAudioProcessorEditor(*this);
}

//==============================================================================
void SirenXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SirenXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SirenXAudioProcessor();
}
