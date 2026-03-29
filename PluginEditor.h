#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ─────────────────────────────────────────────
//  Custom Wired/Lain-aesthetic rotary knob
// ─────────────────────────────────────────────
class WiredKnob : public juce::Slider
{
public:
    WiredKnob();
    void paint (juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredKnob)
};

// ─────────────────────────────────────────────
//  CRT TV screen — animates the embedded GIF
//  with scanlines, glitch lines, and phosphor glow
// ─────────────────────────────────────────────
class WiredTVScreen : public juce::Component,
                      private juce::Timer
{
public:
    WiredTVScreen();
    ~WiredTVScreen() override;

    void paint (juce::Graphics& g) override;

    // Feed the raw GIF bytes from BinaryData
    void setGifData (const void* data, size_t dataSize);

    // 0 = calm,  1 = heavy glitch (driven by effect state)
    void setGlitchIntensity (float intensity);

private:
    void timerCallback() override;

    // Decoded frames (JUCE decodes all GIF frames via GIFImageFormat)
    juce::OwnedArray<juce::Image> frames;
    int   currentFrame  = 0;
    int   frameCounter  = 0;   // ticks between frame advances

    float glitchIntensity = 0.0f;
    float scanlineOffset  = 0.0f;
    int   glitchLineY     = 0;
    float glitchAlpha     = 0.0f;

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredTVScreen)
};

// ─────────────────────────────────────────────
//  LED toggle button — glows green when active
// ─────────────────────────────────────────────
class WiredToggleButton : public juce::ToggleButton
{
public:
    WiredToggleButton();
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredToggleButton)
};

// ─────────────────────────────────────────────
//  Main plugin editor
// ─────────────────────────────────────────────
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
    void drawCRTOverlay (juce::Graphics& g);
    void drawNoise      (juce::Graphics& g);
    void buildWirePattern();

    WiredAudioProcessor& audioProcessor;

    WiredTVScreen     tvScreen;
    WiredKnob         rateKnob;
    WiredToggleButton activeButton;

    juce::Label rateLabel;
    juce::Label activeLabel;
    juce::Label titleLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttachment;

    // Animation state
    float        noisePhase  = 0.0f;
    int          scanlineY   = 0;
    float        flickerAlpha = 0.0f;
    juce::Random rng;

    // Background wire texture (procedural)
    juce::Path wirePattern;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WiredAudioProcessorEditor)
};
