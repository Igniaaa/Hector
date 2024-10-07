/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
EffhectorAudioProcessor::EffhectorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ), params(*this, nullptr, "PARAMETERS", createParameterLayout()) 
#endif
{
}

EffhectorAudioProcessor::~EffhectorAudioProcessor()
{
}

//==============================================================================
const juce::String EffhectorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EffhectorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EffhectorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EffhectorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EffhectorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EffhectorAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EffhectorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EffhectorAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EffhectorAudioProcessor::getProgramName (int index)
{
    return {};
}

void EffhectorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EffhectorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    gain.reset(sampleRate, 0.0005); 
    //creo il buffer di delay circolare con le seguenti dichiarazioni
    auto delayBufferSize = sampleRate * 2.0;
    delayBuffer.setSize(getTotalNumOutputChannels(), (int)delayBufferSize);

}

void EffhectorAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EffhectorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void EffhectorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
        
    auto g = params.getRawParameterValue("GAIN")->load();
    gain.setTargetValue(g);
    

    //DBG("gain");
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        fillBuffer(buffer,channel);
        readBuffer(buffer, delayBuffer, gain,channel);
        fillBuffer(buffer, channel);
    }

    bufferPositionUpdate(buffer, delayBuffer);
}
void EffhectorAudioProcessor::fillBuffer(juce::AudioBuffer<float>& buffer, int channel)
{
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();

    if (delayBufferSize > bufferSize + writePos)
    {
        //copia i contenuti del main buffer a quello di delay
        delayBuffer.copyFrom(channel, writePos, buffer.getWritePointer(channel), bufferSize);
    }
    // se no
    else
    {
        // Calcolo quanto spazio rimane alla fine del delay buffer
        auto numSamplesToEnd = delayBufferSize - writePos;
        // Copio la quantità di contentuti verso la fine
        delayBuffer.copyFrom(channel, writePos, buffer.getWritePointer(channel), numSamplesToEnd);
        // Calcolo quanto contenuto rimane da copiare
        auto numSamplesAtStart = bufferSize - numSamplesToEnd;
        // Copio i contentuti rimanenti all'inizio del delay buffer
        delayBuffer.copyFrom(channel, 0, buffer.getWritePointer(channel, numSamplesToEnd), numSamplesAtStart);

    }
}

void EffhectorAudioProcessor::readBuffer(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer, juce::LinearSmoothedValue<float> gain,int  channel)
{
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();

    auto* delayTime = params.getRawParameterValue("DELAYMS");

    //delay
    auto readPos = writePos - (delayTime->load());

    if (readPos < 0)
    {
        readPos += delayBufferSize;
    }
    //feedback
    
    auto g = params.getRawParameterValue("GAIN")->load();

    //writePos = dov'è il nostro audio al momento
    //readPos = writePos - sampleRate
    if (readPos + bufferSize < delayBufferSize)
    {
        buffer.addFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPos), bufferSize, g, g);
    }
    else
    {
        auto numSamplesToEnd = delayBufferSize - readPos;
        buffer.addFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPos), numSamplesToEnd, g, g);

        auto numSamplesAtStart = bufferSize - numSamplesToEnd;
        buffer.addFromWithRamp(channel, numSamplesToEnd, delayBuffer.getReadPointer(channel, 0), numSamplesAtStart, g, g);
    }
}

void EffhectorAudioProcessor::bufferPositionUpdate(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer)
{
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();

    writePos += bufferSize;
    writePos %= delayBufferSize;
}
//==============================================================================
bool EffhectorAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EffhectorAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void EffhectorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EffhectorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EffhectorAudioProcessor();
}
juce::AudioProcessorValueTreeState::ParameterLayout EffhectorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DELAYMS", "Delay Ms", 0.0f, 8000.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GAIN", "Gain", 0.0f, 0.5f, 0.0f));
    return { params.begin(), params.end() };

}
