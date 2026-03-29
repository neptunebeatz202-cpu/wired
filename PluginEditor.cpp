#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Monochrome palette — everything is black, white, and mid-greys.
//  Intentionally harsh and uneven. Lain / SEL dystopian aesthetic.
// ─────────────────────────────────────────────────────────────────────────────
namespace WC
{
    const juce::Colour black   { 0xFF000000 };
    const juce::Colour nearBlk { 0xFF050505 };
    const juce::Colour dark    { 0xFF0C0C0C };
    const juce::Colour panelDk { 0xFF141414 };   // raised panel dark face
    const juce::Colour panelMd { 0xFF1E1E1E };   // raised panel mid face
    const juce::Colour panelHi { 0xFF2E2E2E };   // raised panel highlight edge
    const juce::Colour mid     { 0xFF2A2A2A };
    const juce::Colour dimGrey { 0xFF555555 };
    const juce::Colour grey    { 0xFF888888 };
    const juce::Colour light   { 0xFFBBBBBB };
    const juce::Colour white   { 0xFFFFFFFF };
    const juce::Colour offWhite{ 0xFFDDDDDD };

    // Helper: draw a raised/extruded panel rectangle (Mishby 3D look, monochrome)
    inline void drawRaisedPanel (juce::Graphics& g, juce::Rectangle<float> r, float depth = 3.0f)
    {
        // Face
        g.setColour (panelMd);
        g.fillRect (r);
        // Top/left highlight
        g.setColour (panelHi);
        g.fillRect (r.getX(), r.getY(), r.getWidth(), depth);
        g.fillRect (r.getX(), r.getY(), depth, r.getHeight());
        // Bottom/right shadow
        g.setColour (black);
        g.fillRect (r.getX(), r.getBottom() - depth, r.getWidth(), depth);
        g.fillRect (r.getRight() - depth, r.getY(), depth, r.getHeight());
        // Outer rim
        g.setColour (juce::Colour (0xFF3A3A3A));
        g.drawRect (r, 1.0f);
    }

    inline void drawSunkenPanel (juce::Graphics& g, juce::Rectangle<float> r, float depth = 2.0f)
    {
        g.setColour (nearBlk);
        g.fillRect (r);
        // Top/left shadow (sunken)
        g.setColour (black);
        g.fillRect (r.getX(), r.getY(), r.getWidth(), depth);
        g.fillRect (r.getX(), r.getY(), depth, r.getHeight());
        // Bottom/right slight highlight
        g.setColour (panelHi);
        g.fillRect (r.getX(), r.getBottom() - depth, r.getWidth(), depth);
        g.fillRect (r.getRight() - depth, r.getY(), depth, r.getHeight());
        g.setColour (juce::Colour (0xFF222222));
        g.drawRect (r, 1.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static juce::Font monoFont (float size, int style = juce::Font::plain)
{
    return juce::Font (juce::Font::getDefaultMonospacedFontName(), size, style);
}

// ═════════════════════════════════════════════
//  WiredKnob — raised 3D body, Mishby-style extrusion
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
    const auto  b      = getLocalBounds().toFloat().reduced (6.0f);
    const float cx     = b.getCentreX();
    const float cy     = b.getCentreY();
    const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

    const float startA = juce::MathConstants<float>::pi * 1.25f;
    const float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float normV  = static_cast<float> ((getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    const float valA   = startA + normV * (endA - startA);

    // ── Extruded base shadow (3D drop effect) ────────────────────────────
    const float dOff = 3.5f;
    g.setColour (WC::black.withAlpha (0.8f));
    g.fillEllipse (cx - radius + dOff, cy - radius + dOff, radius * 2.0f, radius * 2.0f);

    // ── Main body gradient — dark center, lighter rim (raises it) ────────
    {
        juce::ColourGradient grad (WC::panelHi, cx - radius * 0.3f, cy - radius * 0.3f,
                                    WC::nearBlk,  cx + radius * 0.6f, cy + radius * 0.6f, true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }

    // ── Track ─────────────────────────────────────────────────────────────
    {
        juce::Path track;
        track.addCentredArc (cx, cy, radius * 0.82f, radius * 0.82f, 0.0f, startA, endA, true);
        g.setColour (WC::mid);
        g.strokePath (track, juce::PathStrokeType (2.5f));
    }

    // ── Value arc — dashed ────────────────────────────────────────────────
    {
        juce::Path val;
        val.addCentredArc (cx, cy, radius * 0.82f, radius * 0.82f, 0.0f, startA, valA, true);
        juce::PathStrokeType pst (2.5f);
        float dashes[] = { 5.0f, 2.5f };
        pst.createDashedStroke (val, val, dashes, 2);
        g.setColour (WC::white);
        g.strokePath (val, juce::PathStrokeType (2.5f));
    }

    // ── Outer highlight rim (top-left = bright, bottom-right = dark) ─────
    {
        juce::ColourGradient rim (WC::light.withAlpha (0.6f), cx - radius, cy - radius,
                                   WC::black.withAlpha (0.8f),  cx + radius, cy + radius, false);
        g.setGradientFill (rim);
        g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    }

    // ── Inner cap — raised dome effect ───────────────────────────────────
    {
        juce::ColourGradient cap (WC::panelHi, cx - radius * 0.35f, cy - radius * 0.35f,
                                   WC::nearBlk, cx + radius * 0.4f,  cy + radius * 0.4f, true);
        g.setGradientFill (cap);
        g.fillEllipse (cx - radius * 0.62f, cy - radius * 0.62f, radius * 1.24f, radius * 1.24f);
    }
    g.setColour (WC::dimGrey.withAlpha (0.7f));
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
        const float t      = static_cast<float> (i) / 10.0f;
        const float a      = startA + t * (endA - startA);
        const float jitter = 1.0f + tickRng.nextFloat() * 2.0f;
        const float inner  = radius * 0.87f;
        const float outer  = radius * (0.95f + jitter * 0.01f);
        g.setColour (i % 5 == 0 ? WC::grey : WC::mid);
        g.drawLine (cx + inner * std::sin (a), cy - inner * std::cos (a),
                    cx + outer * std::sin (a), cy - outer * std::cos (a), i % 5 == 0 ? 1.5f : 1.0f);
    }
}

// ═════════════════════════════════════════════
//  WiredTVScreen
// ═════════════════════════════════════════════
WiredTVScreen::WiredTVScreen()
{
    startTimerHz (12);
}

WiredTVScreen::~WiredTVScreen() { stopTimer(); }

void WiredTVScreen::setGifData (const void* data, size_t dataSize)
{
    // Legacy stub — we now use setImageData for static images.
    // Kept for ABI compat but does nothing.
    (void) data; (void) dataSize;
}

void WiredTVScreen::setImageData (const void* data, size_t dataSize)
{
    frames.clear();
    currentFrame = frameCounter = 0;
    if (data == nullptr || dataSize == 0) return;

    auto img = juce::ImageFileFormat::loadFrom (data, dataSize);
    if (img.isValid())
        frames.add (new juce::Image (img));
}

void WiredTVScreen::setGlitchIntensity (float i) { glitchIntensity = juce::jlimit (0.0f, 1.0f, i); }

void WiredTVScreen::timerCallback()
{
    if (frames.size() > 1)
        if (++frameCounter >= 3) { frameCounter = 0; currentFrame = (currentFrame + 1) % frames.size(); }

    scanlineOffset = std::fmod (scanlineOffset + 1.2f, static_cast<float> (juce::jmax (1, getHeight())));

    const int h = juce::jmax (1, getHeight());
    const float base = 0.2f + glitchIntensity * 0.6f;

    if (rng.nextFloat() < base)
    {
        glitchLineY = rng.nextInt (h);
        glitchAlpha = 0.5f + rng.nextFloat() * 0.5f;
        hShift      = (rng.nextFloat() - 0.5f) * 18.0f;
    }
    else { glitchAlpha *= 0.6f; hShift *= 0.7f; }

    if (rng.nextFloat() < 0.08f + glitchIntensity * 0.3f)
    {
        corruptBlock = rng.nextInt (h);
        corruptAlpha = 0.7f + rng.nextFloat() * 0.3f;
    }
    else { corruptAlpha *= 0.5f; }

    repaint();
}

void WiredTVScreen::paint (juce::Graphics& g)
{
    const auto  b      = getLocalBounds().toFloat();
    const auto  screen = b.reduced (6.0f);

    // ── Outer casing — raised 3D box (Mishby style) ──────────────────────
    // Drop shadow
    g.setColour (WC::black.withAlpha (0.9f));
    g.fillRoundedRectangle (b.translated (4.0f, 4.0f), 5.0f);

    // Casing body gradient
    {
        juce::ColourGradient cas (WC::panelHi, b.getX(), b.getY(),
                                   WC::nearBlk, b.getRight(), b.getBottom(), false);
        g.setGradientFill (cas);
        g.fillRoundedRectangle (b, 5.0f);
    }
    // Highlight edge (top-left)
    g.setColour (WC::grey.withAlpha (0.5f));
    g.drawLine (b.getX() + 5, b.getY() + 1, b.getRight() - 5, b.getY() + 1, 1.0f);
    g.drawLine (b.getX() + 1, b.getY() + 5, b.getX() + 1, b.getBottom() - 5, 1.0f);
    // Shadow edge (bottom-right)
    g.setColour (WC::black.withAlpha (0.8f));
    g.drawLine (b.getX() + 5, b.getBottom() - 1, b.getRight() - 5, b.getBottom() - 1, 1.5f);
    g.drawLine (b.getRight() - 1, b.getY() + 5, b.getRight() - 1, b.getBottom() - 5, 1.5f);
    // Rim
    g.setColour (WC::dimGrey.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (1.0f), 5.0f, 1.0f);

    // ── Screen recess (sunken) ─────────────────────────────────────────
    WC::drawSunkenPanel (g, screen, 2.0f);

    g.saveState();
    g.reduceClipRegion (screen.toNearestInt());

    if (frames.size() > 0)
    {
        const juce::Image& frame = *frames[currentFrame];

        // ── Base image draw — desaturated / dark ─────────────────────────
        g.setOpacity (0.55f);
        g.drawImage (frame,
                     (int) screen.getX(), (int) screen.getY(),
                     (int) screen.getWidth(), (int) screen.getHeight(),
                     0, 0, frame.getWidth(), frame.getHeight());
        g.setOpacity (1.0f);

        // ── Phosphor burn-in overlay (dark tint) ─────────────────────────
        g.setColour (juce::Colour (0x99000000));
        g.fillRect (screen);

        // ── Ghost re-draw at low opacity ──────────────────────────────────
        g.setOpacity (0.30f);
        g.drawImage (frame,
                     (int) screen.getX(), (int) screen.getY(),
                     (int) screen.getWidth(), (int) screen.getHeight(),
                     0, 0, frame.getWidth(), frame.getHeight());
        g.setOpacity (1.0f);

        // ── RGB chromatic aberration strips (eerie colour fringe) ─────────
        if (glitchIntensity > 0.05f)
        {
            const float ca = glitchIntensity * 0.18f;
            g.setColour (juce::Colour (0xFF, 0x00, 0x00).withAlpha (ca));
            g.drawImage (frame,
                         (int) screen.getX() - 3, (int) screen.getY(),
                         (int) screen.getWidth(), (int) screen.getHeight(),
                         0, 0, frame.getWidth(), frame.getHeight());
            g.setColour (juce::Colour (0x00, 0xFF, 0xFF).withAlpha (ca * 0.7f));
            g.drawImage (frame,
                         (int) screen.getX() + 3, (int) screen.getY(),
                         (int) screen.getWidth(), (int) screen.getHeight(),
                         0, 0, frame.getWidth(), frame.getHeight());
            // Desaturate the colour fringe back with a semi-transparent overlay
            g.setColour (juce::Colour (0xAA000000));
            g.fillRect (screen);
            g.setOpacity (0.25f);
            g.drawImage (frame,
                         (int) screen.getX(), (int) screen.getY(),
                         (int) screen.getWidth(), (int) screen.getHeight(),
                         0, 0, frame.getWidth(), frame.getHeight());
            g.setOpacity (1.0f);
        }
    }
    else
    {
        // No image loaded: dead static
        g.setColour (WC::black);
        g.fillRect (screen);
        juce::Random sr (static_cast<juce::int64> (scanlineOffset * 13));
        for (int i = 0; i < 600; ++i)
        {
            const float a = 0.05f + sr.nextFloat() * 0.25f;
            g.setColour (WC::white.withAlpha (a));
            g.fillRect ((float) sr.nextInt ((int) screen.getWidth()) + screen.getX(),
                        (float) sr.nextInt ((int) screen.getHeight()) + screen.getY(),
                        1.0f + sr.nextFloat() * 2.0f, 1.0f);
        }
    }

    // ── Horizontal glitch strip ─────────────────────────────────────────
    if (glitchAlpha > 0.02f)
    {
        const float gy    = screen.getY() + static_cast<float> (glitchLineY % juce::jmax (1, (int) screen.getHeight()));
        g.setColour (WC::white.withAlpha (glitchAlpha * 0.9f));
        g.fillRect (screen.getX() + hShift, gy, screen.getWidth() * 0.85f, 3.0f);
        g.setColour (WC::black.withAlpha (glitchAlpha * 0.5f));
        g.fillRect (screen.getX() + hShift - 4.0f, gy + 3.0f, screen.getWidth() * 0.6f, 1.5f);
        // Second displaced strip
        g.setColour (WC::white.withAlpha (glitchAlpha * 0.4f));
        g.fillRect (screen.getX() - hShift * 0.5f, gy + 6.0f, screen.getWidth() * 0.4f, 1.5f);
    }

    // ── Corruption block ────────────────────────────────────────────────
    if (corruptAlpha > 0.02f)
    {
        const float cy2 = screen.getY() + static_cast<float> (corruptBlock % juce::jmax (1, (int) screen.getHeight()));
        g.setColour (WC::white.withAlpha (corruptAlpha * 0.3f));
        g.fillRect (screen.getX(), cy2, screen.getWidth(), 10.0f + rng.nextFloat() * 16.0f);
        // Pixel noise inside block
        juce::Random bn (static_cast<juce::int64> (corruptBlock * 7 + (int)(corruptAlpha * 100)));
        for (int i = 0; i < 40; ++i)
        {
            const float bx = screen.getX() + bn.nextFloat() * screen.getWidth();
            const float by = cy2 + bn.nextFloat() * 12.0f;
            g.setColour (bn.nextBool() ? WC::white.withAlpha (0.8f) : WC::black.withAlpha (0.6f));
            g.fillRect (bx, by, 3.0f + bn.nextFloat() * 8.0f, 1.0f);
        }
    }

    // ── Heavy scanlines ─────────────────────────────────────────────────
    g.setColour (juce::Colour (0x55000000));
    for (float y = screen.getY(); y < screen.getBottom(); y += 2.5f)
        g.drawHorizontalLine (static_cast<int> (y), screen.getX(), screen.getRight());

    // ── Moving bright band (phosphor sweep) ─────────────────────────────
    {
        const float sy = screen.getY() + scanlineOffset;
        const float bw = 18.0f + glitchIntensity * 8.0f;
        juce::ColourGradient band (WC::white.withAlpha (0.06f), screen.getX(), sy,
                                    juce::Colour (0x00FFFFFF), screen.getX(), sy + bw, false);
        g.setGradientFill (band);
        g.fillRect (screen.getX(), sy, screen.getWidth(), bw);
    }

    // ── Screen vignette ──────────────────────────────────────────────────
    {
        juce::ColourGradient vig (juce::Colour (0x00000000), screen.getCentreX(), screen.getCentreY(),
                                   juce::Colour (0xCC000000), screen.getX(), screen.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (screen);
    }

    g.restoreState();

    // ── Bezel corner scratch marks ───────────────────────────────────────
    g.setColour (WC::dimGrey.withAlpha (0.6f));
    auto dc = [&] (float x, float y, float dx, float dy) {
        g.drawLine (x, y, x + dx * 9, y, 1.0f);
        g.drawLine (x, y, x, y + dy * 9, 1.0f);
    };
    dc (b.getX() + 4,     b.getY() + 4,     1,  1);
    dc (b.getRight() - 4, b.getY() + 4,    -1,  1);
    dc (b.getX() + 4,     b.getBottom() - 4, 1, -1);
    dc (b.getRight() - 4, b.getBottom() - 4,-1, -1);

    // ── Small indicator LED in corner of bezel ───────────────────────────
    const float ledX = b.getRight() - 14.0f;
    const float ledY = b.getBottom() - 12.0f;
    g.setColour (WC::black);
    g.fillEllipse (ledX, ledY, 5.0f, 5.0f);
    g.setColour ((glitchIntensity > 0.3f ? WC::white : WC::dimGrey).withAlpha (0.9f));
    g.fillEllipse (ledX + 1.0f, ledY + 1.0f, 3.0f, 3.0f);
}

// ═════════════════════════════════════════════
//  WiredWaveform
// ═════════════════════════════════════════════
WiredWaveform::WiredWaveform (WiredAudioProcessor& p) : proc (p)
{
    localBuf.fill (0.0f);
    startTimerHz (30);
}

WiredWaveform::~WiredWaveform() { stopTimer(); }

void WiredWaveform::timerCallback()
{
    // Copy vis buffer from processor
    int wp = proc.visWritePos.load();
    for (int i = 0; i < WiredAudioProcessor::VIS_SIZE; ++i)
        localBuf[static_cast<size_t> (i)] = proc.visBuffer[static_cast<size_t> ((wp + i) % WiredAudioProcessor::VIS_SIZE)];

    scrollOffset = (scrollOffset + 2) % juce::jmax (1, getWidth());
    repaint();
}

void WiredWaveform::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float W = b.getWidth();
    const float H = b.getHeight();
    const float midY = b.getCentreY();

    // ── Raised 3D panel border ─────────────────────────────────────────────
    WC::drawSunkenPanel (g, b, 2.0f);

    // ── Grid lines — phosphor green ghost feel in mono ─────────────────────
    g.setColour (juce::Colour (0xFF181818));
    for (int x = 0; x < (int) W; x += 20)
        g.drawVerticalLine (x, b.getY(), b.getBottom());
    g.setColour (juce::Colour (0xFF141414));
    for (float y = b.getY(); y < b.getBottom(); y += H / 4.0f)
        g.drawHorizontalLine ((int) y, b.getX(), b.getRight());

    // Centre zeroline — slightly brighter
    g.setColour (juce::Colour (0xFF222222));
    g.drawHorizontalLine ((int) midY, b.getX(), b.getRight());

    // ── Build waveform path ────────────────────────────────────────────────
    const int N = WiredAudioProcessor::VIS_SIZE;
    juce::Path wave;
    juce::Path waveFill;
    bool started = false;
    const float scale = H * 0.44f;

    for (int i = 0; i < (int) W; ++i)
    {
        const int idx = ((int) (static_cast<float> (i) / W * N) + scrollOffset) % N;
        const float s = localBuf[static_cast<size_t> (idx)];
        const float y = midY - s * scale;

        if (! started)
        {
            wave.startNewSubPath (b.getX() + (float)i, y);
            waveFill.startNewSubPath (b.getX() + (float)i, midY);
            waveFill.lineTo (b.getX() + (float)i, y);
            started = true;
        }
        else
        {
            wave.lineTo (b.getX() + (float)i, y);
            waveFill.lineTo (b.getX() + (float)i, y);
        }
    }
    waveFill.lineTo (b.getRight(), midY);
    waveFill.closeSubPath();

    // ── Filled waveform body (semi-transparent) ───────────────────────────
    g.setColour (WC::white.withAlpha (0.04f));
    g.fillPath (waveFill);

    // ── Wide outer glow ───────────────────────────────────────────────────
    g.setColour (WC::white.withAlpha (0.05f));
    g.strokePath (wave, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));

    // ── Mid glow ──────────────────────────────────────────────────────────
    g.setColour (WC::white.withAlpha (0.12f));
    g.strokePath (wave, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved));

    // ── Sharp phosphor line ───────────────────────────────────────────────
    g.setColour (WC::white.withAlpha (0.88f));
    g.strokePath (wave, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved));

    // ── Scan-direction arrow marks on right edge ──────────────────────────
    const float arrowX = b.getRight() - 4.0f;
    g.setColour (WC::dimGrey.withAlpha (0.5f));
    for (float ay = b.getY() + 6.0f; ay < b.getBottom() - 4.0f; ay += 10.0f)
    {
        g.drawLine (arrowX, ay, arrowX - 3.0f, ay + 3.0f, 0.5f);
        g.drawLine (arrowX, ay + 3.0f, arrowX - 3.0f, ay + 6.0f, 0.5f);
    }

    // ── Pixel noise / glitch dots ─────────────────────────────────────────
    juce::Random noiseRng (static_cast<juce::int64> (scrollOffset * 17));
    for (int i = 0; i < 14; ++i)
    {
        const float gx = b.getX() + noiseRng.nextFloat() * W;
        const float gy2 = b.getY() + noiseRng.nextFloat() * H;
        const float ga = 0.2f + noiseRng.nextFloat() * 0.5f;
        g.setColour (WC::white.withAlpha (ga));
        g.fillRect (gx, gy2, 1.0f + noiseRng.nextFloat(), 1.0f);
    }

    // ── Label ─────────────────────────────────────────────────────────────
    g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 6.5f, juce::Font::plain));
    g.setColour (WC::dimGrey.withAlpha (0.5f));
    g.drawText ("OUT", (int) b.getRight() - 22, (int) b.getY() + 2, 20, 10, juce::Justification::right);
}

// ═════════════════════════════════════════════
//  WiredEgoButton
// ═════════════════════════════════════════════
WiredEgoButton::WiredEgoButton() { setButtonText (""); }

void WiredEgoButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto  b   = getLocalBounds().toFloat().reduced (2.0f);
    const bool  on  = getToggleState();

    // ── Drop shadow ──────────────────────────────────────────────────────
    g.setColour (WC::black.withAlpha (0.8f));
    g.fillRect (b.translated (3.5f, 3.5f));

    // ── Extruded face ────────────────────────────────────────────────────
    if (on)
    {
        // When on: sunken (pressed in)
        WC::drawSunkenPanel (g, b, 2.5f);
    }
    else
    {
        // When off: raised
        WC::drawRaisedPanel (g, b, 3.0f);
    }

    // ── Outer glow when on ───────────────────────────────────────────────
    if (on)
    {
        g.setColour (WC::white.withAlpha (0.18f));
        g.drawRect (b.expanded (2.0f), 1.5f);
    }

    // ── Diagonal scratch marks (character) ───────────────────────────────
    g.setColour (WC::black.withAlpha (0.25f));
    g.drawLine (b.getX() + 4, b.getBottom() - 5, b.getX() + 10, b.getBottom() - 11, 1.0f);
    g.drawLine (b.getX() + 7, b.getBottom() - 5, b.getX() + 13, b.getBottom() - 11, 0.5f);

    // ── LED pip ───────────────────────────────────────────────────────────
    const float pr = 5.0f;
    const float px = b.getCentreX() - pr;
    const float py = b.getCentreY() - 12.0f - pr;
    // LED housing (sunken socket)
    g.setColour (WC::black);
    g.fillEllipse (px - 1.5f, py - 1.5f, pr * 2.0f + 3.0f, pr * 2.0f + 3.0f);
    // LED bulb
    if (on)
    {
        juce::ColourGradient led (WC::white, px + pr * 0.3f, py + pr * 0.3f,
                                   WC::grey, px + pr, py + pr, true);
        g.setGradientFill (led);
        g.fillEllipse (px, py, pr * 2.0f, pr * 2.0f);
        // Glow halo
        g.setColour (WC::white.withAlpha (0.3f));
        g.fillEllipse (px - 3.0f, py - 3.0f, pr * 2.0f + 6.0f, pr * 2.0f + 6.0f);
        // Specular dot
        g.setColour (WC::white.withAlpha (0.9f));
        g.fillEllipse (px + pr * 0.3f, py + pr * 0.2f, pr * 0.45f, pr * 0.35f);
    }
    else
    {
        g.setColour (WC::mid);
        g.fillEllipse (px, py, pr * 2.0f, pr * 2.0f);
        g.setColour (WC::dimGrey.withAlpha (0.3f));
        g.fillEllipse (px + pr * 0.3f, py + pr * 0.2f, pr * 0.4f, pr * 0.3f);
    }

    // ── Label ──────────────────────────────────────────────────────────────
    g.setColour (on ? WC::white : WC::grey);
    g.setFont (monoFont (11.0f, on ? juce::Font::bold : juce::Font::plain));
    g.drawText ("EGO", b.withTop (b.getCentreY() + 3.0f).toNearestInt(), juce::Justification::centred);

    if (highlighted && !on)
    {
        g.setColour (WC::white.withAlpha (0.07f));
        g.fillRect (b);
    }
    if (down)
    {
        g.setColour (WC::black.withAlpha (0.2f));
        g.fillRect (b);
    }
}

// ═════════════════════════════════════════════
//  LoopModeButton
// ═════════════════════════════════════════════
LoopModeButton::LoopModeButton (const juce::String& label) { setButtonText (label); }

void LoopModeButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto  b   = getLocalBounds().toFloat().reduced (1.5f);
    const bool  sel = getToggleState();

    // Drop shadow
    g.setColour (WC::black.withAlpha (0.7f));
    g.fillRect (b.translated (2.0f, 2.5f));

    if (sel)
    {
        // Selected: sunken + bright face
        WC::drawSunkenPanel (g, b, 2.0f);
        g.setColour (WC::white.withAlpha (0.08f));
        g.fillRect (b.reduced (2.0f));
    }
    else
    {
        WC::drawRaisedPanel (g, b, 2.0f);
    }

    g.setColour (sel ? WC::white : WC::grey);
    g.setFont (monoFont (10.0f, sel ? juce::Font::bold : juce::Font::plain));
    g.drawText (getButtonText(), b.toNearestInt(), juce::Justification::centred);

    if (highlighted && !sel)
    {
        g.setColour (WC::white.withAlpha (0.08f));
        g.fillRect (b);
    }
    if (down)
    {
        g.setColour (WC::black.withAlpha (0.18f));
        g.fillRect (b);
    }
}

// ═════════════════════════════════════════════
//  WiredAudioProcessorEditor
// ═════════════════════════════════════════════
WiredAudioProcessorEditor::WiredAudioProcessorEditor (WiredAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), waveform (p)
{
    setSize (440, 620);

    // ── Title ────────────────────────────────────────────────────────────
    titleLabel.setText ("W I R E D", juce::dontSendNotification);
    titleLabel.setFont (monoFont (16.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, WC::white);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    // ── TV screen ────────────────────────────────────────────────────────
    addAndMakeVisible (tvScreen);

    // ── Waveform ─────────────────────────────────────────────────────────
    addAndMakeVisible (waveform);

    // ── Rate knob ────────────────────────────────────────────────────────
    addAndMakeVisible (rateKnob);
    // NOTE: rateKnob is cosmetic only in this version — rate is now driven
    // by the loop-mode buttons. You can re-attach it to a "rate" param if desired.

    rateLabel.setText ("RATE", juce::dontSendNotification);
    rateLabel.setFont (monoFont (9.0f));
    rateLabel.setColour (juce::Label::textColourId, WC::dimGrey);
    rateLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rateLabel);

    // ── Ego button ───────────────────────────────────────────────────────
    addAndMakeVisible (egoButton);
    egoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "active", egoButton);

    egoLabel.setText ("", juce::dontSendNotification);  // label baked into button
    addAndMakeVisible (egoLabel);

    // ── Loop mode buttons ─────────────────────────────────────────────────
    loopLabel.setText ("BARS", juce::dontSendNotification);
    loopLabel.setFont (monoFont (9.0f));
    loopLabel.setColour (juce::Label::textColourId, WC::dimGrey);
    loopLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (loopLabel);

    for (auto* btn : { &btnHalf, &btnOne, &btnTwo, &btnInf })
    {
        btn->setClickingTogglesState (false);
        btn->setToggleState (false, juce::dontSendNotification);
        addAndMakeVisible (btn);
    }

    btnHalf.onClick = [this] { audioProcessor.setLoopMode (LoopMode::HalfBar);  updateLoopButtons (LoopMode::HalfBar); };
    btnOne.onClick  = [this] { audioProcessor.setLoopMode (LoopMode::OneBar);   updateLoopButtons (LoopMode::OneBar);  };
    btnTwo.onClick  = [this] { audioProcessor.setLoopMode (LoopMode::TwoBars);  updateLoopButtons (LoopMode::TwoBars); };
    btnInf.onClick  = [this] { audioProcessor.setLoopMode (LoopMode::Infinite); updateLoopButtons (LoopMode::Infinite);};

    updateLoopButtons (LoopMode::OneBar);

    buildWirePattern();

    // Load the embedded Lain image from BinaryData
    // Uncomment once project compiles with BinaryData:
    //
    lainImage = juce::ImageCache::getFromMemory (BinaryData::lain_img_jpg,
                                                 BinaryData::lain_img_jpgSize);
    //
    // Also feed the lain image directly into the TV screen component:
    tvScreen.setImageData (BinaryData::lain_img_jpg,
                           BinaryData::lain_img_jpgSize);

    startTimerHz (30);
}

WiredAudioProcessorEditor::~WiredAudioProcessorEditor() { stopTimer(); }

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::updateLoopButtons (LoopMode sel)
{
    btnHalf.setToggleState (sel == LoopMode::HalfBar,  juce::dontSendNotification);
    btnOne.setToggleState  (sel == LoopMode::OneBar,   juce::dontSendNotification);
    btnTwo.setToggleState  (sel == LoopMode::TwoBars,  juce::dontSendNotification);
    btnInf.setToggleState  (sel == LoopMode::Infinite, juce::dontSendNotification);
    repaint();
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::buildWirePattern()
{
    wirePattern.clear();
    juce::Random r (99);
    const float W = 440.0f, H = 620.0f;

    for (int i = 0; i < 70; ++i)
    {
        const float x1  = r.nextFloat() * W;
        const float y1  = r.nextFloat() * H;
        const float x2  = r.nextFloat() * W;
        const float y2  = r.nextFloat() * H;
        const float cpx = (x1 + x2) * 0.5f + (r.nextFloat() - 0.5f) * 120.0f;
        const float cpy = (y1 + y2) * 0.5f + (r.nextFloat() - 0.5f) * 120.0f;
        wirePattern.startNewSubPath (x1, y1);
        wirePattern.quadraticTo (cpx, cpy, x2, y2);
    }

    for (int i = 0; i < 20; ++i)
    {
        const float y = r.nextFloat() * H;
        wirePattern.startNewSubPath (0.0f, y);
        wirePattern.lineTo (W, y + (r.nextFloat() - 0.5f) * 40.0f);
    }
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::timerCallback()
{
    noisePhase   += 0.05f;
    scanlineY     = (scanlineY + 3) % juce::jmax (1, getHeight());
    grainSeed    += 1.0f;

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

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::drawFilmGrain (juce::Graphics& g)
{
    juce::Random gr (static_cast<juce::int64> (grainSeed * 31));
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;

    for (int i = 0; i < 400; ++i)
    {
        const float alpha = 0.02f + gr.nextFloat() * 0.06f;
        g.setColour (WC::white.withAlpha (alpha));
        g.fillRect (gr.nextInt (W), gr.nextInt (H), 1, 1);
    }
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::drawCRTOverlay (juce::Graphics& g)
{
    const auto  b = getLocalBounds().toFloat();
    const float W = b.getWidth();

    // Fine scanlines over everything
    g.setColour (juce::Colour (0x0D000000));
    for (float y = 0; y < b.getHeight(); y += 2.5f)
        g.drawHorizontalLine (static_cast<int> (y), 0.0f, W);

    // Moving bright band
    {
        const float sy = static_cast<float> (scanlineY);
        g.setColour (WC::white.withAlpha (0.025f));
        g.fillRect (0.0f, sy, W, 30.0f);
    }

    // Corner vignette
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

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::drawScratchLines (juce::Graphics& g)
{
    if (scratchAlpha < 0.02f) return;
    const float W = static_cast<float> (getWidth());
    const float sy = static_cast<float> (scratchY);
    g.setColour (WC::white.withAlpha (scratchAlpha * 0.6f));
    g.drawHorizontalLine (static_cast<int> (sy), 0.0f, W * (0.3f + rng.nextFloat() * 0.6f));
    g.setColour (WC::black.withAlpha (scratchAlpha * 0.3f));
    g.drawHorizontalLine (static_cast<int> (sy) + 1, 0.0f, W * 0.4f);
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto  b = getLocalBounds().toFloat();
    const float W = b.getWidth();
    const float H = b.getHeight();

    // ══ BASE — very dark, almost black ════════════════════════════════════
    g.setColour (juce::Colour (0xFF090909));
    g.fillRect (b);

    // ══ MAIN BODY PANEL — slight 3D extrusion of the whole shell ══════════
    {
        // Outer drop shadow of the whole unit
        g.setColour (WC::black.withAlpha (0.95f));
        g.fillRect (5.0f, 5.0f, W - 2.0f, H - 2.0f);

        // Body fill — gradient top-left lighter
        juce::ColourGradient body (juce::Colour (0xFF1A1A1A), 0.0f, 0.0f,
                                    juce::Colour (0xFF0A0A0A), W, H, false);
        g.setGradientFill (body);
        g.fillRect (0.0f, 0.0f, W, H);

        // Top highlight edge
        g.setColour (juce::Colour (0xFF303030));
        g.fillRect (0.0f, 0.0f, W, 2.0f);
        // Left highlight edge
        g.fillRect (0.0f, 0.0f, 2.0f, H);
        // Bottom shadow
        g.setColour (WC::black);
        g.fillRect (0.0f, H - 2.0f, W, 2.0f);
        // Right shadow
        g.fillRect (W - 2.0f, 0.0f, 2.0f, H);
    }

    // ══ WIRE TEXTURE — very faint ══════════════════════════════════════════
    g.setColour (juce::Colour (0xFF111111));
    g.strokePath (wirePattern, juce::PathStrokeType (0.4f));

    // ══ LAIN GHOST WATERMARK ══════════════════════════════════════════════
    if (lainImage.isValid())
    {
        g.setOpacity (0.055f);
        g.drawImage (lainImage,
                     (int)(W * 0.12f), (int)(H * 0.18f),
                     (int)(W * 0.76f), (int)(H * 0.52f),
                     0, 0, lainImage.getWidth(), lainImage.getHeight());
        g.setOpacity (1.0f);
    }

    // ══ ROUGH BACKGROUND GRID ════════════════════════════════════════════
    g.setColour (juce::Colour (0xFF0F0F0F));
    for (float y = 0.0f; y < H; y += 22.0f)
        g.drawHorizontalLine ((int) y, 0.0f, W);
    for (float x = 0.0f; x < W; x += 22.0f)
        g.drawVerticalLine   ((int) x, 0.0f, H);

    // ══ TOP BAR — raised panel ════════════════════════════════════════════
    {
        juce::Rectangle<float> topBar (0.0f, 0.0f, W, 44.0f);
        WC::drawRaisedPanel (g, topBar, 2.5f);

        // Etched line below
        g.setColour (WC::black);
        g.fillRect (0.0f, 44.0f, W, 1.5f);
        g.setColour (WC::dimGrey.withAlpha (0.4f));
        g.fillRect (0.0f, 45.5f, W, 0.5f);

        // Scratch line
        g.setColour (WC::mid.withAlpha (0.5f));
        g.drawHorizontalLine (43, 0.0f, W * 0.55f);
    }

    // ── Pulsing status marks ──────────────────────────────────────────────
    const bool active = audioProcessor.isEffectActive();
    for (int i = 0; i < 9; ++i)
    {
        const float phase   = noisePhase * 4.5f + static_cast<float> (i) * 0.85f;
        const float alpha   = active
            ? juce::jlimit (0.1f, 1.0f, 0.4f + 0.6f * std::abs (std::sin (phase)))
            : 0.12f;
        const float sz      = active && (i % 3 == 0) ? 5.0f : 3.5f;
        // Sunken socket
        g.setColour (WC::black);
        g.fillRect (7.0f + (float)i * 11.0f - 1.0f, 19.0f - 1.0f, sz + 2.0f, sz + 2.0f);
        g.setColour (WC::white.withAlpha (alpha));
        g.fillRect (7.0f + (float)i * 11.0f, 19.0f, sz, sz);
    }

    // ══ TV SCREEN PANEL — raised bezel housing ════════════════════════════
    {
        const auto tv = tvScreen.getBounds().toFloat().expanded (8.0f);
        // Outer housing drop shadow
        g.setColour (WC::black.withAlpha (0.9f));
        g.fillRect (tv.translated (5.0f, 5.0f));
        // Outer housing face
        WC::drawRaisedPanel (g, tv, 4.0f);
        // Embossed rivet marks on housing corners
        for (auto cx2 : { tv.getX() + 6.0f, tv.getRight() - 6.0f })
        {
            for (auto cy2 : { tv.getY() + 6.0f, tv.getBottom() - 6.0f })
            {
                g.setColour (WC::black);
                g.fillEllipse (cx2 - 3.0f, cy2 - 3.0f, 6.0f, 6.0f);
                g.setColour (WC::panelHi);
                g.drawEllipse (cx2 - 2.5f, cy2 - 2.5f, 5.0f, 5.0f, 0.8f);
            }
        }
        // Corner cross-hatch bracket marks
        auto drawBracket = [&] (float x, float y, float sx, float sy)
        {
            g.setColour (WC::dimGrey.withAlpha (0.7f));
            g.drawLine (x, y, x + sx * 12, y, 1.2f);
            g.drawLine (x, y, x, y + sy * 12, 1.2f);
        };
        const auto tv2 = tvScreen.getBounds().toFloat().expanded (4.0f);
        drawBracket (tv2.getX(),     tv2.getY(),       1,  1);
        drawBracket (tv2.getRight(), tv2.getY(),      -1,  1);
        drawBracket (tv2.getX(),     tv2.getBottom(),  1, -1);
        drawBracket (tv2.getRight(), tv2.getBottom(), -1, -1);
    }

    // ══ WAVEFORM HOUSING — recessed slot ════════════════════════════════════
    {
        const auto wv = waveform.getBounds().toFloat().expanded (4.0f);
        WC::drawSunkenPanel (g, wv, 2.0f);
        // Label above
        g.setFont (monoFont (6.5f));
        g.setColour (WC::dimGrey.withAlpha (0.6f));
        g.drawText ("SIG_OUT //", (int) wv.getX(), (int) wv.getY() - 12, 80, 10, juce::Justification::left);
        // Right label
        g.drawText ("SCROLL >>>", (int)(W - 90), (int) wv.getY() - 12, 80, 10, juce::Justification::right);
    }

    // ══ CONTROLS PANEL — big raised slab ════════════════════════════════════
    {
        const float ctrlPanelY = 305.0f;
        const float ctrlPanelH = H - ctrlPanelY - 32.0f;
        juce::Rectangle<float> ctrlPanel (12.0f, ctrlPanelY, W - 24.0f, ctrlPanelH);
        WC::drawRaisedPanel (g, ctrlPanel, 3.5f);

        // Embossed label on panel
        g.setFont (monoFont (6.0f));
        g.setColour (WC::black.withAlpha (0.8f));
        g.drawText ("CTRL_UNIT 04", (int) ctrlPanel.getX() + 6, (int) ctrlPanel.getY() + 5, 100, 10, juce::Justification::left);
        g.setColour (WC::dimGrey.withAlpha (0.4f));
        g.drawText ("CTRL_UNIT 04", (int) ctrlPanel.getX() + 5, (int) ctrlPanel.getY() + 4, 100, 10, juce::Justification::left);
    }

    // ══ KNOB HOUSING — individual sunken socket ══════════════════════════
    {
        const auto kb = rateKnob.getBounds().toFloat().expanded (6.0f);
        WC::drawSunkenPanel (g, kb, 2.5f);
    }

    // ══ EGO BUTTON HOUSING ══════════════════════════════════════════════
    {
        const auto eb = egoButton.getBounds().toFloat().expanded (5.0f);
        WC::drawSunkenPanel (g, eb, 2.0f);
    }

    // ══ LOOP MODE BUTTONS SECTION ════════════════════════════════════════
    {
        const auto lh = loopLabel.getBounds().toFloat();
        const float loopAreaY = lh.getY() - 6.0f;
        const float loopAreaH = 50.0f;
        juce::Rectangle<float> loopPanel (18.0f, loopAreaY, W - 36.0f, loopAreaH);
        WC::drawSunkenPanel (g, loopPanel, 2.0f);
    }

    // ══ SEPARATOR ═══════════════════════════════════════════════════════
    g.setColour (WC::black);
    g.fillRect (20.0f, 392.0f, W - 40.0f, 1.5f);
    g.setColour (WC::dimGrey.withAlpha (0.25f));
    g.fillRect (20.0f, 393.5f, W - 40.0f, 0.8f);
    // Blip
    WC::drawRaisedPanel (g, juce::Rectangle<float> ((float)(W / 2) - 4.0f, 390.0f, 8.0f, 5.0f), 1.0f);

    // ══ BOTTOM BAR ══════════════════════════════════════════════════════
    {
        juce::Rectangle<float> botBar (0.0f, H - 30.0f, W, 30.0f);
        WC::drawRaisedPanel (g, botBar, 2.0f);
        g.setColour (WC::black);
        g.fillRect (0.0f, H - 30.0f, W, 1.5f);

        g.setFont (monoFont (7.0f));
        g.setColour (WC::dimGrey);
        g.drawText ("layer_07", 8, (int)(H - 20), 80, 12, juce::Justification::left);
        g.drawText ("v1.0.0",   (int)W - 54, (int)(H - 20), 48, 12, juce::Justification::right);
        g.setColour (WC::mid);
        g.setFont (monoFont (6.0f));
        g.drawText ("present_wired.exe", (int)(W * 0.32f), (int)(H - 20), 130, 12, juce::Justification::left);
    }

    // ══ FILM GRAIN ══════════════════════════════════════════════════════
    drawFilmGrain (g);

    // ══ CRT OVERLAY ═════════════════════════════════════════════════════
    drawCRTOverlay (g);

    // ══ SCRATCH LINES ════════════════════════════════════════════════════
    drawScratchLines (g);
}

// ─────────────────────────────────────────────
void WiredAudioProcessorEditor::resized()
{
    const int W = getWidth();

    titleLabel.setBounds (0, 8, W, 26);

    // TV screen — centred, upper section
    constexpr int tvW = 300, tvH = 200;
    const int tvX = (W - tvW) / 2;
    tvScreen.setBounds (tvX, 48, tvW, tvH);

    // Waveform — below TV
    waveform.setBounds (20, 258, W - 40, 48);

    // Controls area starts at y=318
    constexpr int ctrlY    = 318;
    constexpr int knobSize = 80;
    constexpr int btnSize  = 72;

    // Rate knob — left side
    const int knobX = W / 2 - 90 - knobSize / 2;
    rateKnob.setBounds  (knobX, ctrlY, knobSize, knobSize);
    rateLabel.setBounds (knobX, ctrlY + knobSize + 2, knobSize, 14);

    // Ego button — right side
    const int btnX = W / 2 + 90 - btnSize / 2;
    egoButton.setBounds (btnX, ctrlY + 4, btnSize, btnSize);

    // Loop mode buttons row — below controls
    constexpr int loopY = 412;
    constexpr int loopBtnW = 50, loopBtnH = 22;
    const int totalBtnsW = 4 * loopBtnW + 3 * 4;
    const int loopX = (W - totalBtnsW) / 2;

    loopLabel.setBounds (0, loopY - 16, W, 14);
    btnHalf.setBounds (loopX + 0 * (loopBtnW + 4), loopY, loopBtnW, loopBtnH);
    btnOne.setBounds  (loopX + 1 * (loopBtnW + 4), loopY, loopBtnW, loopBtnH);
    btnTwo.setBounds  (loopX + 2 * (loopBtnW + 4), loopY, loopBtnW, loopBtnH);
    btnInf.setBounds  (loopX + 3 * (loopBtnW + 4), loopY, loopBtnW, loopBtnH);
}
