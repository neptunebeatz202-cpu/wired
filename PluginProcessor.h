#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Loop rotation modes
//  HalfBar / OneBar / TwoBars:  after N bars the frozen segment advances
//  Infinite:                    the segment is frozen forever until deactivated
// ─────────────────────────────────────────────────────────────────────────────
enum class LoopMode { HalfBar = 0, OneBar, TwoBars, Infinite };

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

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState apvts;

    // Thread-safe reads for the editor
    bool     isEffectActive() const { return effectActive.load(); }
    LoopMode getLoopMode()    const { return currentLoopMode.load(); }

    // Called from the UI thread (editor button clicks)
    void setLoopMode (LoopMode m);

    // Waveform data for the visualiser — lock-free circular copy
    static constexpr int VIS_SIZE = 512;
    std::array<float, VIS_SIZE> visBuffer {};
    std::atomic<int>            visWritePos { 0 };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void freezeNow();
    int  computeStutterLengthSamples() const;

    // ── Ring buffer ──────────────────────────────────────────────────────
    static constexpr int MAX_BUFFER_SECONDS = 8;
    juce::AudioBuffer<float> stutterBuffer;
    int stutterBufferSize = 0;
    int writePos          = 0;
    int segmentStart      = 0;
    int loopPhase         = 0;
    int stutterLength     = 0;

    // ── State ────────────────────────────────────────────────────────────
    std::atomic<bool>     effectActive    { false };
    std::atomic<LoopMode> currentLoopMode { LoopMode::OneBar };
    bool isLooping = false;

    // ── Timing ───────────────────────────────────────────────────────────
    double currentSampleRate = 44100.0;
    double currentBpm        = 120.0;
    int    timeSigNumerator  = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredAudioProcessor)
};
