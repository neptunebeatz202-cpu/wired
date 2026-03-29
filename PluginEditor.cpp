#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────
//  Colour palette  (Serial Experiments Lain)
// ─────────────────────────────────────────────
namespace WiredColors
{
    const juce::Colour bg          { 0xFF050810 };   // near-black blue
    const juce::Colour bgMid       { 0xFF0A1020 };
    const juce::Colour wireGrey    { 0xFF1A2235 };
    const juce::Colour crtGreen    { 0xFF00FF9C };   // phosphor green
    const juce::Colour crtCyan     { 0xFF00D4FF };
    const juce::Colour glitchRed   { 0xFFFF2020 };
    const juce::Colour textDim     { 0xFF406050 };
}

// ═════════════════════════════════════════════
//  WiredKnob
// ═════════════════════════════════════════════
WiredKnob::WiredKnob()
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRange (0.0, 1.0, 0.001);
    setValue (0.5, juce::dontSendNotification);
}

void WiredKnob::paint (juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat().reduced (4.0f);
    const float cx     = bounds.getCentreX();
    const float cy     = bounds.getCentreY();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // Outer glow
    {
        juce::ColourGradient glow (WiredColors::crtGreen.withAlpha (0.25f), cx, cy,
                                    WiredColors::crtGreen.withAlpha (0.0f),
                                    cx + radius * 1.3f, cy, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - radius * 1.3f, cy - radius * 1.3f,
                       radius * 2.6f, radius * 2.6f);
    }

    // Track arc
    const float startAngle = juce::MathConstants<float>::pi * 1.25f;
    const float endAngle   = juce::MathConstants<float>::pi * 2.75f;

    {
        juce::Path track;
        track.addCentredArc (cx, cy, radius * 0.85f, radius * 0.85f,
                              0.0f, startAngle, endAngle, true);
        g.setColour (WiredColors::wireGrey);
        g.strokePath (track, juce::PathStrokeType (2.5f));
    }

    // Value arc
    const float normVal   = static_cast<float> (
        (getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    const float valAngle  = startAngle + normVal * (endAngle - startAngle);

    {
        juce::Path val;
        val.addCentredArc (cx, cy, radius * 0.85f, radius * 0.85f,
                            0.0f, startAngle, valAngle, true);
        g.setColour (WiredColors::crtGreen);
        g.strokePath (val, juce::PathStrokeType (2.5f));
    }

    // Body
    {
        juce::ColourGradient body (WiredColors::bgMid,
                                    cx - radius * 0.3f, cy - radius * 0.3f,
                                    WiredColors::bg,
                                    cx + radius * 0.5f, cy + radius * 0.5f, true);
        g.setGradientFill (body);
        g.fillEllipse (cx - radius * 0.7f, cy - radius * 0.7f,
                       radius * 1.4f, radius * 1.4f);
    }

    g.setColour (WiredColors::crtGreen.withAlpha (0.5f));
    g.drawEllipse (cx - radius * 0.7f, cy - radius * 0.7f,
                   radius * 1.4f, radius * 1.4f, 1.5f);

    // Pointer
    const float pLen = radius * 0.55f;
    g.setColour (WiredColors::crtGreen);
    g.drawLine (cx, cy,
                cx + pLen * std::sin (valAngle),
                cy - pLen * std::cos (valAngle), 2.0f);

    // Centre dot
    g.fillEllipse (cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);

    // Tick marks
    g.setColour (WiredColors::textDim);
    for (int i = 0; i <= 10; ++i)
    {
        const float t     = static_cast<float> (i) / 10.0f;
        const float a     = startAngle + t * (endAngle - startAngle);
        const float inner = radius * 0.88f;
        const float outer = radius * 0.97f;
        const float lw    = (i == 0 || i == 5 || i == 10) ? 1.5f : 0.7f;
        g.drawLine (cx + inner * std::sin (a), cy - inner * std::cos (a),
                    cx + outer * std::sin (a), cy - outer * std::cos (a), lw);
    }
}

// ═════════════════════════════════════════════
//  WiredTVScreen
// ═════════════════════════════════════════════
WiredTVScreen::WiredTVScreen()
{
    startTimerHz (12);   // ~12 fps — low-fi feel
}

WiredTVScreen::~WiredTVScreen()
{
    stopTimer();
}

void WiredTVScreen::setGifData (const void* data, size_t dataSize)
{
    frames.clear();
    currentFrame = 0;
    frameCounter = 0;

    if (data == nullptr || dataSize == 0)
        return;

    // JUCE's GIFImageFormat can decode individual frames via
    // decodeImage() when called on a fresh stream each time,
    // but it only returns the first frame per call on most builds.
    // The reliable approach is to decode via the raw GIF frame blocks.
    // We load the first frame and fall back gracefully if the format
    // doesn't support multi-frame iteration (JUCE 7 limitation).
    juce::MemoryInputStream stream (data, dataSize, false);
    juce::GIFImageFormat    gif;

    if (! gif.canUnderstand (stream))
        return;

    // Attempt to read as many frames as the format will yield.
    // On JUCE 7 this typically yields exactly one composite frame.
    stream.setPosition (0);
    auto img = gif.decodeImage (stream);

    if (img.isValid())
        frames.add (new juce::Image (img));

    // If only one frame was decoded the timer still drives
    // the scanline / glitch animation independently.
}

void WiredTVScreen::setGlitchIntensity (float intensity)
{
    glitchIntensity = juce::jlimit (0.0f, 1.0f, intensity);
}

void WiredTVScreen::timerCallback()
{
    // Advance GIF frame
    if (frames.size() > 1)
    {
        ++frameCounter;
        if (frameCounter >= 3)
        {
            frameCounter = 0;
            currentFrame = (currentFrame + 1) % frames.size();
        }
    }

    // Scroll the moving scanline band
    scanlineOffset += 0.8f;
    if (scanlineOffset >= static_cast<float> (getHeight()))
        scanlineOffset -= static_cast<float> (juce::jmax (1, getHeight()));

    // Random glitch line
    const int h = getHeight();
    if (h > 1 && rng.nextFloat() < 0.15f + glitchIntensity * 0.5f)
    {
        glitchLineY = rng.nextInt (h);
        glitchAlpha = 0.6f + rng.nextFloat() * 0.4f;
    }
    else
    {
        glitchAlpha *= 0.7f;
    }

    repaint();
}

void WiredTVScreen::paint (juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const auto  screen = bounds.reduced (6.0f);

    // ── Bezel ──────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xFF080E1A));
    g.fillRoundedRectangle (bounds, 6.0f);

    // ── Screen background ──────────────────────────────────────────────
    {
        juce::ColourGradient bg (juce::Colour (0xFF000A14),
                                  screen.getCentreX(), screen.getCentreY(),
                                  juce::Colour (0xFF001020),
                                  screen.getRight(), screen.getBottom(), true);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (screen, 3.0f);
    }

    g.saveState();
    g.reduceClipRegion (screen.toNearestInt());

    // ── GIF frame (or fallback) ────────────────────────────────────────
    if (frames.size() > 0)
    {
        const juce::Image& frame = *frames[currentFrame];
        g.drawImage (frame,
                     (int) screen.getX(),    (int) screen.getY(),
                     (int) screen.getWidth(), (int) screen.getHeight(),
                     0, 0, frame.getWidth(), frame.getHeight());

        // Phosphor cyan tint
        g.setColour (juce::Colour (0x2200D4FF));
        g.fillRoundedRectangle (screen, 3.0f);
    }
    else
    {
        // Fallback: minimal glyph eyes + flavour text
        const float eyeW = screen.getWidth()  * 0.14f;
        const float eyeH = screen.getHeight() * 0.12f;
        const float eyeY = screen.getCentreY() - screen.getHeight() * 0.05f;
        const float eyeSpacing = screen.getWidth() * 0.18f;

        for (int side = -1; side <= 1; side += 2)
        {
            const float ex = screen.getCentreX()
                             + static_cast<float> (side) * eyeSpacing
                             - eyeW * 0.5f;
            juce::ColourGradient eyeGrad (
                WiredColors::crtCyan.withAlpha (0.9f),
                ex + eyeW * 0.5f, eyeY,
                WiredColors::crtCyan.withAlpha (0.0f),
                ex + eyeW * 0.5f, eyeY + eyeH * 0.8f, false);
            g.setGradientFill (eyeGrad);
            g.fillEllipse (ex, eyeY, eyeW, eyeH);
        }

        g.setColour (WiredColors::crtCyan.withAlpha (0.5f));
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
        g.drawText ("present day", screen.toNearestInt(), juce::Justification::centred);
    }

    // ── Scanlines ──────────────────────────────────────────────────────
    g.setColour (juce::Colour (0x22000000));
    for (float y = screen.getY(); y < screen.getBottom(); y += 2.5f)
        g.drawHorizontalLine (static_cast<int> (y),
                               screen.getX(), screen.getRight());

    // ── Moving scanline band ───────────────────────────────────────────
    {
        float sy = screen.getY() + scanlineOffset;
        if (sy > screen.getBottom())
            sy -= screen.getHeight();

        juce::ColourGradient scan (
            WiredColors::crtCyan.withAlpha (0.08f), screen.getX(), sy,
            WiredColors::crtCyan.withAlpha (0.0f),  screen.getX(), sy + 20.0f, false);
        g.setGradientFill (scan);
        g.fillRect (screen.getX(), sy, screen.getWidth(), 20.0f);
    }

    // ── Glitch line ────────────────────────────────────────────────────
    if (glitchAlpha > 0.02f && getHeight() > 0)
    {
        const float gy    = screen.getY()
                            + static_cast<float> (glitchLineY % juce::jmax (1, static_cast<int> (screen.getHeight())));
        const float shiftX = rng.nextFloat() * screen.getWidth() * 0.3f;
        const float segW   = screen.getWidth() * (0.3f + rng.nextFloat() * 0.6f);

        g.setColour (WiredColors::crtCyan.withAlpha (glitchAlpha * 0.7f));
        g.fillRect (screen.getX() + shiftX, gy, segW, 1.5f);

        // Chromatic aberration offset
        g.setColour (WiredColors::glitchRed.withAlpha (glitchAlpha * 0.3f));
        g.fillRect (screen.getX() + shiftX + 2.0f, gy + 0.5f, segW * 0.4f, 1.0f);
    }

    // ── Vignette ──────────────────────────────────────────────────────
    {
        juce::ColourGradient vig (
            juce::Colour (0x00000000), screen.getCentreX(), screen.getCentreY(),
            juce::Colour (0xAA000000), screen.getX(), screen.getY(), true);
        g.setGradientFill (vig);
        g.fillRoundedRectangle (screen, 3.0f);
    }

    g.restoreState();

    // ── Bezel inner glow ──────────────────────────────────────────────
    g.setColour (WiredColors::crtCyan.withAlpha (0.15f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 1.5f);
}

// ═════════════════════════════════════════════
//  WiredToggleButton
// ═════════════════════════════════════════════
WiredToggleButton::WiredToggleButton()
{
    setButtonText ("");
}

void WiredToggleButton::paintButton (juce::Graphics& g, bool highlighted, bool /*down*/)
{
    const auto  bounds = getLocalBounds().toFloat().reduced (3.0f);
    const bool  on     = getToggleState();

    // Outer glow when active
    if (on)
    {
        juce::ColourGradient glow (
            WiredColors::crtGreen.withAlpha (0.3f),
            bounds.getCentreX(), bounds.getCentreY(),
            WiredColors::crtGreen.withAlpha (0.0f),
            bounds.getRight() + 12.0f, bounds.getCentreY(), true);
        g.setGradientFill (glow);
        g.fillEllipse (bounds.expanded (10.0f));
    }

    // Body
    {
        juce::ColourGradient body (
            on ? juce::Colour (0xFF0A2018) : juce::Colour (0xFF090F18),
            bounds.getX(), bounds.getY(),
            on ? juce::Colour (0xFF041510) : juce::Colour (0xFF05080F),
            bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (body);
        g.fillEllipse (bounds);
    }

    g.setColour (on ? WiredColors::crtGreen
                    : WiredColors::wireGrey.brighter (0.2f));
    g.drawEllipse (bounds, on ? 1.5f : 1.0f);

    // LED dot
    const float dotR = bounds.getWidth() * 0.22f;
    const float dotX = bounds.getCentreX() - dotR;
    const float dotY = bounds.getCentreY() - dotR;

    if (on)
    {
        juce::ColourGradient led (
            WiredColors::crtGreen, bounds.getCentreX(), bounds.getCentreY(),
            WiredColors::crtGreen.withAlpha (0.0f),
            bounds.getCentreX() + dotR, bounds.getCentreY(), true);
        g.setGradientFill (led);
        g.fillEllipse (dotX, dotY, dotR * 2.0f, dotR * 2.0f);
    }
    else
    {
        g.setColour (WiredColors::wireGrey);
        g.fillEllipse (dotX, dotY, dotR * 2.0f, dotR * 2.0f);
    }

    if (highlighted)
    {
        g.setColour (juce::Colour (0x15FFFFFF));
        g.fillEllipse (bounds);
    }
}

// ═════════════════════════════════════════════
//  WiredAudioProcessorEditor
// ═════════════════════════════════════════════
WiredAudioProcessorEditor::WiredAudioProcessorEditor (WiredAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (420, 560);

    // Title
    titleLabel.setText ("WIRED//STUTTER", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(),
                                    14.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, WiredColors::crtGreen);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    // TV Screen
    addAndMakeVisible (tvScreen);

    // Rate knob
    addAndMakeVisible (rateKnob);
    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "rate", rateKnob);

    rateLabel.setText ("RATE", juce::dontSendNotification);
    rateLabel.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(),
                                    10.0f, juce::Font::plain));
    rateLabel.setColour (juce::Label::textColourId, WiredColors::textDim);
    rateLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rateLabel);

    // Active toggle
    addAndMakeVisible (activeButton);
    activeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "active", activeButton);

    activeLabel.setText ("ENGAGE", juce::dontSendNotification);
    activeLabel.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(),
                                      10.0f, juce::Font::plain));
    activeLabel.setColour (juce::Label::textColourId, WiredColors::textDim);
    activeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (activeLabel);

    buildWirePattern();
    startTimerHz (30);
}

WiredAudioProcessorEditor::~WiredAudioProcessorEditor()
{
    stopTimer();
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::buildWirePattern()
{
    wirePattern.clear();
    juce::Random r (42);
    const float W = 420.0f;
    const float H = 560.0f;

    // Curved cable paths
    for (int i = 0; i < 55; ++i)
    {
        const float x1 = r.nextFloat() * W;
        const float y1 = r.nextFloat() * H;
        const float x2 = r.nextFloat() * W;
        const float y2 = r.nextFloat() * H;
        const float cpx = (x1 + x2) * 0.5f + (r.nextFloat() - 0.5f) * 80.0f;
        const float cpy = (y1 + y2) * 0.5f + (r.nextFloat() - 0.5f) * 80.0f;
        wirePattern.startNewSubPath (x1, y1);
        wirePattern.quadraticTo (cpx, cpy, x2, y2);
    }

    // Horizontal cable runs
    for (int i = 0; i < 15; ++i)
    {
        const float y = r.nextFloat() * H;
        wirePattern.startNewSubPath (0.0f, y);
        wirePattern.lineTo (W, y + (r.nextFloat() - 0.5f) * 30.0f);
    }
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::timerCallback()
{
    noisePhase += 0.04f;
    scanlineY   = (scanlineY + 2) % juce::jmax (1, getHeight());

    if (rng.nextFloat() < 0.06f)
        flickerAlpha = 0.03f + rng.nextFloat() * 0.04f;
    else
        flickerAlpha *= 0.85f;

    tvScreen.setGlitchIntensity (audioProcessor.isEffectActive() ? 0.6f : 0.1f);

    repaint();
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::drawCRTOverlay (juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const float W      = bounds.getWidth();

    // Full-screen scanlines
    g.setColour (juce::Colour (0x09000000));
    for (float y = 0.0f; y < bounds.getHeight(); y += 3.0f)
        g.drawHorizontalLine (static_cast<int> (y), 0.0f, W);

    // Moving band
    {
        const float sy = static_cast<float> (scanlineY);
        juce::ColourGradient sg (
            WiredColors::crtGreen.withAlpha (0.04f), 0.0f, sy,
            WiredColors::crtGreen.withAlpha (0.0f),  0.0f, sy + 40.0f, false);
        g.setGradientFill (sg);
        g.fillRect (0.0f, sy, W, 40.0f);
    }

    // Corner vignette
    {
        juce::ColourGradient vig (
            juce::Colour (0x00000000), bounds.getCentreX(), bounds.getCentreY(),
            juce::Colour (0x88000014), 0.0f, 0.0f, true);
        g.setGradientFill (vig);
        g.fillRect (bounds);
    }

    // Global flicker
    if (flickerAlpha > 0.005f)
    {
        g.setColour (juce::Colour (0xFF00FF9C).withAlpha (flickerAlpha));
        g.fillRect (bounds);
    }
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::drawNoise (juce::Graphics& g)
{
    g.setColour (WiredColors::crtGreen.withAlpha (0.04f));
    juce::Random noiseRng (static_cast<juce::int64> (noisePhase * 1000.0f));
    const int W = getWidth();
    const int H = getHeight();
    if (W <= 0 || H <= 0) return;

    for (int i = 0; i < 200; ++i)
        g.fillRect (noiseRng.nextInt (W), noiseRng.nextInt (H), 1, 1);
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const float W      = bounds.getWidth();
    const float H      = bounds.getHeight();

    // ── Background gradient ───────────────────────────────────────────
    {
        juce::ColourGradient bg (WiredColors::bg, 0.0f, 0.0f,
                                  juce::Colour (0xFF030610), W, H, false);
        g.setGradientFill (bg);
        g.fillRect (bounds);
    }

    // ── Wire texture ──────────────────────────────────────────────────
    g.setColour (WiredColors::wireGrey.withAlpha (0.18f));
    g.strokePath (wirePattern, juce::PathStrokeType (0.5f));

    // ── Grid ──────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xFF0A1825).withAlpha (0.5f));
    for (float y = 0.0f; y < H; y += 20.0f)
        g.drawHorizontalLine (static_cast<int> (y), 0.0f, W);
    for (float x = 0.0f; x < W; x += 20.0f)
        g.drawVerticalLine (static_cast<int> (x), 0.0f, H);

    // ── Top bar ───────────────────────────────────────────────────────
    {
        juce::ColourGradient bar (juce::Colour (0xFF0A1A28), 0.0f,  0.0f,
                                   juce::Colour (0xFF050E18), W,    44.0f, false);
        g.setGradientFill (bar);
        g.fillRect (0.0f, 0.0f, W, 44.0f);

        g.setColour (WiredColors::crtGreen.withAlpha (0.4f));
        g.drawHorizontalLine (44, 0.0f, W);

        // Pulsing status dots
        for (int i = 0; i < 5; ++i)
        {
            const float alpha = audioProcessor.isEffectActive()
                ? 0.3f + 0.5f * std::sin (noisePhase * 3.0f + static_cast<float> (i) * 1.2f)
                : 0.15f;
            g.setColour (WiredColors::crtGreen.withAlpha (alpha));
            g.fillEllipse (8.0f + static_cast<float> (i) * 12.0f, 16.0f, 6.0f, 6.0f);
        }
    }

    // ── Bottom bar ────────────────────────────────────────────────────
    {
        juce::ColourGradient bar (juce::Colour (0xFF050E18), 0.0f, H - 40.0f,
                                   juce::Colour (0xFF0A1A28), W,   H, false);
        g.setGradientFill (bar);
        g.fillRect (0.0f, H - 40.0f, W, 40.0f);

        g.setColour (WiredColors::crtGreen.withAlpha (0.25f));
        g.drawHorizontalLine (static_cast<int> (H) - 40, 0.0f, W);

        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(),
                                8.0f, juce::Font::plain));
        g.setColour (WiredColors::textDim.withAlpha (0.6f));
        g.drawText ("layer 07 :: WIRED",
                    8, static_cast<int> (H) - 30, 200, 20,
                    juce::Justification::left);
        g.drawText ("v1.0.0",
                    static_cast<int> (W) - 50, static_cast<int> (H) - 30, 42, 20,
                    juce::Justification::right);
    }

    // ── Separator ─────────────────────────────────────────────────────
    g.setColour (WiredColors::wireGrey.withAlpha (0.5f));
    g.drawHorizontalLine (380, 30.0f, W - 30.0f);

    // ── Corner brackets around TV ─────────────────────────────────────
    auto drawBracket = [&] (float x, float y, bool flipX, bool flipY,
                             float size = 12.0f)
    {
        const float sx = flipX ? -1.0f : 1.0f;
        const float sy = flipY ? -1.0f : 1.0f;
        g.drawLine (x, y, x + sx * size, y,      1.5f);
        g.drawLine (x, y, x,             y + sy * size, 1.5f);
    };

    g.setColour (WiredColors::crtGreen.withAlpha (0.5f));
    const auto tv = tvScreen.getBounds().toFloat().expanded (4.0f);
    drawBracket (tv.getX(),     tv.getY(),      false, false);
    drawBracket (tv.getRight(), tv.getY(),      true,  false);
    drawBracket (tv.getX(),     tv.getBottom(), false, true);
    drawBracket (tv.getRight(), tv.getBottom(), true,  true);

    // ── CRT overlays (drawn last) ─────────────────────────────────────
    drawNoise (g);
    drawCRTOverlay (g);
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::resized()
{
    const int W = getWidth();

    titleLabel.setBounds (0, 10, W, 24);

    // TV screen
    constexpr int tvW = 320, tvH = 240;
    tvScreen.setBounds ((W - tvW) / 2, 54, tvW, tvH);

    // Controls
    constexpr int ctrlY    = 320;
    constexpr int knobSize = 90;
    constexpr int btnSize  = 60;

    const int knobX = W / 2 - 80 - knobSize / 2;
    rateKnob.setBounds  (knobX, ctrlY, knobSize, knobSize);
    rateLabel.setBounds (knobX, ctrlY + knobSize + 2, knobSize, 16);

    const int btnX = W / 2 + 80 - btnSize / 2;
    activeButton.setBounds (btnX, ctrlY + 15, btnSize, btnSize);
    activeLabel.setBounds  (btnX, ctrlY + btnSize + 17, btnSize, 16);
}
