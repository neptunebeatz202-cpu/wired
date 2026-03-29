#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

class WiredAudioProcessor : public juce::AudioProcessor,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    WiredAudioProcessor();
    ~WiredAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState apvts;

    // Public getters for the editor
    bool isEffectActive() const { return effectActive; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateStutterLength();

    // ── Stutter ring-buffer ────────────────────────────────────────────
    static constexpr int MAX_BUFFER_SECONDS = 4;

    juce::AudioBuffer<float> stutterBuffer;
    int  stutterBufferSize = 0;

    // Absolute write head — always advances
    int  writePos      = 0;

    // The sample index (into stutterBuffer) where the frozen segment begins
    int  segmentStart  = 0;

    // How many samples into the current loop playback we are
    int  loopPhase     = 0;

    // Length of the frozen segment in samples
    int  stutterLength = 0;

    // How many samples we have accumulated since last freeze trigger
    int  captureCount  = 0;

    // True once we have captured enough samples and are now looping
    bool isLooping     = false;

    double currentSampleRate = 44100.0;
    float  currentRate       = 0.5f;
    bool   effectActive      = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredAudioProcessor)
};
