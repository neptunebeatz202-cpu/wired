#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Monochrome palette — everything is black, white, and mid-greys.
//  Intentionally harsh and uneven.
// ─────────────────────────────────────────────────────────────────────────────
namespace WC
{
    const juce::Colour black   { 0xFF000000 };
    const juce::Colour nearBlk { 0xFF080808 };
    const juce::Colour dark    { 0xFF111111 };
    const juce::Colour mid     { 0xFF2A2A2A };
    const juce::Colour dimGrey { 0xFF555555 };
    const juce::Colour grey    { 0xFF888888 };
    const juce::Colour light   { 0xFFBBBBBB };
    const juce::Colour white   { 0xFFFFFFFF };
    const juce::Colour offWhite{ 0xFFDDDDDD };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static juce::Font monoFont (float size, int style = juce::Font::plain)
{
    return juce::Font (juce::Font::getDefaultMonospacedFontName(), size, style);
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
    const auto  b      = getLocalBounds().toFloat().reduced (6.0f);
    const float cx     = b.getCentreX();
    const float cy     = b.getCentreY();
    const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

    const float startA = juce::MathConstants<float>::pi * 1.25f;
    const float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float normV  = static_cast<float> ((getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    const float valA   = startA + normV * (endA - startA);

    // Background circle — rough, not perfectly round
    g.setColour (WC::dark);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Track
    {
        juce::Path track;
        track.addCentredArc (cx, cy, radius * 0.82f, radius * 0.82f, 0.0f, startA, endA, true);
        g.setColour (WC::mid);
        g.strokePath (track, juce::PathStrokeType (2.0f));
    }

    // Value arc — jagged dashed feel via short segments
    {
        g.setColour (WC::white);
        juce::Path val;
        val.addCentredArc (cx, cy, radius * 0.82f, radius * 0.82f, 0.0f, startA, valA, true);
        juce::PathStrokeType pst (2.5f);
        float dashes[] = { 4.0f, 2.0f };
        pst.createDashedStroke (val, val, dashes, 2);
        g.strokePath (val, juce::PathStrokeType (2.5f));
    }

    // Outer rim — rough rectangle-ish (not smooth)
    g.setColour (WC::dimGrey);
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    // Body fill
    g.setColour (WC::nearBlk);
    g.fillEllipse (cx - radius * 0.65f, cy - radius * 0.65f, radius * 1.3f, radius * 1.3f);
    g.setColour (WC::mid);
    g.drawEllipse (cx - radius * 0.65f, cy - radius * 0.65f, radius * 1.3f, radius * 1.3f, 1.0f);

    // Pointer
    g.setColour (WC::white);
    const float pLen = radius * 0.52f;
    g.drawLine (cx, cy, cx + pLen * std::sin (valA), cy - pLen * std::cos (valA), 2.0f);

    // Centre screw-dot
    g.setColour (WC::grey);
    g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);

    // Tick marks — slightly uneven lengths for character
    juce::Random tickRng (77);
    for (int i = 0; i <= 10; ++i)
    {
        const float t      = static_cast<float> (i) / 10.0f;
        const float a      = startA + t * (endA - startA);
        const float jitter = 1.0f + tickRng.nextFloat() * 2.0f;
        const float inner  = radius * 0.87f;
        const float outer  = radius * (0.94f + jitter * 0.01f);
        g.setColour (i % 5 == 0 ? WC::grey : WC::mid);
        g.drawLine (cx + inner * std::sin (a), cy - inner * std::cos (a),
                    cx + outer * std::sin (a), cy - outer * std::cos (a), 1.0f);
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
    frames.clear();
    currentFrame = frameCounter = 0;
    if (data == nullptr || dataSize == 0) return;

    juce::MemoryInputStream stream (data, dataSize, false);
    juce::GIFImageFormat gif;
    if (! gif.canUnderstand (stream)) return;
    stream.setPosition (0);
    auto img = gif.decodeImage (stream);
    if (img.isValid())
    {
        // Convert to greyscale for monochrome look
        auto grey = img.convertedToFormat (juce::Image::RGB);
        frames.add (new juce::Image (grey));
    }
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
    const auto  screen = b.reduced (5.0f);

    // Bezel
    g.setColour (WC::dark);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (WC::mid);
    g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 1.5f);

    // Screen bg
    g.setColour (WC::black);
    g.fillRect (screen);

    g.saveState();
    g.reduceClipRegion (screen.toNearestInt());

    if (frames.size() > 0)
    {
        const juce::Image& frame = *frames[currentFrame];

        // Draw greyscale image
        g.drawImage (frame,
                     (int) screen.getX(),    (int) screen.getY(),
                     (int) screen.getWidth(), (int) screen.getHeight(),
                     0, 0, frame.getWidth(), frame.getHeight());

        // Heavy desaturate overlay
        g.setColour (juce::Colour (0xBB000000));
        g.fillRect (screen);

        // Re-draw with low alpha for ghost effect
        g.setOpacity (0.45f);
        g.drawImage (frame,
                     (int) screen.getX(),    (int) screen.getY(),
                     (int) screen.getWidth(), (int) screen.getHeight(),
                     0, 0, frame.getWidth(), frame.getHeight());
        g.setOpacity (1.0f);
    }

    // Horizontal glitch strip
    if (glitchAlpha > 0.02f)
    {
        const float gy    = screen.getY() + static_cast<float> (glitchLineY % juce::jmax (1, (int) screen.getHeight()));
        const float shift = hShift;
        g.setColour (WC::white.withAlpha (glitchAlpha * 0.8f));
        g.fillRect (screen.getX() + shift, gy, screen.getWidth() * 0.7f, 2.0f);
        g.setColour (WC::black.withAlpha (glitchAlpha * 0.4f));
        g.fillRect (screen.getX() + shift - 3.0f, gy + 2.0f, screen.getWidth() * 0.5f, 1.0f);
    }

    // Corruption block
    if (corruptAlpha > 0.02f)
    {
        const float cy2 = screen.getY() + static_cast<float> (corruptBlock % juce::jmax (1, (int) screen.getHeight()));
        g.setColour (WC::white.withAlpha (corruptAlpha * 0.25f));
        g.fillRect (screen.getX(), cy2, screen.getWidth(), 8.0f + rng.nextFloat() * 12.0f);
    }

    // Scanlines
    g.setColour (juce::Colour (0x44000000));
    for (float y = screen.getY(); y < screen.getBottom(); y += 2.0f)
        g.drawHorizontalLine (static_cast<int> (y), screen.getX(), screen.getRight());

    // Moving bright band
    {
        const float sy = screen.getY() + scanlineOffset;
        g.setColour (WC::white.withAlpha (0.04f));
        g.fillRect (screen.getX(), sy, screen.getWidth(), 15.0f);
    }

    // Vignette
    {
        juce::ColourGradient vig (juce::Colour (0x00000000), screen.getCentreX(), screen.getCentreY(),
                                   juce::Colour (0xDD000000), screen.getX(), screen.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (screen);
    }

    g.restoreState();

    // Corner scratches on bezel
    g.setColour (WC::dimGrey.withAlpha (0.5f));
    g.drawLine (b.getX(), b.getY(), b.getX() + 8, b.getY(), 1.0f);
    g.drawLine (b.getX(), b.getY(), b.getX(), b.getY() + 8, 1.0f);
    g.drawLine (b.getRight(), b.getY(), b.getRight() - 8, b.getY(), 1.0f);
    g.drawLine (b.getRight(), b.getY(), b.getRight(), b.getY() + 8, 1.0f);
    g.drawLine (b.getX(), b.getBottom(), b.getX() + 8, b.getBottom(), 1.0f);
    g.drawLine (b.getX(), b.getBottom(), b.getX(), b.getBottom() - 8, 1.0f);
    g.drawLine (b.getRight(), b.getBottom(), b.getRight() - 8, b.getBottom(), 1.0f);
    g.drawLine (b.getRight(), b.getBottom(), b.getRight(), b.getBottom() - 8, 1.0f);
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

    // Background
    g.setColour (WC::black);
    g.fillRect (b);

    // Subtle grid
    g.setColour (WC::dark.brighter (0.15f));
    g.drawHorizontalLine ((int) midY, b.getX(), b.getRight());
    for (int x = 0; x < (int) W; x += 32)
        g.drawVerticalLine (x, b.getY(), b.getBottom());

    // Waveform — scrolling phosphor line
    const int N = WiredAudioProcessor::VIS_SIZE;
    juce::Path wave;
    bool started = false;
    for (int i = 0; i < (int) W; ++i)
    {
        const int idx = ((int) (static_cast<float> (i) / W * N) + scrollOffset) % N;
        const float s = localBuf[static_cast<size_t> (idx)];
        const float y = midY - s * (H * 0.42f);
        if (! started) { wave.startNewSubPath (b.getX() + i, y); started = true; }
        else            wave.lineTo (b.getX() + i, y);
    }

    // Glow behind
    g.setColour (WC::white.withAlpha (0.06f));
    g.strokePath (wave, juce::PathStrokeType (4.0f));

    // Main line
    g.setColour (WC::white.withAlpha (0.85f));
    g.strokePath (wave, juce::PathStrokeType (1.2f));

    // Random glitch pixel noise
    g.setColour (WC::white.withAlpha (0.35f));
    juce::Random noiseRng (static_cast<juce::int64> (scrollOffset * 17));
    for (int i = 0; i < 8; ++i)
        g.fillRect (noiseRng.nextInt ((int) W), noiseRng.nextInt ((int) H), 1, 1);

    // Border
    g.setColour (WC::dimGrey);
    g.drawRect (b, 1.0f);
}

// ═════════════════════════════════════════════
//  WiredEgoButton
// ═════════════════════════════════════════════
WiredEgoButton::WiredEgoButton() { setButtonText (""); }

void WiredEgoButton::paintButton (juce::Graphics& g, bool highlighted, bool /*down*/)
{
    const auto  b   = getLocalBounds().toFloat().reduced (2.0f);
    const bool  on  = getToggleState();

    // Outer glow when on
    if (on)
    {
        g.setColour (WC::white.withAlpha (0.12f));
        g.fillRoundedRectangle (b.expanded (4.0f), 3.0f);
    }

    // Body — deliberately rough rectangle, not rounded
    g.setColour (on ? WC::mid : WC::dark);
    g.fillRect (b);

    // Scratchy border — not uniform line width
    g.setColour (on ? WC::white : WC::dimGrey);
    g.drawRect (b, on ? 2.0f : 1.0f);

    // Diagonal scratch mark (character)
    g.setColour (WC::dimGrey.withAlpha (0.4f));
    g.drawLine (b.getX() + 3, b.getBottom() - 3, b.getX() + 8, b.getBottom() - 8, 1.0f);

    // LED pip
    const float pr = 4.0f;
    const float px = b.getCentreX() - pr;
    const float py = b.getCentreY() - 10.0f - pr;
    g.setColour (on ? WC::white : WC::dimGrey);
    g.fillEllipse (px, py, pr * 2.0f, pr * 2.0f);
    if (on)
    {
        g.setColour (WC::white.withAlpha (0.4f));
        g.fillEllipse (px - 2.0f, py - 2.0f, pr * 2.0f + 4.0f, pr * 2.0f + 4.0f);
    }

    // Label
    g.setColour (on ? WC::white : WC::grey);
    g.setFont (monoFont (10.0f, on ? juce::Font::bold : juce::Font::plain));
    g.drawText ("EGO", b.withTop (b.getCentreY() + 2.0f).toNearestInt(), juce::Justification::centred);

    if (highlighted)
    {
        g.setColour (WC::white.withAlpha (0.07f));
        g.fillRect (b);
    }
}

// ═════════════════════════════════════════════
//  LoopModeButton
// ═════════════════════════════════════════════
LoopModeButton::LoopModeButton (const juce::String& label) { setButtonText (label); }

void LoopModeButton::paintButton (juce::Graphics& g, bool highlighted, bool /*down*/)
{
    const auto  b   = getLocalBounds().toFloat().reduced (1.5f);
    const bool  sel = getToggleState();

    // Filled when selected, hollow otherwise — no rounded corners
    g.setColour (sel ? WC::white : WC::dark);
    g.fillRect (b);

    g.setColour (sel ? WC::dark : WC::dimGrey);
    g.drawRect (b, 1.0f);

    g.setColour (sel ? WC::black : WC::grey);
    g.setFont (monoFont (10.0f, sel ? juce::Font::bold : juce::Font::plain));
    g.drawText (getButtonText(), b.toNearestInt(), juce::Justification::centred);

    if (highlighted && !sel)
    {
        g.setColour (WC::white.withAlpha (0.08f));
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
    titleLabel.setText ("WIRED", juce::dontSendNotification);
    titleLabel.setFont (monoFont (18.0f, juce::Font::bold));
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
    // (generated from resources/lain_img.jpg by juce_add_binary_data)
    // The exact symbol name is derived from the filename:
    //   lain_img.jpg → BinaryData::lain_img_jpg / BinaryData::lain_img_jpgSize
    // We load it here so it's available for drawing in paint().
    // If BinaryData isn't linked yet during development, this will be null
    // and paint() will simply skip the image draw.
    // Uncomment the two lines below once the project compiles with BinaryData:
    //
    // lainImage = juce::ImageCache::getFromMemory (BinaryData::lain_img_jpg,
    //                                              BinaryData::lain_img_jpgSize);

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

    // ── Pure black background ─────────────────────────────────────────────
    g.setColour (WC::black);
    g.fillRect (b);

    // ── Wire texture — very faint ─────────────────────────────────────────
    g.setColour (WC::dark.brighter (0.08f));
    g.strokePath (wirePattern, juce::PathStrokeType (0.4f));

    // ── Lain image — ghostly background watermark ───────────────────────────
    if (lainImage.isValid())
    {
        // Draw desaturated Lain as a very faint ghost behind everything
        g.setOpacity (0.07f);
        g.drawImage (lainImage,
                     (int)(W * 0.18f), (int)(H * 0.05f),
                     (int)(W * 0.64f), (int)(H * 0.55f),
                     0, 0, lainImage.getWidth(), lainImage.getHeight());
        g.setOpacity (1.0f);
    }

    // ── Rough grid ───────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xFF111111));
    for (float y = 0.0f; y < H; y += 24.0f)
        g.drawHorizontalLine (static_cast<int> (y), 0.0f, W);
    for (float x = 0.0f; x < W; x += 24.0f)
        g.drawVerticalLine   (static_cast<int> (x), 0.0f, H);

    // ── Top bar — deliberately uneven height ──────────────────────────────
    g.setColour (WC::dark);
    g.fillRect (0.0f, 0.0f, W, 42.0f);
    g.setColour (WC::dimGrey);
    g.drawHorizontalLine (42, 0.0f, W);
    // Extra scratch line
    g.setColour (WC::mid);
    g.drawHorizontalLine (41, 0.0f, W * 0.6f);

    // Pulsing status marks — dots that stutter when effect is on
    const bool active = audioProcessor.isEffectActive();
    for (int i = 0; i < 7; ++i)
    {
        const float phase   = noisePhase * 4.0f + static_cast<float> (i) * 0.9f;
        const float alpha   = active
            ? juce::jlimit (0.1f, 1.0f, 0.4f + 0.6f * std::abs (std::sin (phase)))
            : 0.15f;
        const float sz      = active && (i % 2 == 0) ? 5.0f : 4.0f;
        g.setColour (WC::white.withAlpha (alpha));
        g.fillRect (8.0f + static_cast<float> (i) * 10.0f, 18.0f, sz, sz);
    }

    // ── Bottom bar ────────────────────────────────────────────────────────
    g.setColour (WC::dark);
    g.fillRect (0.0f, H - 32.0f, W, 32.0f);
    g.setColour (WC::dimGrey);
    g.drawHorizontalLine ((int) (H - 32.0f), 0.0f, W);

    g.setFont (monoFont (7.5f));
    g.setColour (WC::dimGrey);
    g.drawText ("layer 07", 8, (int)(H - 22), 80, 14, juce::Justification::left);
    g.drawText ("v1.0.0",   (int)W - 55, (int)(H - 22), 48, 14, juce::Justification::right);

    // Some random static text fragments for character
    g.setColour (WC::mid);
    g.setFont (monoFont (6.5f));
    g.drawText ("present_wired.exe", (int)(W * 0.35f), (int)(H - 22), 120, 14, juce::Justification::left);

    // ── Separator above controls ──────────────────────────────────────────
    g.setColour (WC::mid);
    g.drawHorizontalLine (390, 20.0f, W - 20.0f);
    // Small blip
    g.setColour (WC::dimGrey);
    g.fillRect ((int)(W * 0.5f) - 2, 388, 4, 4);

    // ── TV screen corner marks ────────────────────────────────────────────
    auto drawCorner = [&] (float x, float y, bool fx, bool fy)
    {
        const float sx = fx ? -1.0f : 1.0f, sy2 = fy ? -1.0f : 1.0f;
        g.drawLine (x, y, x + sx * 10, y, 1.0f);
        g.drawLine (x, y, x, y + sy2 * 10, 1.0f);
    };
    g.setColour (WC::dimGrey);
    const auto tv = tvScreen.getBounds().toFloat().expanded (3.0f);
    drawCorner (tv.getX(),     tv.getY(),      false, false);
    drawCorner (tv.getRight(), tv.getY(),      true,  false);
    drawCorner (tv.getX(),     tv.getBottom(), false, true);
    drawCorner (tv.getRight(), tv.getBottom(), true,  true);

    // ── Film grain ────────────────────────────────────────────────────────
    drawFilmGrain (g);

    // ── CRT overlay ───────────────────────────────────────────────────────
    drawCRTOverlay (g);

    // ── Film scratch lines ────────────────────────────────────────────────
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
