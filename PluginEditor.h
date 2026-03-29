#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Dithered, scratchy rotary knob — monochrome Lain aesthetic
// ─────────────────────────────────────────────────────────────────────────────
class WiredKnob : public juce::Slider
{
public:
    WiredKnob();
    void paint (juce::Graphics& g) override;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredKnob)
};

// ─────────────────────────────────────────────────────────────────────────────
//  CRT TV screen — plays embedded GIF with heavy analogue glitch
// ─────────────────────────────────────────────────────────────────────────────
class WiredTVScreen : public juce::Component, private juce::Timer
{
public:
    WiredTVScreen();
    ~WiredTVScreen() override;

    void paint (juce::Graphics& g) override;
    void setGifData (const void* data, size_t dataSize);
    void setGlitchIntensity (float intensity);   // 0..1

private:
    void timerCallback() override;

    juce::OwnedArray<juce::Image> frames;
    int   currentFrame  = 0;
    int   frameCounter  = 0;

    float glitchIntensity = 0.0f;
    float scanlineOffset  = 0.0f;
    int   glitchLineY     = 0;
    float glitchAlpha     = 0.0f;
    float hShift          = 0.0f;   // horizontal glitch shift
    int   corruptBlock    = 0;      // y position of corruption block
    float corruptAlpha    = 0.0f;

    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredTVScreen)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Scrolling waveform visualiser
// ─────────────────────────────────────────────────────────────────────────────
class WiredWaveform : public juce::Component, private juce::Timer
{
public:
    explicit WiredWaveform (WiredAudioProcessor& p);
    ~WiredWaveform() override;
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    WiredAudioProcessor& proc;
    std::array<float, WiredAudioProcessor::VIS_SIZE> localBuf {};
    int   scrollOffset = 0;
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredWaveform)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Ego (engage) toggle button — scratchy LED, monochrome
// ─────────────────────────────────────────────────────────────────────────────
class WiredEgoButton : public juce::ToggleButton
{
public:
    WiredEgoButton();
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredEgoButton)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Loop-mode selector button (text pill)
// ─────────────────────────────────────────────────────────────────────────────
class LoopModeButton : public juce::TextButton
{
public:
    explicit LoopModeButton (const juce::String& label);
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopModeButton)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Main plugin editor
// ─────────────────────────────────────────────────────────────────────────────
class WiredAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit WiredAudioProcessorEditor (WiredAudioProcessor&);
    ~WiredAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized ()                override;

private:
    void timerCallback() override;
    void buildWirePattern();
    void drawFilmGrain     (juce::Graphics& g);
    void drawCRTOverlay    (juce::Graphics& g);
    void drawScratchLines  (juce::Graphics& g);
    void updateLoopButtons (LoopMode selected);

    WiredAudioProcessor& audioProcessor;

    // ── Sub-components ───────────────────────────────────────────────────
    WiredTVScreen  tvScreen;
    WiredWaveform  waveform;
    WiredKnob      rateKnob;
    WiredEgoButton egoButton;

    LoopModeButton btnHalf  { "1/2" };
    LoopModeButton btnOne   { "1"   };
    LoopModeButton btnTwo   { "2"   };
    LoopModeButton btnInf   { u8"\u221E" };  // ∞

    juce::Label rateLabel;
    juce::Label egoLabel;
    juce::Label loopLabel;
    juce::Label titleLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> egoAttachment;

    // ── Animation state ──────────────────────────────────────────────────
    float        noisePhase   = 0.0f;
    int          scanlineY    = 0;
    float        flickerAlpha = 0.0f;
    float        grainSeed    = 0.0f;
    int          scratchY     = 0;
    float        scratchAlpha = 0.0f;
    juce::Random rng;

    // ── Background wire texture ──────────────────────────────────────────
    juce::Path   wirePattern;

    // ── Lain image (loaded from BinaryData) ─────────────────────────────
    juce::Image  lainImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredAudioProcessorEditor)
};
