#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Wired — stutter / freeze effect
//  Vendor: kyoa
// ─────────────────────────────────────────────────────────────────────────────

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
    bool acceptsMidi()  const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState apvts;

    // Thread-safe reads for the editor
    float getCurrentRate() const { return currentRate; }
    bool  isEffectActive() const { return effectActive.load(); }

    // Waveform visualiser data — lock-free circular buffer
    static constexpr int VIS_SIZE = 512;
    std::array<float, VIS_SIZE> visBuffer {};
    std::atomic<int>            visWritePos { 0 };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateStutterLength();

    static constexpr int MAX_BUFFER_SECONDS = 4;
    juce::AudioBuffer<float> stutterBuffer;
    int   stutterBufferSize = 0;
    int   writePosition     = 0;
    int   readPosition      = 0;
    int   stutterLength     = 0;
    bool  isCapturing       = false;
    bool  isStuttering      = false;
    int   stutterPhase      = 0;
    int   captureCountdown  = 0;

    double currentSampleRate = 44100.0;
    float  currentRate       = 0.5f;
    std::atomic<bool> effectActive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredAudioProcessor)
};
