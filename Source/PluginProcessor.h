/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class EffhectorAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    EffhectorAudioProcessor();
    ~EffhectorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================

private:
    //juce::dsp::ProcessorDuplicator<juce::dsp::StateVariableFilter::Filter<float>   >;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void EffhectorAudioProcessor::fillBuffer(juce::AudioBuffer<float>& buffer, int channel);
    void EffhectorAudioProcessor::readBuffer(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer, juce::LinearSmoothedValue<float> gain, int  channel);
    void EffhectorAudioProcessor::bufferPositionUpdate(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer);
    
    juce::LinearSmoothedValue<float> gain{ 0.0f };

    juce::AudioProcessorValueTreeState params;
    juce::AudioBuffer<float> delayBuffer;
    int writePos{ 0 }; 
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffhectorAudioProcessor)
};

