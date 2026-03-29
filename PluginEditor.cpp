#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Monochrome palette — harsh, industrial, broken
//  Every panel has a 3D extrusion: lighter top-left edge, black bottom-right shadow
// ─────────────────────────────────────────────────────────────────────────────
namespace WC
{
    const juce::Colour black   { 0xFF000000 };
    const juce::Colour nearBlk { 0xFF050505 };
    const juce::Colour dark    { 0xFF0C0C0C };
    const juce::Colour panelDk { 0xFF141414 };
    const juce::Colour panelMd { 0xFF1E1E1E };
    const juce::Colour panelHi { 0xFF2E2E2E };
    const juce::Colour mid     { 0xFF2A2A2A };
    const juce::Colour dimGrey { 0xFF555555 };
    const juce::Colour grey    { 0xFF888888 };
    const juce::Colour light   { 0xFFBBBBBB };
    const juce::Colour white   { 0xFFFFFFFF };
    const juce::Colour offWhite{ 0xFFDDDDDD };

    // ── 3D panel helpers ──────────────────────────────────────────────────
    inline void drawRaisedPanel (juce::Graphics& g, juce::Rectangle<float> r, float depth = 3.0f)
    {
        g.setColour (panelMd);
        g.fillRect (r);
        g.setColour (panelHi);
        g.fillRect (r.getX(), r.getY(), r.getWidth(), depth);
        g.fillRect (r.getX(), r.getY(), depth, r.getHeight());
        g.setColour (black);
        g.fillRect (r.getX(), r.getBottom() - depth, r.getWidth(), depth);
        g.fillRect (r.getRight() - depth, r.getY(), depth, r.getHeight());
        g.setColour (juce::Colour (0xFF3A3A3A));
        g.drawRect (r, 1.0f);
    }

    inline void drawSunkenPanel (juce::Graphics& g, juce::Rectangle<float> r, float depth = 2.0f)
    {
        g.setColour (nearBlk);
        g.fillRect (r);
        g.setColour (black);
        g.fillRect (r.getX(), r.getY(), r.getWidth(), depth);
        g.fillRect (r.getX(), r.getY(), depth, r.getHeight());
        g.setColour (panelHi);
        g.fillRect (r.getX(), r.getBottom() - depth, r.getWidth(), depth);
        g.fillRect (r.getRight() - depth, r.getY(), depth, r.getHeight());
        g.setColour (juce::Colour (0xFF222222));
        g.drawRect (r, 1.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
static juce::Font monoFont (float size, int style = juce::Font::plain)
{
    return juce::Font (juce::Font::getDefaultMonospacedFontName(), size, style);
}

// ═════════════════════════════════════════════════════════════════════════════
//  WiredKnob — raised 3D dome, Mishby extruded style
// ═════════════════════════════════════════════════════════════════════════════
WiredKnob::WiredKnob()
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRange (0.0, 1.0, 0.001);
    setValue (0.5, juce::dontSendNotification);
}

void WiredKnob::paint (juce::Graphics& g)
{
    const auto  b      = getLocalBounds().toFloat().reduced (6.0f);
    const float cx     = b.getCentreX();
    const float cy     = b.getCentreY();
    const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

    const float startA = juce::MathConstants<float>::pi * 1.25f;
    const float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float normV  = static_cast<float> ((getValue() - getMinimum())
                                              / (getMaximum() - getMinimum()));
    const float valA   = startA + normV * (endA - startA);

    // ── Drop shadow ───────────────────────────────────────────────────────
    g.setColour (WC::black.withAlpha (0.85f));
    g.fillEllipse (cx - radius + 3.5f, cy - radius + 3.5f, radius * 2.0f, radius * 2.0f);

    // ── Outer body — gradient dome ────────────────────────────────────────
    {
        juce::ColourGradient body (WC::panelHi, cx - radius * 0.3f, cy - radius * 0.3f,
                                    WC::nearBlk, cx + radius * 0.6f, cy + radius * 0.6f, true);
        g.setGradientFill (body);
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }

    // ── Track ─────────────────────────────────────────────────────────────
    {
        juce::Path track;
        track.addCentredArc (cx, cy, radius * 0.82f, radius * 0.82f, 0.0f, startA, endA, true);
        g.setColour (WC::mid);
        g.strokePath (track, juce::PathStrokeType (2.5f));
    }

    // ── Value arc — dashed white ──────────────────────────────────────────
    {
        juce::Path val;
        val.addCentredArc (cx, cy, radius * 0.82f, radius * 0.82f, 0.0f, startA, valA, true);
        float dashes[] = { 5.0f, 2.5f };
        juce::PathStrokeType pst (2.5f);
        pst.createDashedStroke (val, val, dashes, 2);
        g.setColour (WC::white);
        g.strokePath (val, juce::PathStrokeType (2.5f));
    }

    // ── Outer highlight rim ───────────────────────────────────────────────
    {
        juce::ColourGradient rim (WC::light.withAlpha (0.6f), cx - radius, cy - radius,
                                   WC::black.withAlpha (0.8f), cx + radius, cy + radius, false);
        g.setGradientFill (rim);
        g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    }

    // ── Inner cap — raised dome ───────────────────────────────────────────
    {
        juce::ColourGradient cap (WC::panelHi, cx - radius * 0.35f, cy - radius * 0.35f,
                                   WC::nearBlk, cx + radius * 0.4f,  cy + radius * 0.4f, true);
        g.setGradientFill (cap);
        g.fillEllipse (cx - radius * 0.62f, cy - radius * 0.62f, radius * 1.24f, radius * 1.24f);
    }
    g.setColour (WC::dimGrey.withAlpha (0.6f));
    g.drawEllipse (cx - radius * 0.62f, cy - radius * 0.62f, radius * 1.24f, radius * 1.24f, 1.0f);

    // ── Pointer ───────────────────────────────────────────────────────────
    g.setColour (WC::white);
    const float pLen = radius * 0.50f;
    g.drawLine (cx, cy, cx + pLen * std::sin (valA), cy - pLen * std::cos (valA), 2.0f);

    // ── Centre screw ──────────────────────────────────────────────────────
    g.setColour (WC::grey);
    g.fillEllipse (cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    g.setColour (WC::black);
    g.drawLine (cx - 1.5f, cy, cx + 1.5f, cy, 0.8f);

    // ── Tick marks ────────────────────────────────────────────────────────
    juce::Random tickRng (77);
    for (int i = 0; i <= 10; ++i)
    {
        const float t = static_cast<float> (i) / 10.0f;
        const float a = startA + t * (endA - startA);
        const float inner = radius * 0.88f;
        const float outer = radius * (0.96f + tickRng.nextFloat() * 0.01f);
        g.setColour (i % 5 == 0 ? WC::grey : WC::mid);
        g.drawLine (cx + inner * std::sin (a), cy - inner * std::cos (a),
                    cx + outer * std::sin (a), cy - outer * std::cos (a),
                    i % 5 == 0 ? 1.5f : 1.0f);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  WiredTVScreen — lain_img.jpg with heavy CRT glitch
// ═════════════════════════════════════════════════════════════════════════════
WiredTVScreen::WiredTVScreen()
{
    startTimerHz (12);
}

WiredTVScreen::~WiredTVScreen() { stopTimer(); }

void WiredTVScreen::setImageData (const void* data, size_t dataSize)
{
    if (data == nullptr || dataSize == 0) return;
    auto img = juce::ImageFileFormat::loadFrom (data, dataSize);
    if (img.isValid())
        staticImage = img;
}

void WiredTVScreen::setGlitchIntensity (float i)
{
    glitchIntensity = juce::jlimit (0.0f, 1.0f, i);
}

void WiredTVScreen::timerCallback()
{
    const int h = juce::jmax (1, getHeight());
    scanlineOffset = std::fmod (scanlineOffset + 1.2f, static_cast<float> (h));

    const float base = 0.2f + glitchIntensity * 0.6f;
    if (rng.nextFloat() < base)
    {
        glitchLineY = rng.nextInt (h);
        glitchAlpha = 0.5f + rng.nextFloat() * 0.5f;
        hShift      = (rng.nextFloat() - 0.5f) * 20.0f;
    }
    else { glitchAlpha *= 0.6f; hShift *= 0.7f; }

    if (rng.nextFloat() < 0.08f + glitchIntensity * 0.35f)
    {
        corruptBlock = rng.nextInt (h);
        corruptAlpha = 0.6f + rng.nextFloat() * 0.4f;
    }
    else { corruptAlpha *= 0.5f; }

    repaint();
}

void WiredTVScreen::paint (juce::Graphics& g)
{
    const auto b      = getLocalBounds().toFloat();
    const auto screen = b.reduced (6.0f);

    // ── Outer casing — raised 3D slab ────────────────────────────────────
    g.setColour (WC::black.withAlpha (0.9f));
    g.fillRoundedRectangle (b.translated (4.0f, 4.0f), 5.0f);

    {
        juce::ColourGradient cas (WC::panelHi, b.getX(), b.getY(),
                                   WC::nearBlk, b.getRight(), b.getBottom(), false);
        g.setGradientFill (cas);
        g.fillRoundedRectangle (b, 5.0f);
    }
    g.setColour (WC::grey.withAlpha (0.45f));
    g.drawLine (b.getX() + 5, b.getY() + 1, b.getRight() - 5, b.getY() + 1, 1.0f);
    g.drawLine (b.getX() + 1, b.getY() + 5, b.getX() + 1, b.getBottom() - 5, 1.0f);
    g.setColour (WC::black.withAlpha (0.8f));
    g.drawLine (b.getX() + 5, b.getBottom() - 1, b.getRight() - 5, b.getBottom() - 1, 1.5f);
    g.drawLine (b.getRight() - 1, b.getY() + 5, b.getRight() - 1, b.getBottom() - 5, 1.5f);
    g.setColour (WC::dimGrey.withAlpha (0.5f));
    g.drawRoundedRectangle (b.reduced (1.0f), 5.0f, 1.0f);

    // ── Screen recess ─────────────────────────────────────────────────────
    WC::drawSunkenPanel (g, screen, 2.0f);

    g.saveState();
    g.reduceClipRegion (screen.toNearestInt());

    if (staticImage.isValid())
    {
        // Base draw — dark, desaturated
        g.setOpacity (0.55f);
        g.drawImage (staticImage,
                     (int) screen.getX(), (int) screen.getY(),
                     (int) screen.getWidth(), (int) screen.getHeight(),
                     0, 0, staticImage.getWidth(), staticImage.getHeight());
        g.setOpacity (1.0f);

        // Phosphor burn-in overlay
        g.setColour (juce::Colour (0x99000000));
        g.fillRect (screen);

        // Ghost re-draw
        g.setOpacity (0.28f);
        g.drawImage (staticImage,
                     (int) screen.getX(), (int) screen.getY(),
                     (int) screen.getWidth(), (int) screen.getHeight(),
                     0, 0, staticImage.getWidth(), staticImage.getHeight());
        g.setOpacity (1.0f);

        // RGB chromatic aberration fringe (intensifies when active)
        if (glitchIntensity > 0.05f)
        {
            const float ca = glitchIntensity * 0.15f;
            g.setColour (juce::Colour (0xFF, 0x00, 0x00).withAlpha (ca));
            g.drawImage (staticImage,
                         (int) screen.getX() - 3, (int) screen.getY(),
                         (int) screen.getWidth(), (int) screen.getHeight(),
                         0, 0, staticImage.getWidth(), staticImage.getHeight());
            g.setColour (juce::Colour (0x00, 0xFF, 0xFF).withAlpha (ca * 0.65f));
            g.drawImage (staticImage,
                         (int) screen.getX() + 3, (int) screen.getY(),
                         (int) screen.getWidth(), (int) screen.getHeight(),
                         0, 0, staticImage.getWidth(), staticImage.getHeight());
            g.setColour (juce::Colour (0xAA000000));
            g.fillRect (screen);
            g.setOpacity (0.22f);
            g.drawImage (staticImage,
                         (int) screen.getX(), (int) screen.getY(),
                         (int) screen.getWidth(), (int) screen.getHeight(),
                         0, 0, staticImage.getWidth(), staticImage.getHeight());
            g.setOpacity (1.0f);
        }
    }
    else
    {
        // No image — dead static noise
        g.setColour (WC::black);
        g.fillRect (screen);
        juce::Random sr (static_cast<juce::int64> (scanlineOffset * 13));
        for (int i = 0; i < 700; ++i)
        {
            g.setColour (WC::white.withAlpha (0.05f + sr.nextFloat() * 0.2f));
            g.fillRect (screen.getX() + sr.nextFloat() * screen.getWidth(),
                        screen.getY() + sr.nextFloat() * screen.getHeight(),
                        1.5f, 1.0f);
        }
    }

    // ── Horizontal glitch strip ───────────────────────────────────────────
    if (glitchAlpha > 0.02f)
    {
        const float gy = screen.getY() + static_cast<float> (
            glitchLineY % juce::jmax (1, (int) screen.getHeight()));
        g.setColour (WC::white.withAlpha (glitchAlpha * 0.9f));
        g.fillRect (screen.getX() + hShift, gy, screen.getWidth() * 0.85f, 3.0f);
        g.setColour (WC::black.withAlpha (glitchAlpha * 0.5f));
        g.fillRect (screen.getX() + hShift - 4.0f, gy + 3.0f, screen.getWidth() * 0.55f, 1.5f);
        g.setColour (WC::white.withAlpha (glitchAlpha * 0.35f));
        g.fillRect (screen.getX() - hShift * 0.4f, gy + 7.0f, screen.getWidth() * 0.35f, 1.5f);
    }

    // ── Corruption block ──────────────────────────────────────────────────
    if (corruptAlpha > 0.02f)
    {
        const float cy2 = screen.getY() + static_cast<float> (
            corruptBlock % juce::jmax (1, (int) screen.getHeight()));
        g.setColour (WC::white.withAlpha (corruptAlpha * 0.28f));
        g.fillRect (screen.getX(), cy2, screen.getWidth(), 10.0f + rng.nextFloat() * 14.0f);
        juce::Random bn (static_cast<juce::int64> (corruptBlock * 7));
        for (int i = 0; i < 50; ++i)
        {
            g.setColour (bn.nextBool() ? WC::white.withAlpha (0.8f) : WC::black.withAlpha (0.6f));
            g.fillRect (screen.getX() + bn.nextFloat() * screen.getWidth(),
                        cy2 + bn.nextFloat() * 12.0f,
                        2.0f + bn.nextFloat() * 9.0f, 1.0f);
        }
    }

    // ── Heavy scanlines ───────────────────────────────────────────────────
    g.setColour (juce::Colour (0x55000000));
    for (float y = screen.getY(); y < screen.getBottom(); y += 2.5f)
        g.drawHorizontalLine ((int) y, screen.getX(), screen.getRight());

    // ── Phosphor sweep band ───────────────────────────────────────────────
    {
        const float sy = screen.getY() + scanlineOffset;
        const float bw = 18.0f + glitchIntensity * 8.0f;
        juce::ColourGradient band (WC::white.withAlpha (0.06f), screen.getX(), sy,
                                    juce::Colour (0x00FFFFFF), screen.getX(), sy + bw, false);
        g.setGradientFill (band);
        g.fillRect (screen.getX(), sy, screen.getWidth(), bw);
    }

    // ── Screen vignette ───────────────────────────────────────────────────
    {
        juce::ColourGradient vig (juce::Colour (0x00000000), screen.getCentreX(), screen.getCentreY(),
                                   juce::Colour (0xCC000000), screen.getX(), screen.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (screen);
    }

    g.restoreState();

    // ── Bezel corner brackets + LED ───────────────────────────────────────
    g.setColour (WC::dimGrey.withAlpha (0.6f));
    auto dc = [&] (float x, float y, float dx, float dy)
    {
        g.drawLine (x, y, x + dx * 9, y, 1.2f);
        g.drawLine (x, y, x, y + dy * 9, 1.2f);
    };
    dc (b.getX() + 4,     b.getY() + 4,      1,  1);
    dc (b.getRight() - 4, b.getY() + 4,     -1,  1);
    dc (b.getX() + 4,     b.getBottom() - 4,  1, -1);
    dc (b.getRight() - 4, b.getBottom() - 4, -1, -1);

    const float ledX = b.getRight() - 14.0f;
    const float ledY = b.getBottom() - 12.0f;
    g.setColour (WC::black);
    g.fillEllipse (ledX, ledY, 5.0f, 5.0f);
    g.setColour ((glitchIntensity > 0.3f ? WC::white : WC::dimGrey).withAlpha (0.9f));
    g.fillEllipse (ledX + 1.0f, ledY + 1.0f, 3.0f, 3.0f);
}

// ═════════════════════════════════════════════════════════════════════════════
//  WiredWaveform — scrolling phosphor side-scroller
// ═════════════════════════════════════════════════════════════════════════════
WiredWaveform::WiredWaveform (WiredAudioProcessor& p) : proc (p)
{
    localBuf.fill (0.0f);
    startTimerHz (30);
}

WiredWaveform::~WiredWaveform() { stopTimer(); }

void WiredWaveform::timerCallback()
{
    const int wp = proc.visWritePos.load();
    for (int i = 0; i < WiredAudioProcessor::VIS_SIZE; ++i)
        localBuf[static_cast<size_t> (i)] =
            proc.visBuffer[static_cast<size_t> ((wp + i) % WiredAudioProcessor::VIS_SIZE)];

    scrollOffset = (scrollOffset + 2) % juce::jmax (1, getWidth());
    repaint();
}

void WiredWaveform::paint (juce::Graphics& g)
{
    const auto  b    = getLocalBounds().toFloat();
    const float W    = b.getWidth();
    const float H    = b.getHeight();
    const float midY = b.getCentreY();

    WC::drawSunkenPanel (g, b, 2.0f);

    // Grid
    g.setColour (juce::Colour (0xFF181818));
    for (int x = 0; x < (int) W; x += 20)
        g.drawVerticalLine (x, b.getY(), b.getBottom());
    g.setColour (juce::Colour (0xFF141414));
    for (float y = b.getY(); y < b.getBottom(); y += H / 4.0f)
        g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
    g.setColour (juce::Colour (0xFF222222));
    g.drawHorizontalLine ((int) midY, b.getX(), b.getRight());

    // Build wave path
    const int  N = WiredAudioProcessor::VIS_SIZE;
    juce::Path wave, waveFill;
    bool started = false;
    const float scale = H * 0.44f;

    for (int i = 0; i < (int) W; ++i)
    {
        const int   idx = ((int) (static_cast<float> (i) / W * N) + scrollOffset) % N;
        const float s   = localBuf[static_cast<size_t> (idx)];
        const float y   = midY - s * scale;

        if (! started)
        {
            wave.startNewSubPath (b.getX() + (float) i, y);
            waveFill.startNewSubPath (b.getX() + (float) i, midY);
            waveFill.lineTo (b.getX() + (float) i, y);
            started = true;
        }
        else
        {
            wave.lineTo (b.getX() + (float) i, y);
            waveFill.lineTo (b.getX() + (float) i, y);
        }
    }
    waveFill.lineTo (b.getRight(), midY);
    waveFill.closeSubPath();

    g.setColour (WC::white.withAlpha (0.04f));
    g.fillPath (waveFill);
    g.setColour (WC::white.withAlpha (0.05f));
    g.strokePath (wave, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setColour (WC::white.withAlpha (0.12f));
    g.strokePath (wave, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved));
    g.setColour (WC::white.withAlpha (0.88f));
    g.strokePath (wave, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved));

    // Glitch noise
    juce::Random noiseRng (static_cast<juce::int64> (scrollOffset * 17));
    for (int i = 0; i < 14; ++i)
    {
        g.setColour (WC::white.withAlpha (0.2f + noiseRng.nextFloat() * 0.5f));
        g.fillRect (b.getX() + noiseRng.nextFloat() * W,
                    b.getY() + noiseRng.nextFloat() * H,
                    1.0f + noiseRng.nextFloat(), 1.0f);
    }

    // Labels
    g.setFont (monoFont (6.5f));
    g.setColour (WC::dimGrey.withAlpha (0.5f));
    g.drawText ("SIG_OUT //", (int) b.getX() + 3, (int) b.getY() + 2, 70, 9, juce::Justification::left);
    g.drawText (">>>", (int) b.getRight() - 24, (int) b.getY() + 2, 22, 9, juce::Justification::right);
}

// ═════════════════════════════════════════════════════════════════════════════
//  WiredEgoButton — raised 3D housing, Mishby LED style
// ═════════════════════════════════════════════════════════════════════════════
WiredEgoButton::WiredEgoButton() { setButtonText (""); }

void WiredEgoButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto b  = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();

    // Drop shadow
    g.setColour (WC::black.withAlpha (0.8f));
    g.fillRect (b.translated (3.5f, 3.5f));

    // Panel — sunken when on, raised when off
    if (on)
        WC::drawSunkenPanel (g, b, 2.5f);
    else
        WC::drawRaisedPanel (g, b, 3.0f);

    if (on)
    {
        g.setColour (WC::white.withAlpha (0.18f));
        g.drawRect (b.expanded (2.0f), 1.5f);
    }

    // Scratch marks
    g.setColour (WC::black.withAlpha (0.22f));
    g.drawLine (b.getX() + 4, b.getBottom() - 5, b.getX() + 11, b.getBottom() - 12, 1.0f);
    g.drawLine (b.getX() + 7, b.getBottom() - 5, b.getX() + 14, b.getBottom() - 12, 0.5f);

    // LED socket
    const float pr = 5.0f;
    const float px = b.getCentreX() - pr;
    const float py = b.getCentreY() - 13.0f - pr;
    g.setColour (WC::black);
    g.fillEllipse (px - 1.5f, py - 1.5f, pr * 2.0f + 3.0f, pr * 2.0f + 3.0f);

    if (on)
    {
        juce::ColourGradient led (WC::white, px + pr * 0.3f, py + pr * 0.3f,
                                   WC::grey,  px + pr, py + pr, true);
        g.setGradientFill (led);
        g.fillEllipse (px, py, pr * 2.0f, pr * 2.0f);
        g.setColour (WC::white.withAlpha (0.3f));
        g.fillEllipse (px - 3.0f, py - 3.0f, pr * 2.0f + 6.0f, pr * 2.0f + 6.0f);
        g.setColour (WC::white.withAlpha (0.9f));
        g.fillEllipse (px + pr * 0.3f, py + pr * 0.2f, pr * 0.45f, pr * 0.35f);
    }
    else
    {
        g.setColour (WC::mid);
        g.fillEllipse (px, py, pr * 2.0f, pr * 2.0f);
    }

    // Label
    g.setColour (on ? WC::white : WC::grey);
    g.setFont (monoFont (11.0f, on ? juce::Font::bold : juce::Font::plain));
    g.drawText ("EGO", b.withTop (b.getCentreY() + 2.0f).toNearestInt(),
                juce::Justification::centred);

    if (highlighted && ! on) { g.setColour (WC::white.withAlpha (0.07f)); g.fillRect (b); }
    if (down)                 { g.setColour (WC::black.withAlpha (0.2f));  g.fillRect (b); }
}

// ═════════════════════════════════════════════════════════════════════════════
//  WiredAudioProcessorEditor
// ═════════════════════════════════════════════════════════════════════════════
WiredAudioProcessorEditor::WiredAudioProcessorEditor (WiredAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), waveform (p)
{
    setSize (420, 580);

    // ── Title ─────────────────────────────────────────────────────────────
    titleLabel.setText ("W I R E D", juce::dontSendNotification);
    titleLabel.setFont (monoFont (16.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, WC::white);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    // ── TV Screen ─────────────────────────────────────────────────────────
    addAndMakeVisible (tvScreen);
    tvScreen.setImageData (BinaryData::lain_img_jpg, BinaryData::lain_img_jpgSize);

    // ── Waveform ──────────────────────────────────────────────────────────
    addAndMakeVisible (waveform);

    // ── Rate knob ─────────────────────────────────────────────────────────
    addAndMakeVisible (rateKnob);
    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "rate", rateKnob);

    rateLabel.setText ("RATE", juce::dontSendNotification);
    rateLabel.setFont (monoFont (9.0f));
    rateLabel.setColour (juce::Label::textColourId, WC::dimGrey);
    rateLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rateLabel);

    // ── EGO button ────────────────────────────────────────────────────────
    addAndMakeVisible (egoButton);
    egoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "active", egoButton);

    egoLabel.setText ("", juce::dontSendNotification);
    addAndMakeVisible (egoLabel);

    // ── Lain ghost watermark ──────────────────────────────────────────────
    lainImage = juce::ImageCache::getFromMemory (BinaryData::lain_img_jpg,
                                                 BinaryData::lain_img_jpgSize);

    buildWirePattern();
    startTimerHz (30);
}

WiredAudioProcessorEditor::~WiredAudioProcessorEditor() { stopTimer(); }

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessorEditor::buildWirePattern()
{
    wirePattern.clear();
    juce::Random r (99);
    const float W = 420.0f, H = 580.0f;

    for (int i = 0; i < 65; ++i)
    {
        const float x1  = r.nextFloat() * W;
        const float y1  = r.nextFloat() * H;
        const float x2  = r.nextFloat() * W;
        const float y2  = r.nextFloat() * H;
        const float cpx = (x1 + x2) * 0.5f + (r.nextFloat() - 0.5f) * 110.0f;
        const float cpy = (y1 + y2) * 0.5f + (r.nextFloat() - 0.5f) * 110.0f;
        wirePattern.startNewSubPath (x1, y1);
        wirePattern.quadraticTo (cpx, cpy, x2, y2);
    }
    for (int i = 0; i < 18; ++i)
    {
        const float y = r.nextFloat() * H;
        wirePattern.startNewSubPath (0.0f, y);
        wirePattern.lineTo (W, y + (r.nextFloat() - 0.5f) * 35.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessorEditor::timerCallback()
{
    noisePhase += 0.05f;
    scanlineY   = (scanlineY + 3) % juce::jmax (1, getHeight());
    grainSeed  += 1.0f;

    if (rng.nextFloat() < 0.05f)
        flickerAlpha = 0.015f + rng.nextFloat() * 0.025f;
    else
        flickerAlpha *= 0.8f;

    if (rng.nextFloat() < 0.04f)
    {
        scratchY     = rng.nextInt (juce::jmax (1, getHeight()));
        scratchAlpha = 0.3f + rng.nextFloat() * 0.5f;
    }
    else
        scratchAlpha *= 0.7f;

    tvScreen.setGlitchIntensity (audioProcessor.isEffectActive() ? 0.75f : 0.15f);
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessorEditor::drawFilmGrain (juce::Graphics& g)
{
    juce::Random gr (static_cast<juce::int64> (grainSeed * 31));
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;
    for (int i = 0; i < 400; ++i)
    {
        g.setColour (WC::white.withAlpha (0.02f + gr.nextFloat() * 0.06f));
        g.fillRect (gr.nextInt (W), gr.nextInt (H), 1, 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessorEditor::drawCRTOverlay (juce::Graphics& g)
{
    const auto  b = getLocalBounds().toFloat();
    const float W = b.getWidth();

    g.setColour (juce::Colour (0x0D000000));
    for (float y = 0; y < b.getHeight(); y += 2.5f)
        g.drawHorizontalLine ((int) y, 0.0f, W);

    g.setColour (WC::white.withAlpha (0.025f));
    g.fillRect (0.0f, (float) scanlineY, W, 30.0f);

    {
        juce::ColourGradient vig (juce::Colour (0x00000000), b.getCentreX(), b.getCentreY(),
                                   juce::Colour (0xCC000000), 0.0f, 0.0f, true);
        g.setGradientFill (vig);
        g.fillRect (b);
    }

    if (flickerAlpha > 0.003f)
    {
        g.setColour (WC::white.withAlpha (flickerAlpha));
        g.fillRect (b);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessorEditor::drawScratchLines (juce::Graphics& g)
{
    if (scratchAlpha < 0.02f) return;
    const float W  = static_cast<float> (getWidth());
    const float sy = static_cast<float> (scratchY);
    g.setColour (WC::white.withAlpha (scratchAlpha * 0.6f));
    g.drawHorizontalLine ((int) sy, 0.0f, W * (0.3f + rng.nextFloat() * 0.6f));
    g.setColour (WC::black.withAlpha (scratchAlpha * 0.3f));
    g.drawHorizontalLine ((int) sy + 1, 0.0f, W * 0.4f);
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto  b = getLocalBounds().toFloat();
    const float W = b.getWidth();
    const float H = b.getHeight();

    // ══ BASE — body gradient ══════════════════════════════════════════════
    g.setColour (WC::black);
    g.fillRect (b);
    g.setColour (WC::black.withAlpha (0.95f));
    g.fillRect (5.0f, 5.0f, W - 2.0f, H - 2.0f);
    {
        juce::ColourGradient body (juce::Colour (0xFF1A1A1A), 0.0f, 0.0f,
                                    juce::Colour (0xFF0A0A0A), W, H, false);
        g.setGradientFill (body);
        g.fillRect (0.0f, 0.0f, W, H);
    }
    // Shell edges
    g.setColour (juce::Colour (0xFF303030));
    g.fillRect (0.0f, 0.0f, W, 2.0f);
    g.fillRect (0.0f, 0.0f, 2.0f, H);
    g.setColour (WC::black);
    g.fillRect (0.0f, H - 2.0f, W, 2.0f);
    g.fillRect (W - 2.0f, 0.0f, 2.0f, H);

    // ══ WIRE TEXTURE ══════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xFF111111));
    g.strokePath (wirePattern, juce::PathStrokeType (0.4f));

    // ══ LAIN GHOST WATERMARK ══════════════════════════════════════════════
    if (lainImage.isValid())
    {
        g.setOpacity (0.05f);
        g.drawImage (lainImage,
                     (int) (W * 0.12f), (int) (H * 0.22f),
                     (int) (W * 0.76f), (int) (H * 0.46f),
                     0, 0, lainImage.getWidth(), lainImage.getHeight());
        g.setOpacity (1.0f);
    }

    // ══ GRID ════════════════════════════════════════════════════════════
    g.setColour (juce::Colour (0xFF0F0F0F));
    for (float y = 0.0f; y < H; y += 22.0f) g.drawHorizontalLine ((int) y, 0.0f, W);
    for (float x = 0.0f; x < W; x += 22.0f) g.drawVerticalLine   ((int) x, 0.0f, H);

    // ══ TOP BAR — raised panel ════════════════════════════════════════════
    WC::drawRaisedPanel (g, { 0.0f, 0.0f, W, 44.0f }, 2.5f);
    g.setColour (WC::black);
    g.fillRect (0.0f, 44.0f, W, 1.5f);
    g.setColour (WC::dimGrey.withAlpha (0.3f));
    g.fillRect (0.0f, 45.5f, W, 0.5f);

    // Status dots — pulsing when active
    const bool active = audioProcessor.isEffectActive();
    for (int i = 0; i < 9; ++i)
    {
        const float phase = noisePhase * 4.5f + static_cast<float> (i) * 0.85f;
        const float alpha = active
            ? juce::jlimit (0.1f, 1.0f, 0.4f + 0.6f * std::abs (std::sin (phase))) : 0.12f;
        const float sz = active && (i % 3 == 0) ? 5.0f : 3.5f;
        g.setColour (WC::black);
        g.fillRect (6.0f + (float) i * 11.0f - 1.0f, 19.0f - 1.0f, sz + 2.0f, sz + 2.0f);
        g.setColour (WC::white.withAlpha (alpha));
        g.fillRect (6.0f + (float) i * 11.0f, 19.0f, sz, sz);
    }

    // ══ TV SCREEN HOUSING — big raised slab ═══════════════════════════════
    {
        const auto tv = tvScreen.getBounds().toFloat().expanded (8.0f);
        g.setColour (WC::black.withAlpha (0.9f));
        g.fillRect (tv.translated (5.0f, 5.0f));
        WC::drawRaisedPanel (g, tv, 4.0f);

        // Rivet dots at corners
        for (auto rvx : { tv.getX() + 7.0f, tv.getRight() - 7.0f })
            for (auto rvy : { tv.getY() + 7.0f, tv.getBottom() - 7.0f })
            {
                g.setColour (WC::black);
                g.fillEllipse (rvx - 3.0f, rvy - 3.0f, 6.0f, 6.0f);
                g.setColour (WC::panelHi);
                g.drawEllipse (rvx - 2.5f, rvy - 2.5f, 5.0f, 5.0f, 0.8f);
            }

        // Corner bracket targets
        auto drawBracket = [&] (float x, float y, float sx, float sy)
        {
            g.setColour (WC::dimGrey.withAlpha (0.65f));
            g.drawLine (x, y, x + sx * 12, y, 1.2f);
            g.drawLine (x, y, x, y + sy * 12, 1.2f);
        };
        const auto tv2 = tvScreen.getBounds().toFloat().expanded (3.0f);
        drawBracket (tv2.getX(),     tv2.getY(),       1,  1);
        drawBracket (tv2.getRight(), tv2.getY(),      -1,  1);
        drawBracket (tv2.getX(),     tv2.getBottom(),  1, -1);
        drawBracket (tv2.getRight(), tv2.getBottom(), -1, -1);
    }

    // ══ WAVEFORM HOUSING — recessed slot ══════════════════════════════════
    {
        const auto wv = waveform.getBounds().toFloat().expanded (4.0f);
        WC::drawSunkenPanel (g, wv, 2.0f);
    }

    // ══ CONTROLS PANEL — large raised slab ════════════════════════════════
    {
        const float cpY = 315.0f;
        const float cpH = H - cpY - 30.0f;
        const juce::Rectangle<float> cp (12.0f, cpY, W - 24.0f, cpH);
        WC::drawRaisedPanel (g, cp, 3.5f);

        // Engraved label
        g.setFont (monoFont (6.0f));
        g.setColour (WC::black.withAlpha (0.8f));
        g.drawText ("CTRL_UNIT", (int) cp.getX() + 6, (int) cp.getY() + 5, 80, 10, juce::Justification::left);
        g.setColour (WC::dimGrey.withAlpha (0.35f));
        g.drawText ("CTRL_UNIT", (int) cp.getX() + 5, (int) cp.getY() + 4, 80, 10, juce::Justification::left);
    }

    // ══ KNOB SOCKET — sunken ══════════════════════════════════════════════
    WC::drawSunkenPanel (g, rateKnob.getBounds().toFloat().expanded (6.0f), 2.5f);

    // ══ EGO BUTTON SOCKET ════════════════════════════════════════════════
    WC::drawSunkenPanel (g, egoButton.getBounds().toFloat().expanded (5.0f), 2.0f);

    // ══ SEPARATOR ════════════════════════════════════════════════════════
    g.setColour (WC::black);
    g.fillRect (20.0f, 400.0f, W - 40.0f, 1.5f);
    g.setColour (WC::dimGrey.withAlpha (0.22f));
    g.fillRect (20.0f, 401.5f, W - 40.0f, 0.8f);
    WC::drawRaisedPanel (g, { (float) (W / 2) - 4.0f, 398.0f, 8.0f, 5.0f }, 1.0f);

    // ══ BOTTOM BAR ═══════════════════════════════════════════════════════
    {
        WC::drawRaisedPanel (g, { 0.0f, H - 30.0f, W, 30.0f }, 2.0f);
        g.setColour (WC::black);
        g.fillRect (0.0f, H - 30.0f, W, 1.5f);

        g.setFont (monoFont (7.0f));
        g.setColour (WC::dimGrey);
        g.drawText ("kyoa", 8, (int) (H - 20), 50, 12, juce::Justification::left);
        g.drawText ("v1.0.0", (int) W - 54, (int) (H - 20), 48, 12, juce::Justification::right);
        g.setFont (monoFont (6.0f));
        g.setColour (WC::mid);
        g.drawText ("present_wired.exe", (int) (W * 0.32f), (int) (H - 20), 130, 12, juce::Justification::left);
    }

    // ══ FILM GRAIN / CRT / SCRATCH ═══════════════════════════════════════
    drawFilmGrain   (g);
    drawCRTOverlay  (g);
    drawScratchLines (g);
}

// ─────────────────────────────────────────────────────────────────────────────
void WiredAudioProcessorEditor::resized()
{
    const int W = getWidth();

    titleLabel.setBounds (0, 10, W, 24);

    // TV screen — centred, upper section
    constexpr int tvW = 310, tvH = 220;
    tvScreen.setBounds ((W - tvW) / 2, 50, tvW, tvH);

    // Waveform — below TV
    waveform.setBounds (20, 282, W - 40, 48);

    // Controls
    constexpr int ctrlY    = 335;
    constexpr int knobSize = 86;
    constexpr int btnSize  = 74;

    const int knobX = W / 2 - 85 - knobSize / 2;
    rateKnob.setBounds  (knobX, ctrlY, knobSize, knobSize);
    rateLabel.setBounds (knobX, ctrlY + knobSize + 2, knobSize, 14);

    const int btnX = W / 2 + 85 - btnSize / 2;
    egoButton.setBounds (btnX, ctrlY + 4, btnSize, btnSize);
}
