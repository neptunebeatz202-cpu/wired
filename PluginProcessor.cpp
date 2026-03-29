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
void WiredAudioProcessor::parameterChanged (const juce::String& parameterID,
                                             float newValue)
{
    if (parameterID == "rate")
    {
        currentRate = newValue;
        updateStutterLength();
    }
    else if (parameterID == "active")
    {
        effectActive = (newValue > 0.5f);

        if (! effectActive)
        {
            // Reset stutter engine on deactivation
            isLooping    = false;
            loopPhase    = 0;
            captureCount = 0;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::updateStutterLength()
{
    // rate 0 → long loop (~800 ms)   rate 1 → short loop (~30 ms)
    const float minLen     = 0.03f;
    const float maxLen     = 0.80f;
    const float lenSeconds = minLen + (1.0f - currentRate) * (maxLen - minLen);

    stutterLength = juce::roundToInt (lenSeconds * (float) currentSampleRate);

    if (stutterBufferSize > 0)
        stutterLength = juce::jlimit (1, stutterBufferSize, stutterLength);
}

// ─────────────────────────────────────────────────────────────────────────────
const juce::String WiredAudioProcessor::getName() const  { return JucePlugin_Name; }
bool WiredAudioProcessor::acceptsMidi()  const           { return false; }
bool WiredAudioProcessor::producesMidi() const           { return false; }
bool WiredAudioProcessor::isMidiEffect() const           { return false; }
double WiredAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  WiredAudioProcessor::getNumPrograms()                              { return 1; }
int  WiredAudioProcessor::getCurrentProgram()                           { return 0; }
void WiredAudioProcessor::setCurrentProgram (int)                       {}
const juce::String WiredAudioProcessor::getProgramName (int)            { return {}; }
void WiredAudioProcessor::changeProgramName (int, const juce::String&)  {}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate  = sampleRate;
    stutterBufferSize  = static_cast<int> (MAX_BUFFER_SECONDS * sampleRate);

    stutterBuffer.setSize (2, stutterBufferSize);
    stutterBuffer.clear();

    writePos      = 0;
    segmentStart  = 0;
    loopPhase     = 0;
    captureCount  = 0;
    isLooping     = false;

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

    // Silence any unused output channels
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (! effectActive)
        return;   // pass-through

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (totalIn, 2);

    for (int s = 0; s < numSamples; ++s)
    {
        // ── Always write incoming audio into the ring buffer ─────────
        for (int ch = 0; ch < numChannels; ++ch)
            stutterBuffer.setSample (ch, writePos, buffer.getSample (ch, s));

        if (! isLooping)
        {
            // ── Capture phase: pass audio through, count samples ─────
            ++captureCount;

            if (captureCount >= stutterLength)
            {
                // We have a full segment — freeze it.
                // segmentStart is the oldest sample in the just-filled segment.
                // writePos is currently at the LAST written sample, so the
                // segment occupies [writePos - stutterLength + 1 .. writePos]
                // (with wrap-around).
                segmentStart = (writePos - stutterLength + 1 + stutterBufferSize)
                               % stutterBufferSize;

                captureCount = 0;
                loopPhase    = 0;
                isLooping    = true;
            }
        }
        else
        {
            // ── Loop phase: read from the frozen segment ─────────────
            const int readPos = (segmentStart + loopPhase) % stutterBufferSize;

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, s, stutterBuffer.getSample (ch, readPos));

            ++loopPhase;

            if (loopPhase >= stutterLength)
                loopPhase = 0;   // loop back to segment start
        }

        writePos = (writePos + 1) % stutterBufferSize;
    }
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
