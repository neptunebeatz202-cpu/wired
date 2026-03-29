#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
WiredAudioProcessor::WiredAudioProcessor()
    : AudioProcessor (BusesProperties()
                    #if ! JucePlugin_IsMidiEffect
                     #if ! JucePlugin_IsSynth
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     #endif
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                    #endif
                      ),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    apvts.addParameterListener ("active", this);
}

WiredAudioProcessor::~WiredAudioProcessor()
{
    apvts.removeParameterListener ("active", this);
}

// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
WiredAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterBool> ("active", "Active", false));
    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "active")
    {
        const bool nowActive = (newValue > 0.5f);
        effectActive.store (nowActive);

        if (nowActive)
            freezeNow();
        else
        {
            isLooping = false;
            loopPhase = 0;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::setLoopMode (LoopMode m)
{
    currentLoopMode.store (m);
    if (isLooping) freezeNow();
}

// ─────────────────────────────────────────────────────────────────────────────
int WiredAudioProcessor::computeStutterLengthSamples() const
{
    if (currentLoopMode.load() == LoopMode::Infinite)
        return stutterBufferSize;

    const double samplesPerBeat = currentSampleRate * 60.0 / currentBpm;
    const double samplesPerBar  = samplesPerBeat * static_cast<double> (timeSigNumerator);

    double mult = 1.0;
    switch (currentLoopMode.load())
    {
        case LoopMode::HalfBar:  mult = 0.5; break;
        case LoopMode::TwoBars:  mult = 2.0; break;
        default: break;
    }

    return juce::jlimit (1, stutterBufferSize, juce::roundToInt (samplesPerBar * mult));
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::freezeNow()
{
    stutterLength = computeStutterLengthSamples();

    if (currentLoopMode.load() == LoopMode::Infinite)
    {
        segmentStart = 0;
        loopPhase    = 0;
        isLooping    = true;
        return;
    }

    segmentStart = (writePos - stutterLength + stutterBufferSize) % stutterBufferSize;
    loopPhase    = 0;
    isLooping    = true;
}

// ─────────────────────────────────────────────────────────────────────────────
const juce::String WiredAudioProcessor::getName() const  { return JucePlugin_Name; }
bool WiredAudioProcessor::acceptsMidi()  const           { return false; }
bool WiredAudioProcessor::producesMidi() const           { return false; }
bool WiredAudioProcessor::isMidiEffect() const           { return false; }
double WiredAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int  WiredAudioProcessor::getNumPrograms()                             { return 1; }
int  WiredAudioProcessor::getCurrentProgram()                          { return 0; }
void WiredAudioProcessor::setCurrentProgram (int)                      {}
const juce::String WiredAudioProcessor::getProgramName (int)           { return {}; }
void WiredAudioProcessor::changeProgramName (int, const juce::String&) {}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    stutterBufferSize = static_cast<int> (MAX_BUFFER_SECONDS * sampleRate);

    stutterBuffer.setSize (2, stutterBufferSize);
    stutterBuffer.clear();

    writePos     = 0;
    segmentStart = 0;
    loopPhase    = 0;
    isLooping    = false;

    visBuffer.fill (0.0f);
    visWritePos.store (0);
}

void WiredAudioProcessor::releaseResources() {}

// ─────────────────────────────────────────────────────────────────────────────
#ifndef JucePlugin_PreferredChannelConfigurations
bool WiredAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Pull BPM / time-sig from host
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue())           currentBpm = *pos->getBpm();
            if (pos->getTimeSignature().hasValue())  timeSigNumerator = pos->getTimeSignature()->numerator;
        }
    }

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (totalIn, 2);
    int       vwp         = visWritePos.load();

    for (int s = 0; s < numSamples; ++s)
    {
        // Write incoming audio to ring buffer
        for (int ch = 0; ch < numChannels; ++ch)
            stutterBuffer.setSample (ch, writePos, buffer.getSample (ch, s));

        if (effectActive.load() && isLooping)
        {
            if (currentLoopMode.load() == LoopMode::Infinite)
            {
                // Infinite: output frozen snapshot, never advance loopPhase
                for (int ch = 0; ch < numChannels; ++ch)
                    buffer.setSample (ch, s,
                        stutterBuffer.getSample (ch, (segmentStart + loopPhase) % stutterBufferSize));
            }
            else
            {
                const int readPos = (segmentStart + loopPhase) % stutterBufferSize;
                for (int ch = 0; ch < numChannels; ++ch)
                    buffer.setSample (ch, s, stutterBuffer.getSample (ch, readPos));

                if (++loopPhase >= stutterLength)
                    loopPhase = 0;
            }
        }

        // Feed visualiser with output sample (mix to mono)
        float visSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            visSample += buffer.getSample (ch, s);
        if (numChannels > 0) visSample /= static_cast<float> (numChannels);

        visBuffer[static_cast<size_t> (vwp)] = visSample;
        vwp = (vwp + 1) % VIS_SIZE;

        writePos = (writePos + 1) % stutterBufferSize;
    }

    visWritePos.store (vwp);
}

// ─────────────────────────────────────────────────────────────────────────────
bool WiredAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* WiredAudioProcessor::createEditor()
{
    return new WiredAudioProcessorEditor (*this);
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WiredAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WiredAudioProcessor();
}
