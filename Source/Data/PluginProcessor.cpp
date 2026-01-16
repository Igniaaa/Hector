/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"

//==============================================================================
//==============================================================================
EffhectorAudioProcessor::EffhectorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
	: foleys::MagicProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
		.withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
	), params(*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
	//Imposto il source path per il salvataggio delle impostazioni UI
	FOLEYS_SET_SOURCE_PATH(__FILE__);

	// Carica automaticamente Foleys GUI XML
	{
		auto sourceFolder = juce::File{ __FILE__ }.getParentDirectory(); // Source folder
		auto guiFile = sourceFolder.getChildFile("magic.sav.xml");
		if (guiFile.existsAsFile())
		{
			magicState.setGuiValueTree(guiFile);
		}
		else
		{
			DBG("Foleys GUI XML not found: " + guiFile.getFullPathName());
		}
	}
	// Non salva automaticamente un nuovo xml
	magicState.setApplicationSettingsFile(juce::File()); // Passa un file vuoto
}

EffhectorAudioProcessor::~EffhectorAudioProcessor()
{
}

//==============================================================================
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

void EffhectorAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String EffhectorAudioProcessor::getProgramName(int index)
{
	return {};
}

void EffhectorAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}


//==============================================================================
//funzioni helper
//==============================================================================
static float linearInterp(float a, float b, float t)
{
	return a + t * (b - a);
}

static float readWithLerp(const juce::AudioBuffer<float>& buf, int ch, float pos)
{
	int size = buf.getNumSamples();
	while (pos < 0) pos += size;
	while (pos >= size) pos -= size;

	int i0 = (int)pos;
	int i1 = (i0 + 1) % size;
	float frac = pos - i0;

	return linearInterp(buf.getSample(ch, i0), buf.getSample(ch, i1), frac);
}

static float readWithQlerp(const juce::AudioBuffer<float>& buf, int ch, float pos)
{
	const int size = buf.getNumSamples();

	// Normalizza posizione
	while (pos < 0) pos += size;
	while (pos >= size) pos -= size;

	int i0 = (int)pos;
	float frac = pos - (float)i0;

	// Indici dei 3 punti usati nella quadratic
	int iM1 = (i0 - 1 + size) % size;
	int iP1 = (i0 + 1) % size;

	// Campioni
	float xm1 = buf.getSample(ch, iM1);
	float x0 = buf.getSample(ch, i0);
	float xp1 = buf.getSample(ch, iP1);

	// Lagrange quadratic interpolation
	float c0 = x0;
	float c1 = 0.5f * (xp1 - xm1);
	float c2 = 0.5f * (xm1 - 2.0f * x0 + xp1);

	return c0 + c1 * frac + c2 * frac * frac;
}



//==============================================================================
//==============================================================================
void EffhectorAudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
	gain.reset(sr, 0.0005);


	//creo il buffer di delay circolare con le seguenti dichiarazioni
	sampleRate = sr;

	int maxDelay = (int)(sr * 10.0); // 10 secondi
	delayBuffer.setSize(getTotalNumOutputChannels(), maxDelay);
	delayBuffer.clear();

	int maxChorusDelay = (int)(sr * 0.05f);
	chorusBuffer.setSize(getTotalNumOutputChannels(), maxChorusDelay);
	chorusBuffer.clear();

	writePos = 0;
	chorusWritePos = 0;

	currentDelaySamples = 0.0f;

	//Preparo i parametri del riverbero
	reverb.setParameters(revParams);

	revParams.roomSize = 0.5f;
	revParams.damping = 0.5f;
	revParams.width = 1.0f;
	revParams.wetLevel = 0.3f;
	revParams.dryLevel = 0.0f; // DRY viene gestito a mano


	revBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
	revBuffer.clear();

}

void EffhectorAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
	// 
	// 1. Reset reverb 
	reverb.reset();

	// 2. Pulisco i buffer 
	delayBuffer.clear();
	chorusBuffer.clear();
	revBuffer.clear();

	// 3. Reset posizioni
	writePos = 0;
	chorusWritePos = 0;

	// 4. Reset stato crossfade 
	isCrossfading = false;
	crossfadePos = 0.0f;

}



#ifndef JucePlugin_PreferredChannelConfigurations
	bool EffhectorAudioProcessor::isBusesLayoutSupported(const BusesLayout & layouts) const
	{
#if JucePlugin_IsMidiEffect
		juce::ignoreUnused(layouts);
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

	void EffhectorAudioProcessor::processBlock(juce::AudioBuffer<float>&buffer, juce::MidiBuffer & midiMessages)
	{
		juce::ScopedNoDenormals noDenormals;

		auto numSamples = buffer.getNumSamples();
		auto numChannels = buffer.getNumChannels();
		int delayBufSize = delayBuffer.getNumSamples();
		int chorusBufSize = chorusBuffer.getNumSamples();

		// Lettura parametri
		bool enableDelay = params.getRawParameterValue("ENABLE_DELAY")->load();
		bool enableChorus = params.getRawParameterValue("ENABLE_CHORUS")->load();
		bool enableReverb = params.getRawParameterValue("ENABLE_REVERB")->load();

		float fb = params.getRawParameterValue("FEEDBACK")->load();
		float delayMs = params.getRawParameterValue("DELAYMS")->load();
		float dry = params.getRawParameterValue("DRY")->load();
		float wet = params.getRawParameterValue("WET")->load();

		float chorusRate = params.getRawParameterValue("CHORUS_RATE")->load();
		float chorusDepth = params.getRawParameterValue("CHORUS_DEPTH")->load();
		float chorusDelay = params.getRawParameterValue("CHORUS_DELAY")->load();
		float chorusMix = params.getRawParameterValue("CHORUS_MIX")->load();

		// Aggiorna target delay
		targetDelaySamples = (delayMs * sampleRate) / 1000.0f;

		// Inizia crossfade se necessario
		if (!isCrossfading && enableDelay) {
			float currentDelay = usingA ? delaySamplesA : delaySamplesB;
			if (std::abs(targetDelaySamples - currentDelay) > 1.0f) {
				isCrossfading = true;
				crossfadePos = 0.0f;
				crossfadeInc = 1.0f / (crossfadeTime * sampleRate);

				if (usingA) {
					delaySamplesB = targetDelaySamples;
				}
				else {
					delaySamplesA = targetDelaySamples;
				}
			}
		}

		// Clear reverb buffer se disabilitato
		if (wasReverbEnabled && !enableReverb) {
			reverb.reset();
			revBuffer.clear();
		}
		wasReverbEnabled = enableReverb;

		// Processa campione per campione
		for (int i = 0; i < numSamples; ++i) {
			// Smoothing del delay
			currentDelaySamples += smoothingCoeff * (targetDelaySamples - currentDelaySamples);

			// LFO del chorus (solo se chorus abilitato)
			float lfo = 0.0f;
			if (enableChorus) {
				lfo = std::sin(2.0f * juce::MathConstants<float>::pi * chorusPhase);
				chorusPhase += chorusRate / sampleRate;
				if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;
			}

			for (int ch = 0; ch < numChannels; ++ch) {
				float in = buffer.getReadPointer(ch)[i];
				float delayedSample = 0.0f;
				float chorusSample = 0.0f;

				// === DELAY ===
				if (enableDelay) {
					// Calcola posizione lettura
					float readPos = writePos - currentDelaySamples;
					while (readPos < 0) readPos += delayBufSize;
					while (readPos >= delayBufSize) readPos -= delayBufSize;

					// Leggi con interpolazione quadratica
					delayedSample = readWithQlerp(delayBuffer, ch, readPos);

					// Gestione crossfade
					if (isCrossfading) {
						float oldDelay = usingA ? delaySamplesA : delaySamplesB;
						float newDelay = usingA ? delaySamplesB : delaySamplesA;

						float oldPos = writePos - oldDelay;
						float newPos = writePos - newDelay;

						while (oldPos < 0) oldPos += delayBufSize;
						while (oldPos >= delayBufSize) oldPos -= delayBufSize;
						while (newPos < 0) newPos += delayBufSize;
						while (newPos >= delayBufSize) newPos -= delayBufSize;

						float oldSample = readWithQlerp(delayBuffer, ch, oldPos);
						float newSample = readWithQlerp(delayBuffer, ch, newPos);

						float fadeOut = 1.0f - crossfadePos;
						float fadeIn = crossfadePos;

						delayedSample = (oldSample * fadeOut) + (newSample * fadeIn);
					}

					// Scrivi nel buffer delay (con feedback)
					float toWrite = in + delayedSample * fb;
					toWrite = juce::jlimit(-1.0f, 1.0f, toWrite);
					delayBuffer.setSample(ch, writePos, toWrite);
				}

				// === CHORUS ===
				if (enableChorus) {
					// Scrivi l'input corrente nel buffer chorus
					chorusBuffer.setSample(ch, chorusWritePos, in);

					// Calcola posizione lettura chorus
					float chorusMs = chorusDelay + lfo * chorusDepth;
					float chorusSamples = (chorusMs * sampleRate) / 1000.0f;

					// Assicurati che il delay del chorus non superi la dimensione del buffer
					chorusSamples = juce::jmin(chorusSamples, (float)(chorusBufSize - 1));

					float chorusReadPos = chorusWritePos - chorusSamples;

					// Gestione wrap-around
					while (chorusReadPos < 0.0f) chorusReadPos += chorusBufSize;
					while (chorusReadPos >= chorusBufSize) chorusReadPos -= chorusBufSize;

					// Lettura con interpolazione lineare
					chorusSample = readWithLerp(chorusBuffer, ch, chorusReadPos);
				}

				// === OUTPUT ===
				float output = in * dry;
				if (enableDelay) output += delayedSample * wet;
				if (enableChorus) output += chorusSample * chorusMix;

				buffer.setSample(ch, i, output);
			}

			// === INCREMENTI ===
			if (enableDelay) {
				writePos++;
				if (writePos >= delayBufSize) writePos = 0;
			}

			if (enableChorus) {
				chorusWritePos++;
				if (chorusWritePos >= chorusBufSize) chorusWritePos = 0;
			}

			// Aggiorna crossfade
			if (isCrossfading) {
				crossfadePos += crossfadeInc;
				if (crossfadePos >= 1.0f) {
					crossfadePos = 0.0f;
					isCrossfading = false;
					usingA = !usingA;
				}
			}
		}

		// === REVERB ===
		if (enableReverb) {
			// Aggiorna parametri reverb
			revParams.roomSize = params.getRawParameterValue("REVERB_ROOM")->load();
			revParams.damping = params.getRawParameterValue("REVERB_DAMPING")->load();
			revParams.width = params.getRawParameterValue("REVERB_WIDTH")->load();
			float revMix = params.getRawParameterValue("REVERB_MIX")->load();

			// Imposta wet/dry levels per il reverb JUCE
			revParams.wetLevel = revMix;
			revParams.dryLevel = 1.0f - revMix; // Dry gestito internamente

			reverb.setParameters(revParams);

			// Assicura dimensione corretta del buffer reverb
			if (revBuffer.getNumSamples() < numSamples) {
				revBuffer.setSize(numChannels, numSamples);
			}

			// Copia il segnale corrente nel buffer reverb
			for (int ch = 0; ch < numChannels; ++ch) {
				revBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
			}

			// Processa il reverb
			if (numChannels == 1) {
				reverb.processMono(revBuffer.getWritePointer(0), numSamples);
			}
			else if (numChannels == 2) {
				reverb.processStereo(revBuffer.getWritePointer(0),
					revBuffer.getWritePointer(1), numSamples);
			}

			// Mix finale: sostituisci il buffer con l'output del reverb
			// (il reverb JUCE già gestisce il mix wet/dry internamente)
			for (int ch = 0; ch < numChannels; ++ch) {
				buffer.copyFrom(ch, 0, revBuffer.getReadPointer(ch), numSamples);
			}
		}
	}



	bool EffhectorAudioProcessor::hasEditor() const
	{
		return true; // (change this to false if you choose to not supply an editor)
	}

	juce::AudioProcessorEditor* EffhectorAudioProcessor::createEditor()
	{
		return new foleys::MagicPluginEditor(magicState);
	}

	//==============================================================================
	//==============================================================================
	void EffhectorAudioProcessor::getStateInformation(juce::MemoryBlock & destData)
	{
		// Salva lo stato dei parametri
		auto state = params.copyState();
		std::unique_ptr<juce::XmlElement> xml(state.createXml());
		copyXmlToBinary(*xml, destData);

		// Salva anche lo stato della GUI Foleys se necessario
		magicState.getStateInformation(destData);
	}

	void EffhectorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
	{
		// You should use this method to restore your parameters from this memory block,
		// whose contents will have been created by the getStateInformation() call.

		// Ripristina lo stato dei parametri
		std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
		if (xmlState.get() != nullptr) {
			if (xmlState->hasTagName(params.state.getType())) {
				params.replaceState(juce::ValueTree::fromXml(*xmlState));
			}
		}

		// Ripristina lo stato della GUI Foleys se necessario
		magicState.setStateInformation(data, sizeInBytes);
	}

	//==============================================================================
	//==============================================================================
	// This creates new instances of the plugin..
	juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
	{
		return new EffhectorAudioProcessor();
	}

	//==============================================================================
	//==============================================================================
	juce::AudioProcessorValueTreeState::ParameterLayout EffhectorAudioProcessor::createParameterLayout()
	{
		std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

		//delay params
		params.push_back(std::make_unique<juce::AudioParameterFloat>(
			"DELAYMS", "Delay (ms)",
			juce::NormalisableRange<float>(0.0f, 1000.0f, 1.0f, 0.3f),
			0.0f,
			juce::AudioParameterFloatAttributes().withStringFromValueFunction(
				[](float v, int) { return juce::String(v, 1) + " ms"; }
			)));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("FEEDBACK", "Feedback", 0.0f, 0.95f, 0.3f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("DRY", "Dry", 0.1f, 1.0f, 1.0f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("WET", "Wet", 0.1f, 1.0f, 0.5f));

		//chorus params
		params.push_back(std::make_unique<juce::AudioParameterFloat>("CHORUS_RATE", "Chorus Rate", 0.01f, 5.0f, 0.3f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("CHORUS_DEPTH", "Chorus Depth", 0.1f, 10.0f, 4.0f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("CHORUS_DELAY", "Chorus Delay", 1.0f, 20.0f, 8.0f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("CHORUS_MIX", "Chorus Mix", 0.0f, 1.0f, 0.3f));

		//Reverb params
		params.push_back(std::make_unique<juce::AudioParameterFloat>("REVERB_MIX", "Reverb Mix", 0.0f, 1.0f, 0.3f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("REVERB_ROOM", "Reverb Room", 0.0f, 1.0f, 0.5f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("REVERB_DAMPING", "Reverb Damping", 0.0f, 1.0f, 0.5f));
		params.push_back(std::make_unique<juce::AudioParameterFloat>("REVERB_WIDTH", "Reverb Width", 0.0f, 1.0f, 1.0f));

		//activation params
		params.push_back(std::make_unique<juce::AudioParameterBool>("ENABLE_DELAY", "Enable Delay", false));
		params.push_back(std::make_unique<juce::AudioParameterBool>("ENABLE_CHORUS", "Enable Chorus", false));
		params.push_back(std::make_unique<juce::AudioParameterBool>("ENABLE_REVERB", "Enable Reverb", false));


		return { params.begin(), params.end() };

	}





