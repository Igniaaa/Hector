/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <foleys_gui_magic/foleys_gui_magic.h>


//==============================================================================
class EffhectorAudioProcessor  : public foleys::MagicProcessor
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
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState params;
    
    juce::LinearSmoothedValue<float> gain{ 0.0f };

    juce::AudioBuffer<float> delayBuffer;
    juce::AudioBuffer<float> chorusBuffer;  // Buffer separato per chorus
    int chorusWritePos = 0;

    //testine
    float delaySamplesA = 0.0f;
    float delaySamplesB = 0.0f;

    bool usingA = true;
    bool isCrossfading = false;

    float crossfadePos = 0.0f;
    float crossfadeInc = 0.0f; 
    float crossfadeTime = 0.05f; // 50 ms




    //delay params
    int writePos{ 0 }; 
    float sampleRate{ 44100.0 };
    
    bool wasDelayEnabled = false;
    float targetDelaySamples = 0.0f;
    float currentDelaySamples = 0.0f; //smoothed
    float smoothingCoeff = 0.0005f;

    float dry{ 1.0f };
    float wet{ 0.5f };

    //chorus params
    float chorusPhase = 0.0f;
    float chorusLFOFreq = 0.3f;   //Hz
    float chorusDepthMs = 4.0f;   //modulazione (ms)
    float chorusDelayMs = 8.0f;   //base delay ms

    //Reverb
    juce::Reverb reverb;
    juce::Reverb::Parameters revParams;
    juce::AudioBuffer<float> revBuffer;

    bool wasReverbEnabled = false;
    float revMix = 0.3f; //mix dry/wet
    float revRoom = 0.5f; //tempo di 
    float revDamp = 0.5f; //chiarità reverb
    float revWidth = 1.0f; //stereofonia del riverbero


    //==============================================================================
    
    foleys::MagicProcessorState magicState{ *this };
    
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffhectorAudioProcessor)
};

