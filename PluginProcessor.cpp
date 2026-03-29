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
    apvts.addParameterListener ("rate",   this);
    apvts.addParameterListener ("active", this);
}

WiredAudioProcessor::~WiredAudioProcessor()
{
    apvts.removeParameterListener ("rate",   this);
    apvts.removeParameterListener ("active", this);
}

// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
WiredAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "rate", "Stutter Rate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.5f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "active", "Active", false));

    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "rate")
    {
        currentRate = newValue;
        updateStutterLength();
    }
    else if (parameterID == "active")
    {
        const bool nowActive = (newValue > 0.5f);
        effectActive.store (nowActive);
        if (! nowActive)
        {
            isStuttering   = false;
            isCapturing    = false;
            stutterPhase   = 0;
            captureCountdown = 0;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::updateStutterLength()
{
    const float minLen = 0.03f;
    const float maxLen = 0.8f;
    const float lenSeconds = minLen + (1.0f - currentRate) * (maxLen - minLen);
    stutterLength = juce::jlimit (1, stutterBufferSize,
                                  juce::roundToInt (lenSeconds * (float) currentSampleRate));
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

    writePosition    = 0;
    readPosition     = 0;
    stutterPhase     = 0;
    captureCountdown = 0;
    isCapturing      = false;
    isStuttering     = false;

    visBuffer.fill (0.0f);
    visWritePos.store (0);

    updateStutterLength();
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

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (totalIn, 2);
    int       vwp         = visWritePos.load();

    if (! effectActive.load())
    {
        // Pass-through — still feed the visualiser
        for (int s = 0; s < numSamples; ++s)
        {
            float vis = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                vis += buffer.getSample (ch, s);
            if (numChannels > 0) vis /= (float) numChannels;
            visBuffer[static_cast<size_t> (vwp)] = vis;
            vwp = (vwp + 1) % VIS_SIZE;
        }
        visWritePos.store (vwp);
        return;
    }

    for (int s = 0; s < numSamples; ++s)
    {
        // Write to ring buffer
        for (int ch = 0; ch < numChannels; ++ch)
            stutterBuffer.setSample (ch, writePosition, buffer.getSample (ch, s));

        if (! isStuttering)
        {
            captureCountdown++;
            if (captureCountdown >= stutterLength)
            {
                captureCountdown = 0;
                isStuttering     = true;
                stutterPhase     = 0;
                readPosition     = (writePosition - stutterLength + stutterBufferSize)
                                    % stutterBufferSize;
            }
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, s, stutterBuffer.getSample (ch, readPosition));

            readPosition = (readPosition + 1) % stutterBufferSize;
            stutterPhase++;

            if (stutterPhase >= stutterLength)
            {
                stutterPhase = 0;
                readPosition = (readPosition - stutterLength + stutterBufferSize)
                                % stutterBufferSize;
            }
        }

        writePosition = (writePosition + 1) % stutterBufferSize;

        // Feed visualiser
        float vis = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            vis += buffer.getSample (ch, s);
        if (numChannels > 0) vis /= (float) numChannels;
        visBuffer[static_cast<size_t> (vwp)] = vis;
        vwp = (vwp + 1) % VIS_SIZE;
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
