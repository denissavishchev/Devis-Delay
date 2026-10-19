#include "PluginEditor.h"
#include <map>

using namespace theme;

static juce::Font mono (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

static juce::Font sans (float h)
{
    return juce::Font (juce::FontOptions (h, juce::Font::bold));
}

//==============================================================================
namespace
{
    float hash2 (int x, int y, int seed)
    {
        uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffff) / 16777215.0f;
    }

    float vnoise (float x, float y, int seed)
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash2 (xi, yi, seed), b = hash2 (xi + 1, yi, seed);
        const float c = hash2 (xi, yi + 1, seed), d = hash2 (xi + 1, yi + 1, seed);
        return juce::jmap (fy, juce::jmap (fx, a, b), juce::jmap (fx, c, d));
    }

    float fbm (float x, float y, int seed, int octaves)
    {
        float v = 0.0f, a = 0.5f;
        for (int i = 0; i < octaves; ++i)
        {
            v += a * vnoise (x, y, seed + i * 7);
            x *= 2.0f; y *= 2.0f; a *= 0.5f;
        }
        return v;
    }

    float sstep (float a, float b, float x)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
        return t * t * (3.0f - 2.0f * t);
    }

    // Dirty off-white enamel: yellowed edges, soft smudges, grain, specks and light scuffs
    juce::Image makeDirtyWhite (int w, int h, int scale)
    {
        const int W = w * scale, H = h * scale;
        juce::Image img (juce::Image::ARGB, W, H, false);
        {
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            const auto top = juce::Colour (0xfff0ede2), bot = juce::Colour (0xffe1dccd);
            const auto grime = juce::Colour (0xffa9997a), yellowed = juce::Colour (0xffd8c99b);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const float lx = (float) x / (float) scale, ly = (float) y / (float) scale;
                    const float u = lx / (float) w, v = ly / (float) h;
                    const float big = fbm (lx / 75.0f, ly / 75.0f, 31, 4);
                    const float mid = fbm (lx / 16.0f, ly / 16.0f, 47, 3);
                    const float fine = hash2 (x, y, 5);
                    const float edge = std::pow (juce::jmax (std::abs (u - 0.5f), std::abs (v - 0.5f)) * 2.0f, 2.5f);
                    const float streak = fbm (lx / 5.0f, ly / 120.0f, 61, 3);

                    auto c = top.interpolatedWith (bot, v);
                    c = c.interpolatedWith (yellowed, juce::jlimit (0.0f, 0.7f, edge * 0.8f));
                    c = c.interpolatedWith (grime, sstep (0.52f, 0.80f, big * 0.8f + mid * 0.2f) * 0.38f);
                    c = c.interpolatedWith (grime.darker (0.4f), sstep (0.58f, 0.9f, streak) * 0.22f * (1.0f - 0.5f * v));
                    float m = 0.965f + 0.05f * fine;
                    if (hash2 (x / scale, y / scale, 77) > 0.9965f) m *= 0.7f;   // dirt specks
                    bd.setPixelColour (x, y, c.withMultipliedBrightness (m));
                }
        }
        juce::Graphics g (img);
        juce::Random rnd (4242);
        for (int i = 0; i < 90; ++i)
        {
            const float x = rnd.nextFloat() * (float) W, y = rnd.nextFloat() * (float) H;
            const float ang = (rnd.nextFloat() - 0.5f) * 3.0f;
            const float len = (6.0f + rnd.nextFloat() * 40.0f) * (float) scale;
            g.setColour (juce::Colour (rnd.nextFloat() < 0.5f ? 0xff6f6552 : 0xffffffff).withAlpha (0.05f + rnd.nextFloat() * 0.10f));
            g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.6f * (float) scale);
        }
        return img;
    }

    const juce::Image& bodyTexture()
    {
        static juce::Image img = makeDirtyWhite (820, 580, 2);
        return img;
    }

    // warm-grey matte module plate (grain, a few scuffs and specks)
    const juce::Image& plateTexture (int w, int h)
    {
        static std::map<std::pair<int, int>, juce::Image> cache;
        auto& img = cache[{ w, h }];
        if (img.isValid()) return img;

        const int scale = 2, W = w * scale, H = h * scale;
        img = juce::Image (juce::Image::ARGB, W, H, false);
        {
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            const auto top = juce::Colour (0xff938f85), bot = juce::Colour (0xff6f6c64);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const float u = (float) x / (float) W, v = (float) y / (float) H;
                    auto c = top.interpolatedWith (bot, v);
                    const float cloud = fbm ((float) x / 90.0f, (float) y / 90.0f, 13 + w, 3);
                    c = c.withMultipliedBrightness (0.94f + 0.12f * cloud);
                    c = c.brighter (0.10f * std::max (0.0f, 1.0f - std::sqrt (u * u + v * v * 2.0f)));
                    const float grain = 0.97f + 0.06f * hash2 (x, y, 19);
                    bd.setPixelColour (x, y, c.withMultipliedBrightness (grain));
                }
        }
        juce::Graphics g (img);
        juce::Random rnd (w * 7 + h);
        for (int i = 0; i < 26; ++i)
        {
            const float x = rnd.nextFloat() * (float) W, y = rnd.nextFloat() * (float) H;
            const float ang = (rnd.nextFloat() - 0.5f) * 3.0f;
            const float len = (6.0f + rnd.nextFloat() * 36.0f) * (float) scale;
            g.setColour (juce::Colour (rnd.nextFloat() < 0.6f ? 0xfff6f3ea : 0xff3a3832).withAlpha (0.06f + rnd.nextFloat() * 0.12f));
            g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.6f * (float) scale);
        }
        return img;
    }

    void drawScrew (juce::Graphics& g, juce::Point<float> c, float r = 4.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillEllipse (c.x - r + 0.6f, c.y - r + 1.2f, r * 2, r * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6b69), c.x - r * 0.5f, c.y - r * 0.6f,
                                                 juce::Colour (0xff222322), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
        g.setColour (juce::Colour (0xff0e0e0d));
        g.drawLine (c.x - r * 0.6f, c.y, c.x + r * 0.6f, c.y, 1.3f);
        g.drawLine (c.x, c.y - r * 0.6f, c.x, c.y + r * 0.6f, 1.3f);
    }

    void drawModule (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.20f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.0f), 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (r.translated (0.0f, 6.0f).expanded (3.0f), 12.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path p;
            p.addRoundedRectangle (r, 9.0f);
            g.reduceClipRegion (p);
            g.setOpacity (1.0f);
            g.drawImage (plateTexture ((int) r.getWidth(), (int) r.getHeight()), r);
        }
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (1.0f), 9.0f, 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
        for (auto p : { r.getTopLeft(), r.getTopRight(), r.getBottomLeft(), r.getBottomRight() })
            drawScrew (g, { p.x + (p.x < r.getCentreX() ? 10.0f : -10.0f), p.y + (p.y < r.getCentreY() ? 10.0f : -10.0f) }, 3.4f);
    }

    // white label, slightly sunk into the plate by a thin dark outline; only a faint glow
    void glowText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
                   juce::Justification j = juce::Justification::centred)
    {
        for (float rad : { 2.2f })
            for (int k = 0; k < 8; ++k)
            {
                const float a = (float) k * juce::MathConstants<float>::twoPi / 8.0f;
                g.setColour (juce::Colour (0xfffff6dc).withAlpha (0.035f));
                g.drawText (s, r.translated (std::cos (a) * rad, std::sin (a) * rad), j);
            }
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawText (s, r.translated (0.0f, 1.6f), j);                // soft drop
        for (int k = 0; k < 8; ++k)                                   // thin outline
        {
            const float a = (float) k * juce::MathConstants<float>::twoPi / 8.0f;
            g.setColour (juce::Colour (0xff1c1b18).withAlpha (0.75f));
            g.drawText (s, r.translated (std::cos (a) * 0.9f, std::sin (a) * 0.9f), j);
        }
        g.setColour (juce::Colour (0xfff3efe2));
        g.drawText (s, r, j);
    }

    void printed (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, float size = 11.0f)
    {
        g.setFont (sans (size));
        glowText (g, s, r.toFloat());
    }

    // label: dark ink when idle, glowing LCD green when the key is down
    void keyText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, bool lit)
    {
        if (lit)
        {
            for (float rad : { 3.0f, 1.6f })
                for (int k = 0; k < 8; ++k)
                {
                    const float a = (float) k * juce::MathConstants<float>::twoPi / 8.0f;
                    g.setColour (orange.withAlpha (rad > 2.0f ? 0.06f : 0.12f));
                    g.drawText (s, r.translated (std::cos (a) * rad, std::sin (a) * rad), juce::Justification::centred);
                }
            g.setColour (juce::Colour (0xff4f7d08));
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawText (s, r.translated (0.0f, 1.0f), juce::Justification::centred);
            g.setColour (juce::Colour (0xff2c2b28));
        }
        g.drawText (s, r, juce::Justification::centred);
    }

    // dark housing that joins a row of keys (like the transport-key reference)
    void drawKeyHousing (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (0.5f), 9.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1c1a), 0, r.getY(), juce::Colour (0xff0b0b0a), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.0f);
    }

    // light bevelled key; pressed = darker, sunk into the housing
    juce::Rectangle<float> drawKeycap (juce::Graphics& g, juce::Rectangle<float> r, bool pressed, bool hover)
    {
        const float radius = 6.0f;
        auto face = r.reduced (1.5f);
        if (! pressed)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (face.translated (0.0f, 2.5f), radius);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e7df), 0, face.getY(), juce::Colour (0xffb9b6ac), 0, face.getBottom(), false));
            g.fillRoundedRectangle (face, radius);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (face.reduced (1.2f), radius - 1.0f, 1.0f);
            g.setColour (juce::Colour (0xff3d3c38));
            g.drawRoundedRectangle (face, radius, 1.0f);
            if (hover)
            {
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.fillRoundedRectangle (face, radius);
            }
        }
        else
        {
            face = face.translated (0.0f, 1.5f);
            // same colour as the raised key, only shaded by an inner-shadow gradient
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe2e0d8), 0, face.getY(), juce::Colour (0xffb9b6ac), 0, face.getBottom(), false));
            g.fillRoundedRectangle (face, radius);
            {
                juce::Graphics::ScopedSaveState ss (g);
                juce::Path p;
                p.addRoundedRectangle (face, radius);
                g.reduceClipRegion (p);
                g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.50f), 0, face.getY(),
                                                         juce::Colours::transparentBlack, 0, face.getY() + face.getHeight() * 0.65f, false));
                g.fillRect (face);
                g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.28f), face.getX(), 0,
                                                         juce::Colours::transparentBlack, face.getX() + 12.0f, 0, false));
                g.fillRect (face);
                g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.28f), face.getRight(), 0,
                                                         juce::Colours::transparentBlack, face.getRight() - 12.0f, 0, false));
                g.fillRect (face);
            }
            g.setColour (juce::Colour (0xff1c1c1a));
            g.drawRoundedRectangle (face, radius, 1.2f);
        }
        return face;
    }
}

//==============================================================================
IndustrialLookAndFeel::IndustrialLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, ink);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::highlightColourId, orangeD);
}

juce::Label* IndustrialLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (mono (12.0f, true));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void IndustrialLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    g.setFont (l.getFont());
    glowText (g, l.getText(), l.getLocalBounds().toFloat());
}

void IndustrialLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                              float pos, float, float, juce::Slider::SliderStyle,
                                              juce::Slider&)
{
    const float cx = (float) x + (float) w * 0.5f;
    // the glass strip is a little longer than the cap travel, so a bit of it stays visible past the cap
    // JUCE keeps the cap centre inside [y, y+h]; the bezel reaches past that by the thumb radius,
    // so the whole cap always stays inside the glass with a strip of it visible beyond
    const auto bezel = juce::Rectangle<float> (cx - 15.0f, (float) y - 25.0f, 30.0f, (float) h + 50.0f);
    const auto glass = bezel.reduced (4.5f);

    // printed scale marks
    g.setColour (ink.withAlpha (0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = (float) y + (float) h * (1.0f - (float) i / 10.0f);
        const float len = (i % 5 == 0) ? 9.0f : 5.0f;
        g.drawLine (cx - 21.0f - len, ty, cx - 21.0f, ty, 1.1f);
        g.drawLine (cx + 21.0f, ty, cx + 21.0f + len, ty, 1.1f);
    }

    // brushed-grey recessed bezel
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.fillRoundedRectangle (bezel.translated (0.0f, 1.2f), 11.0f);
    {
        juce::ColourGradient bg (juce::Colour (0xff5d5b55), bezel.getX(), 0, juce::Colour (0xff5d5b55), bezel.getRight(), 0, false);
        bg.addColour (0.5, juce::Colour (0xff8f8d85));
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bezel, 11.0f);
    }
    g.setColour (juce::Colour (0xff1c1c1a));
    g.drawRoundedRectangle (bezel, 11.0f, 1.0f);

    // yellow glass tube: bright below the cap, dimmed above it
    juce::Path tube;
    tube.addRoundedRectangle (glass, 7.5f);
    g.saveState();
    g.reduceClipRegion (tube);
    {
        juce::ColourGradient gg (juce::Colour (0xffb59f10), glass.getX(), 0, juce::Colour (0xffb59f10), glass.getRight(), 0, false);
        gg.addColour (0.35, juce::Colour (0xfff3e23a));
        gg.addColour (0.5, juce::Colour (0xfffff27a));
        gg.addColour (0.75, juce::Colour (0xffe0cb22));
        g.setGradientFill (gg);
        g.fillRect (glass);
    }
    g.setColour (juce::Colour (0xff7a6a0a).withAlpha (0.35f));              // fine ribbing
    for (float ty = glass.getY() + 2.0f; ty < glass.getBottom(); ty += 3.0f)
        g.drawHorizontalLine ((int) ty, glass.getX() + 3.0f, glass.getRight() - 3.0f);
    g.setColour (juce::Colour (0xff1b1a10).withAlpha (0.55f));               // unlit part above the cap
    g.fillRect (glass.getX(), glass.getY(), glass.getWidth(), juce::jmax (0.0f, pos - glass.getY()));
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), 0, glass.getY(), juce::Colours::transparentBlack, 0, glass.getY() + 10.0f, false));
    g.fillRect (glass);
    g.restoreState();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (tube, juce::PathStrokeType (1.0f));

    // soft grey pill cap
    const auto cap = juce::Rectangle<float> (cx - 13.5f, pos - 17.0f, 27.0f, 34.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (cap.translated (1.5f, 3.5f), 12.0f);
    {
        juce::ColourGradient cg (juce::Colour (0xff8e8c84), cap.getX(), 0, juce::Colour (0xff8e8c84), cap.getRight(), 0, false);
        cg.addColour (0.35, juce::Colour (0xffd8d6ce));
        cg.addColour (0.7, juce::Colour (0xffb1afa6));
        g.setGradientFill (cg);
        g.fillRoundedRectangle (cap, 12.0f);
    }
    g.setColour (juce::Colour (0xff2b2a27));
    g.drawRoundedRectangle (cap, 12.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (cap.reduced (1.6f), 10.5f, 0.9f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine (cap.getX() + 6.0f, pos, cap.getRight() - 6.0f, pos, 1.2f);
}

void IndustrialLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                  const juce::Colour&, bool hl, bool down)
{
    drawKeycap (g, b.getLocalBounds().toFloat(), b.getToggleState() || down, hl);
}

void IndustrialLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool pressed = b.getToggleState() || down;
    auto face = b.getLocalBounds().toFloat().reduced (1.5f).translated (0.0f, pressed ? 1.5f : 0.0f);
    g.setFont (sans (b.getProperties().contains ("big") ? 10.5f : 11.0f));
    keyText (g, b.getButtonText(), face, b.getToggleState());
}

void IndustrialLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool down)
{
    const bool pressed = b.getToggleState() || down;
    auto face = drawKeycap (g, b.getLocalBounds().toFloat(), pressed, hl);
    g.setFont (sans (11.0f));
    keyText (g, b.getButtonText(), face, b.getToggleState());
}

//==============================================================================
SpectrumView::SpectrumView (DelayProcessor& p)
    : proc (p),
      fft (DelayProcessor::fftOrder),
      window ((size_t) DelayProcessor::fftSize, juce::dsp::WindowingFunction<float>::hann),
      fftData ((size_t) DelayProcessor::fftSize * 2, 0.0f),
      levels ((size_t) DelayProcessor::fftSize / 2, -100.0f)
{
    startTimerHz (30);
}

void SpectrumView::timerCallback()
{
    constexpr int N = DelayProcessor::fftSize;

    if (proc.scopeReady.load())
    {
        std::copy (proc.scopeBlock, proc.scopeBlock + N, fftData.begin());
        std::fill (fftData.begin() + N, fftData.end(), 0.0f);
        proc.scopeReady = false;

        window.multiplyWithWindowingTable (fftData.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int i = 0; i < N / 2; ++i)
        {
            const float db = juce::Decibels::gainToDecibels (fftData[(size_t) i] / ((float) N * 0.25f), -120.0f);
            levels[(size_t) i] = juce::jmax (db, levels[(size_t) i] - 3.0f);
        }
    }
    else
    {
        for (auto& l : levels) l = juce::jmax (-120.0f, l - 1.5f);
    }
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bezel = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff1f201e));
    g.fillRoundedRectangle (bezel, 9.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (bezel.reduced (1.0f), 9.0f, 1.0f);

    const auto b = bezel.reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (lcdA, 0, b.getY(), lcdB, 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, 4.0f);
    g.saveState();
    juce::Path lcdClip;
    lcdClip.addRoundedRectangle (b, 4.0f);
    g.reduceClipRegion (lcdClip);

    const auto area = b.reduced (10.0f, 6.0f).withTrimmedBottom (12.0f).withTrimmedTop (10.0f);
    const float sr = (float) proc.currentSampleRate.load();
    constexpr float minDb = -90.0f, maxDb = 0.0f;
    constexpr float fLo = 20.0f, fHi = 20000.0f;

    auto xForFreq = [&] (float f) { return area.getX() + area.getWidth() * std::log (f / fLo) / std::log (fHi / fLo); };
    auto yForDb   = [&] (float d) { return juce::jmap (juce::jlimit (minDb, maxDb, d), minDb, maxDb, area.getBottom(), area.getY()); };

    g.setColour (lcdInk.withAlpha (0.05f));
    for (float yy = b.getY(); yy < b.getBottom(); yy += 3.0f)
        g.drawHorizontalLine ((int) yy, b.getX(), b.getRight());

    g.setFont (mono (10.0f, true));
    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = xForFreq (f);
        g.setColour (lcdInk.withAlpha (0.18f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        if (f == 100.f || f == 1000.f || f == 10000.f || f == 20.f || f == 20000.f)
        {
            g.setColour (lcdInk.withAlpha (0.8f));
            const auto label = f >= 1000.f ? juce::String ((int) (f / 1000.f)) + "k" : juce::String ((int) f);
            g.drawText (label, (int) x - 20, (int) area.getBottom() + 1, 40, 11, juce::Justification::centred);
        }
    }
    for (float d : { -20.f, -40.f, -60.f, -80.f })
    {
        g.setColour (lcdInk.withAlpha (0.18f));
        g.drawHorizontalLine ((int) yForDb (d), area.getX(), area.getRight());
        g.setColour (lcdInk.withAlpha (0.65f));
        g.drawText (juce::String ((int) d), (int) area.getX() + 2, (int) yForDb (d) - 11, 28, 10, juce::Justification::left);
    }

    constexpr int bands = 64;
    const int half = DelayProcessor::fftSize / 2;
    const float bw = area.getWidth() / (float) bands;
    for (int i = 0; i < bands; ++i)
    {
        const float f0 = fLo * std::pow (fHi / fLo, (float) i / (float) bands);
        const float f1 = fLo * std::pow (fHi / fLo, (float) (i + 1) / (float) bands);
        const float b0 = f0 * (float) DelayProcessor::fftSize / sr, b1 = f1 * (float) DelayProcessor::fftSize / sr;
        float db = -120.0f;
        if (b1 - b0 < 1.0f)
        {
            const float bc = 0.5f * (b0 + b1);
            const int i0 = juce::jlimit (0, half - 2, (int) bc);
            const float fr = bc - (float) i0;
            db = levels[(size_t) i0] * (1.0f - fr) + levels[(size_t) i0 + 1] * fr;
        }
        else
            for (int k = juce::jmax (0, (int) b0); k <= juce::jmin (half - 1, (int) b1); ++k)
                db = juce::jmax (db, levels[(size_t) k]);

        const float top = yForDb (db);
        const float x = area.getX() + (float) i * bw + 0.8f;
        g.setColour (lcdInk.withAlpha (0.88f));
        for (float yy = area.getBottom() - 3.0f; yy >= top; yy -= 4.0f)
            g.fillRect (x, yy, bw - 1.6f, 3.0f);
    }

    g.setColour (lcdInk);
    g.setFont (mono (10.0f, true));
    g.drawText ("SPECTRUM  Hz", (int) b.getX() + 8, (int) b.getY() + 3, 120, 12, juce::Justification::left);

    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.3f), 0, b.getY(), juce::Colours::transparentBlack, 0, b.getY() + 14.0f, false));
    g.fillRect (b);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.2f), b.getX(), 0, juce::Colours::transparentBlack, b.getX() + 12.0f, 0, false));
    g.fillRect (b);
    g.restoreState();
}

//==============================================================================
TubeView::TubeView (DelayProcessor& p) : proc (p) { startTimerHz (30); }

void TubeView::timerCallback()
{
    // Filament inertia: it lights up slowly and cools even more slowly. Soft picking stays dim,
    // hard picking drives it up. With TUBE at zero the lamp is cold.
    const float sat = proc.apvts.getRawParameterValue ("sat")->load();
    float target = 0.0f;
    if (sat > 0.001f)
    {
        const float db = juce::Decibels::gainToDecibels (proc.inputLevel.load(), -80.0f);
        const float lvl = std::pow (juce::jlimit (0.0f, 1.0f, (db + 42.0f) / 36.0f), 1.7f);
        const float base = 0.04f + 0.10f * sat;
        target = base + (1.0f - base) * lvl * (0.35f + 0.65f * sat);
    }
    glow += (target - glow) * (target > glow ? 0.14f : 0.04f);
    if (target == 0.0f && glow < 0.004f) glow = 0.0f;
    repaint();
}

void TubeView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float heat = glow;

    // dark window
    juce::Path win;
    win.addRoundedRectangle (b, 10.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141514), 0, 0, juce::Colour (0xff050605), 0, b.getBottom(), false));
    g.fillPath (win);
    g.saveState();
    g.reduceClipRegion (win);

    const float cx = b.getCentreX();
    auto heater = [] (float t)
    {
        const auto red = juce::Colour (0xffa81c0a), warm = juce::Colour (0xffff7a1a), pale = juce::Colour (0xffffd98a);
        return t < 0.5f ? red.interpolatedWith (warm, t * 2.0f) : warm.interpolatedWith (pale, (t - 0.5f) * 2.0f);
    };
    const auto hot = heater (heat);

    // light spilling from the tube onto the dark window
    g.setGradientFill (juce::ColourGradient (hot.withAlpha (0.5f * heat), cx, b.getCentreY() - 4.0f,
                                             juce::Colours::transparentBlack, cx, b.getCentreY() - 4.0f - 95.0f, true));
    g.fillRect (b);

    // ---- glass envelope ----
    const float gw = 33.0f, top = 12.0f, baseTop = b.getBottom() - 32.0f;
    juce::Path glass;
    glass.startNewSubPath (cx - gw, baseTop);
    glass.lineTo (cx - gw, top + gw);
    glass.addArc (cx - gw, top, gw * 2, gw * 2, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
    glass.lineTo (cx + gw, baseTop);
    glass.closeSubPath();

    g.saveState();
    g.reduceClipRegion (glass);
    {
        juce::ColourGradient cg (juce::Colour (0xff23282b), cx - gw, 0, juce::Colour (0xff23282b), cx + gw, 0, false);
        cg.addColour (0.10, juce::Colour (0xff0d1011));
        cg.addColour (0.5, juce::Colour (0xff050606));
        cg.addColour (0.90, juce::Colour (0xff0d1011));
        g.setGradientFill (cg);
        g.fillPath (glass);
    }
    const float plateH = 70.0f, plateTop = baseTop - plateH - 16.0f, plateBot = plateTop + plateH;
    g.setGradientFill (juce::ColourGradient (hot.withAlpha (0.45f * heat), cx, (plateTop + plateBot) * 0.5f,
                                             hot.withAlpha (0.0f), cx, (plateTop + plateBot) * 0.5f - 74.0f, true));
    g.fillPath (glass);

    // mica spacer discs with tabs
    auto mica = [&] (float y)
    {
        const auto r = juce::Rectangle<float> (cx - 30.0f, y - 4.5f, 60.0f, 9.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe7dec3), r.getX(), y, juce::Colour (0xff6d6551), r.getRight(), y, false));
        g.fillEllipse (r);
        g.setColour (hot.withAlpha (0.5f * heat));
        g.fillEllipse (r.reduced (6.0f, 2.2f));
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawEllipse (r, 1.0f);
        for (float hx : { -18.0f, 18.0f })
            g.fillEllipse (cx + hx - 1.8f, y - 1.3f, 3.6f, 2.6f);
    };
    mica (plateBot + 5.0f);

    // two nickel anodes with flared wings, ribs and a lit gap
    for (int side = -1; side <= 1; side += 2)
    {
        const float x0 = side < 0 ? cx - 32.0f : cx + 6.0f;
        const auto pr = juce::Rectangle<float> (x0, plateTop, 26.0f, plateH);
        juce::ColourGradient pg (juce::Colour (0xff1d2021), pr.getX(), 0, juce::Colour (0xff1d2021), pr.getRight(), 0, false);
        pg.addColour (0.25, juce::Colour (0xffb7bcbb));
        pg.addColour (0.50, juce::Colour (0xff5d6261));
        pg.addColour (0.80, juce::Colour (0xff343837));
        g.setGradientFill (pg);
        g.fillRoundedRectangle (pr, 3.0f);
        // outer flared wing
        const float wx = side < 0 ? pr.getX() - 3.0f : pr.getRight() - 1.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8d9291), wx, 0, juce::Colour (0xff2b2e2d), wx + 4.0f, 0, false));
        g.fillRoundedRectangle (wx, pr.getY() + 2.0f, 4.0f, pr.getHeight() - 4.0f, 1.5f);
        // centre crease + horizontal ribs
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine (pr.getCentreX(), pr.getY() + 2.0f, pr.getCentreX(), pr.getBottom() - 2.0f, 1.0f);
        for (float ry = pr.getY() + 7.0f; ry < pr.getBottom() - 4.0f; ry += 9.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawLine (pr.getX() + 1.0f, ry, pr.getRight() - 1.0f, ry, 0.9f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawLine (pr.getX() + 1.0f, ry + 1.0f, pr.getRight() - 1.0f, ry + 1.0f, 0.7f);
        }
        // heater light on the inner edge, cherry glow when driven hard
        const float ex = side < 0 ? pr.getRight() : pr.getX();
        g.setGradientFill (juce::ColourGradient (hot.withAlpha (0.75f * heat), ex, 0, hot.withAlpha (0.0f), ex - (float) side * 11.0f, 0, false));
        g.fillRoundedRectangle (side < 0 ? pr.withLeft (pr.getRight() - 11.0f) : pr.withRight (pr.getX() + 11.0f), 3.0f);
        if (heat > 0.6f)
        {
            g.setColour (juce::Colour (0xffff3a10).withAlpha ((heat - 0.6f) * 0.55f));
            g.fillRoundedRectangle (pr, 3.0f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (pr, 3.0f, 1.0f);
    }
    {
        const auto gap = juce::Rectangle<float> (cx - 3.0f, plateTop + 3.0f, 6.0f, plateH - 6.0f);
        g.setGradientFill (juce::ColourGradient (hot.withAlpha (0.9f * heat), cx, gap.getCentreY(),
                                                 hot.withAlpha (0.0f), cx + 12.0f, gap.getCentreY(), true));
        g.fillRect (gap.expanded (10.0f, 0.0f));
        g.setColour (hot.withAlpha (heat));
        g.fillRoundedRectangle (gap.reduced (1.4f, 0.0f), 1.5f);
        g.setColour (juce::Colour (0xffffeed0).withAlpha (heat * heat));
        g.fillRoundedRectangle (gap.reduced (2.4f, 4.0f), 1.0f);
    }
    mica (plateTop - 5.0f);

    // lead-in wires up to the halo getter
    g.setColour (juce::Colour (0xffa4aaa8));
    g.drawLine (cx - 12.0f, plateTop - 5.0f, cx - 12.0f, top + 24.0f, 1.1f);
    g.drawLine (cx + 12.0f, plateTop - 5.0f, cx + 12.0f, top + 24.0f, 1.1f);
    g.setColour (juce::Colour (0xffc9cecc));
    g.drawEllipse (cx - 14.0f, top + 20.0f, 28.0f, 7.0f, 1.3f);

    // mirrored getter flash on the dome
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f2ef).withAlpha (0.8f), cx, top + 2.0f,
                                             juce::Colours::transparentBlack, cx, top + 30.0f, false));
    g.fillEllipse (cx - gw + 2.0f, top + 1.0f, gw * 2 - 4.0f, 32.0f);
    g.setColour (hot.withAlpha (0.3f * heat));
    g.fillEllipse (cx - 18.0f, top + 5.0f, 36.0f, 14.0f);

    // softbox reflections on the glass
    {
        juce::ColourGradient streak (juce::Colours::white.withAlpha (0.0f), 0, top + gw, juce::Colours::white.withAlpha (0.0f), 0, baseTop, false);
        streak.addColour (0.15, juce::Colours::white.withAlpha (0.38f));
        streak.addColour (0.80, juce::Colours::white.withAlpha (0.26f));
        g.setGradientFill (streak);
        g.fillRoundedRectangle (cx - gw + 5.0f, top + gw - 8.0f, 6.0f, baseTop - top - gw + 2.0f, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (cx - gw + 14.0f, top + gw, 2.0f, baseTop - top - gw - 10.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillEllipse (cx - gw + 8.0f, top + 9.0f, 8.0f, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (cx + gw - 11.0f, top + gw, 3.0f, baseTop - top - gw - 6.0f, 1.5f);
        g.setGradientFill (juce::ColourGradient (hot.withAlpha (0.0f), cx + gw - 11.0f, 0, hot.withAlpha (0.4f * heat), cx + gw, 0, false));
        g.fillRect (cx + gw - 11.0f, top + gw, 11.0f, baseTop - top - gw);
    }
    g.restoreState();

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.strokePath (glass, juce::PathStrokeType (1.4f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (glass, juce::PathStrokeType (0.6f));

    // glass pinch seal, brass collar, bakelite base, pins
    {
        g.setColour (juce::Colour (0xff1a1c1c));
        g.fillRect (cx - 22.0f, baseTop - 7.0f, 44.0f, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (cx - 21.0f, baseTop - 6.0f, cx + 21.0f, baseTop - 6.0f);
        const auto collar = juce::Rectangle<float> (cx - gw - 1.0f, baseTop - 1.0f, gw * 2 + 2.0f, 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeed98f), 0, collar.getY(), juce::Colour (0xff8a6a24), 0, collar.getBottom(), false));
        g.fillRoundedRectangle (collar, 1.5f);
        const auto base = juce::Rectangle<float> (cx - gw - 3.0f, baseTop + 3.0f, gw * 2 + 6.0f, 16.0f);
        juce::ColourGradient bg (juce::Colour (0xff0d0e0e), base.getX(), 0, juce::Colour (0xff0d0e0e), base.getRight(), 0, false);
        bg.addColour (0.28, juce::Colour (0xff3d413f));
        g.setGradientFill (bg);
        g.fillRoundedRectangle (base, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawLine (base.getX() + 4.0f, base.getY() + 1.0f, base.getRight() - 4.0f, base.getY() + 1.0f);
        for (int k = 0; k < 5; ++k)
        {
            const float px = cx - 24.0f + (float) k * 12.0f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e1c4), px, 0, juce::Colour (0xff75663a), px + 2.6f, 0, false));
            g.fillRect (px, base.getBottom(), 2.6f, 8.0f);
        }
    }
    g.restoreState();

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
}

//==============================================================================
DelayEditor::DelayEditor (DelayProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p), tube (p),
      modeAttach (*p.apvts.getParameter ("mode"),
                  [this] (float v)
                  {
                      for (size_t i = 0; i < modeButtons.size(); ++i)
                          modeButtons[i].setToggleState ((int) std::round (v) == (int) i, juce::dontSendNotification);
                  }),
      satAttach (*p.apvts.getParameter ("satmode"),
                 [this] (float v)
                 {
                     for (size_t i = 0; i < satButtons.size(); ++i)
                         satButtons[i].setToggleState ((int) std::round (v) == (int) i, juce::dontSendNotification);
                 })
{
    setLookAndFeel (&laf);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (tube);

    const char* names[] = { "TIME", "FEEDBACK", "MIX", "TONE", "MOD", "DRIVE" };
    const char* ids[]   = { "time", "feedback", "mix", "tone", "mod", "sat" };
    for (size_t i = 0; i < strips.size(); ++i)
    {
        auto& s = strips[i];
        s.name = names[i];
        s.slider.setSliderStyle (juce::Slider::LinearVertical);
        s.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
        addAndMakeVisible (s.slider);
        s.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids[i], s.slider);

        s.slider.setColour (juce::Slider::textBoxTextColourId, ink);
        s.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

        if (i == 0)
        {
            s.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };
            s.slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
        }
        else if (i == 3)
        {
            s.slider.textFromValueFunction = [] (double v)
            {
                return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
            };
            s.slider.valueFromTextFunction = [] (const juce::String& t)
            {
                return t.containsIgnoreCase ("k") ? t.getDoubleValue() * 1000.0 : t.getDoubleValue();
            };
        }
        else
        {
            s.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
            s.slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue() / 100.0; };
        }
        s.slider.updateText();
    }

    const char* modeNames[] = { "TAPE", "ANALOG", "FLANGER" };
    for (size_t i = 0; i < modeButtons.size(); ++i)
    {
        modeButtons[i].setButtonText (modeNames[i]);
        modeButtons[i].getProperties().set ("big", true);
        modeButtons[i].onClick = [this, i] { modeAttach.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (modeButtons[i]);
    }
    modeAttach.sendInitialUpdate();

    const char* satNames[] = { "DRY", "TAILS", "BOTH" };
    for (size_t i = 0; i < satButtons.size(); ++i)
    {
        satButtons[i].setButtonText (satNames[i]);
        satButtons[i].onClick = [this, i] { satAttach.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (satButtons[i]);
    }
    satAttach.sendInitialUpdate();

    addAndMakeVisible (reverseBtn);
    addAndMakeVisible (pingPong);
    reverseAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "reverse", reverseBtn);
    pingAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "pingpong", pingPong);

    setSize (820, 580);
}

DelayEditor::~DelayEditor()
{
    setLookAndFeel (nullptr);
}

void DelayEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    g.setOpacity (1.0f);
    g.drawImage (bodyTexture(), 0, 0, getWidth(), getHeight(), 0, 0, 1640, 1160);

    for (auto r : { specArea, slidersArea, tubeArea, modeArea })
        drawModule (g, r.toFloat());

    // joined key bars
    drawKeyHousing (g, modeKeys.toFloat());
    drawKeyHousing (g, modeSmall.toFloat());
    drawKeyHousing (g, satKeys.toFloat());

    for (size_t i = 0; i < 5; ++i)
    {
        const auto sb = strips[i].slider.getBounds();
        printed (g, strips[i].name, { sb.getCentreX() - 40, slidersArea.getY() + 16, 80, 14 }, 11.5f);
    }
    printed (g, "TUBE SATURATION", { tubeArea.getX(), tubeArea.getY() + 12, tubeArea.getWidth(), 14 });
    printed (g, "DRIVE", { strips[5].slider.getX(), tubeArea.getY() + 30, strips[5].slider.getWidth(), 14 }, 10.5f);
    printed (g, "MODE", { modeArea.getX(), modeArea.getY() + 11, modeArea.getWidth(), 14 });
    printed (g, "SATURATE ON", { tubeArea.getX(), tubeArea.getBottom() - 70, tubeArea.getWidth(), 12 }, 9.5f);

    // brand: big faint title from the left edge of the window, then the small logotype and racing stripes on top
    {
        const auto r = brandArea.toFloat();
        {
            const auto bigF = juce::Font (juce::FontOptions (120.0f, juce::Font::bold));
            juce::GlyphArrangement big;
            big.addLineOfText (bigF, "DEVIS DELAY", 0.0f, 0.0f);
            juce::Path bp;
            big.createPath (bp);
            const auto pb = bp.getBounds();
            const juce::Rectangle<float> dst (8.0f, r.getY() + 1.0f, b.getWidth() - 16.0f, r.getHeight() - 2.0f);
            bp.applyTransform (juce::AffineTransform::translation (-pb.getX(), -pb.getY())
                                   .scaled (dst.getWidth() / pb.getWidth(), dst.getHeight() / pb.getHeight())
                                   .translated (dst.getX(), dst.getY()));
            g.setColour (juce::Colour (0xff6f6552).withAlpha (0.16f));
            g.fillPath (bp, juce::AffineTransform::translation (1.2f, 1.2f));
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillPath (bp);
        }
        const auto f = juce::Font (juce::FontOptions (30.0f, juce::Font::bold | juce::Font::italic));
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, "Devis Delay", r.getX() + 4.0f, r.getCentreY() + 10.0f);
        juce::Path txt;
        ga.createPath (txt);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillPath (txt, juce::AffineTransform::translation (1.2f, 1.6f));
        g.setColour (ink);
        g.fillPath (txt);
        const float tx = txt.getBounds().getRight() + 16.0f;

        const juce::Colour stripe[] = { orange, juce::Colour (0xffe3b72a), juce::Colour (0xff3a3a37) };
        for (int k = 0; k < 3; ++k)
        {
            const float yy = r.getCentreY() - 8.0f + (float) k * 7.0f;
            g.setColour (stripe[k]);
            g.fillRoundedRectangle (tx + (float) k * 10.0f, yy, r.getRight() - tx - (float) k * 10.0f - 4.0f, 4.5f, 2.2f);
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawRect (b.reduced (1.0f), 1.5f);
    g.setColour (ink.withAlpha (0.45f));
    g.drawRect (b, 2.0f);
}

void DelayEditor::resized()
{
    auto r = getLocalBounds().reduced (22, 20);
    brandArea = r.removeFromBottom (44);
    r.removeFromBottom (8);

    auto left = r.removeFromLeft (514);
    r.removeFromLeft (12);
    auto right = r;

    specArea = left.removeFromTop (150);
    left.removeFromTop (12);
    slidersArea = left;

    tubeArea = right.removeFromTop (284);
    right.removeFromTop (12);
    modeArea = right;

    spectrum.setBounds (specArea.reduced (14));

    auto sl = slidersArea.reduced (8, 0).withTrimmedTop (36).withTrimmedBottom (12);
    const int w = sl.getWidth() / 5;
    for (size_t i = 0; i < 5; ++i)
        strips[i].slider.setBounds (sl.removeFromLeft (w));

    // tube module: lamp window + DRIVE slider + saturation stage keys
    tube.setBounds (tubeArea.getX() + 16, tubeArea.getY() + 34, 128, 172);
    strips[5].slider.setBounds (tubeArea.getX() + 156, tubeArea.getY() + 46, 78, 164);
    satKeys = juce::Rectangle<int> (tubeArea.getX() + 14, tubeArea.getBottom() - 56, 222, 42);
    for (size_t i = 0; i < satButtons.size(); ++i)
        satButtons[i].setBounds (satKeys.getX() + 3 + (int) i * 72, satKeys.getY() + 3, 72, 36);

    // mode module: a joined bar of three keys, and a second bar with two
    modeKeys = juce::Rectangle<int> (modeArea.getX() + 14, modeArea.getY() + 34, 222, 62);
    for (size_t i = 0; i < modeButtons.size(); ++i)
        modeButtons[i].setBounds (modeKeys.getX() + 3 + (int) i * 72, modeKeys.getY() + 3, 72, 56);
    modeSmall = juce::Rectangle<int> (modeArea.getX() + 14, modeArea.getBottom() - 56, 222, 42);
    reverseBtn.setBounds (modeSmall.getX() + 3, modeSmall.getY() + 3, 108, 36);
    pingPong.setBounds (modeSmall.getX() + 111, modeSmall.getY() + 3, 108, 36);

    // thin housing: hug the keys so only a slim dark rim shows around them
    modeKeys = modeButtons.front().getBounds().getUnion (modeButtons.back().getBounds()).expanded (1);
    satKeys = satButtons.front().getBounds().getUnion (satButtons.back().getBounds()).expanded (1);
    modeSmall = reverseBtn.getBounds().getUnion (pingPong.getBounds()).expanded (1);
}
