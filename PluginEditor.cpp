#include "PluginEditor.h"

namespace
{
    constexpr int W = 560, H = 420;
    const juce::Colour nuke     { 0xffc2d62b };   // nuclear waste: yellow with a lot of green in it
    const juce::Colour nukeHi   { 0xffd9ee3d };
    const juce::Colour ink      { 0xff1d1d0c };

    juce::Font boldFont (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
}

//==============================================================================
void GrungeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float a0, float a1, juce::Slider& s)
{
    using namespace juce;
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float r  = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto  c  = b.getCentre();
    const float rb = r * 0.74f;
    const float ang = a0 + pos * (a1 - a0);

    // tick marks
    for (int i = 0; i <= 10; ++i)
    {
        const float t = a0 + (float) i / 10.f * (a1 - a0);
        g.setColour (ink.withAlpha (i == 0 || i == 10 ? 0.95f : 0.7f));
        g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.88f, t), c.getPointOnCircumference (r, t)), i % 5 == 0 ? 2.4f : 1.4f);
    }

    // shadow
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (rb * 2.f, rb * 2.f).withCentre (c + Point<float> (3.f, 5.f)));

    // body
    Rectangle<float> body (rb * 2.f, rb * 2.f);
    body.setCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xff45453f), c.x - rb * 0.5f, c.y - rb * 0.6f,
                                       Colour (0xff0c0c0a), c.x + rb * 0.7f, c.y + rb * 0.8f, true));
    g.fillEllipse (body);
    g.setColour (Colour (0xff050504));
    g.drawEllipse (body, 2.f);

    // grip ridges
    g.setColour (Colours::white.withAlpha (0.10f));
    for (int i = 0; i < 28; ++i)
    {
        const float t = (float) i / 28.f * MathConstants<float>::twoPi;
        g.drawLine (Line<float> (c.getPointOnCircumference (rb * 0.90f, t), c.getPointOnCircumference (rb * 0.99f, t)), 1.f);
    }

    // scuffs and wear (stable per knob)
    Random rnd (s.getName().hashCode());
    for (int i = 0; i < 9; ++i)
    {
        Path p;
        const float rad = rb * (0.25f + rnd.nextFloat() * 0.65f);
        const float st  = rnd.nextFloat() * MathConstants<float>::twoPi;
        p.addCentredArc (c.x, c.y, rad, rad, 0.f, st, st + 0.3f + rnd.nextFloat() * 0.9f, true);
        g.setColour (Colours::white.withAlpha (0.06f + rnd.nextFloat() * 0.14f));
        g.strokePath (p, PathStrokeType (0.6f + rnd.nextFloat()));
    }

    // pointer
    g.setColour (Colour (0xfff2eec4).withAlpha (0.92f));
    g.drawLine (Line<float> (c.getPointOnCircumference (rb * 0.22f, ang), c.getPointOnCircumference (rb * 0.86f, ang)), 3.2f);
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (rb * 0.28f, rb * 0.28f).withCentre (c));
}

//==============================================================================
SqueltronEditor::SqueltronEditor (SqueltronProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setSize (W, H);
    setLookAndFeel (&laf);
    buildGrunge();

    for (int i = 0; i < SqueltronProcessor::numParams; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.setName (SqueltronProcessor::paramIds[i]);
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (1.25f * juce::MathConstants<float>::pi, 2.75f * juce::MathConstants<float>::pi, true);
        k.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (k);

        attachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, SqueltronProcessor::paramIds[i], k);

        k.onRightClick = [this, i]
        {
            juce::PopupMenu m;
            m.addItem (1, "MIDI Learn (then wiggle a knob on your controller)");
            m.addItem (2, "Clear MIDI mapping");
            m.addItem (3, "Cancel learn");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, i] (int r)
            {
                if (r == 1)      proc.learnIndex = i;
                else if (r == 2) proc.clearCC (i);
                else if (r == 3) proc.learnIndex = -1;
            });
        };
    }
    startTimerHz (8);
}

SqueltronEditor::~SqueltronEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SqueltronEditor::buildGrunge()
{
    using namespace juce;
    grunge = Image (Image::ARGB, W, H, true);
    Random rnd (1977);

    {
        Image::BitmapData bd (grunge, Image::BitmapData::writeOnly);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                bd.setPixelColour (x, y, nuke.withMultipliedBrightness (0.86f + rnd.nextFloat() * 0.20f));
    }

    Graphics g (grunge);

    // grime blotches
    for (int i = 0; i < 34; ++i)
    {
        const float cx = rnd.nextFloat() * W, cy = rnd.nextFloat() * H, r = 20.f + rnd.nextFloat() * 70.f;
        g.setGradientFill (ColourGradient (Colour (0xff4b4412).withAlpha (0.10f + rnd.nextFloat() * 0.12f), cx, cy,
                                           Colours::transparentBlack, cx + r, cy, true));
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
    }

    // rusty corners / bottom grime
    for (auto corner : { Point<float> (0, 0), Point<float> (W, 0), Point<float> (0, H), Point<float> (W, H) })
    {
        g.setGradientFill (ColourGradient (Colour (0xff6a3a10).withAlpha (0.38f), corner.x, corner.y,
                                           Colours::transparentBlack, corner.x + (corner.x > 0 ? -90.f : 90.f), corner.y + (corner.y > 0 ? -70.f : 70.f), true));
        g.fillRect (0, 0, W, H);
    }
    g.setGradientFill (ColourGradient (Colours::transparentBlack, 0, H - 90.f, Colour (0xff2c2808).withAlpha (0.35f), 0, (float) H, false));
    g.fillRect (0, H - 90, W, 90);

    // scratches, light and dark
    for (int i = 0; i < 140; ++i)
    {
        const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H;
        const float len = 6.f + rnd.nextFloat() * 60.f, an = rnd.nextFloat() * MathConstants<float>::twoPi;
        g.setColour (i % 2 ? Colour (0xfff0f8b0).withAlpha (0.10f + rnd.nextFloat() * 0.12f)
                            : Colour (0xff2a2a10).withAlpha (0.12f + rnd.nextFloat() * 0.18f));
        g.drawLine (x, y, x + std::cos (an) * len, y + std::sin (an) * len, 0.5f + rnd.nextFloat() * 0.6f);
    }

    // chipped paint specks
    for (int i = 0; i < 160; ++i)
    {
        g.setColour (Colour (0xff3a3a30).withAlpha (0.25f + rnd.nextFloat() * 0.4f));
        g.fillEllipse (rnd.nextFloat() * W, rnd.nextFloat() * H, 0.8f + rnd.nextFloat() * 2.2f, 0.8f + rnd.nextFloat() * 2.2f);
    }

    // vignette
    g.setGradientFill (ColourGradient (Colours::transparentBlack, W * 0.5f, H * 0.5f,
                                       Colours::black.withAlpha (0.55f), 0.f, 0.f, true));
    g.fillRect (0, 0, W, H);
}

void SqueltronEditor::resized()
{
    for (int i = 0; i < SqueltronProcessor::numParams; ++i)
    {
        const int col = i % 4, row = i / 4;
        const int cx = 70 + col * 140;
        const int cy = row == 0 ? 165 : 310;
        knobs[(size_t) i].setBounds (cx - 48, cy - 48, 96, 96);
    }
}

void SqueltronEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.drawImageAt (grunge, 0, 0);

    // bezel
    g.setColour (Colour (0xff111108));
    g.drawRect (0, 0, W, H, 6);

    // title plate
    const Rectangle<float> plate (28.f, 22.f, 504.f, 64.f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (plate.translated (3.f, 4.f), 6.f);
    g.setColour (Colour (0xff12120a));
    g.fillRoundedRectangle (plate, 6.f);
    g.setColour (Colour (0xff4a4a30));
    g.drawRoundedRectangle (plate.reduced (1.f), 6.f, 1.2f);

    g.setFont (boldFont (38.f));
    g.setColour (Colour (0xff6d7c10).withAlpha (0.7f));       // misregistered print
    g.drawText ("SQUELCHTRON", plate.translated (2.f, -3.f).withHeight (44.f).toNearestInt(), Justification::centred);
    g.setColour (nukeHi);
    g.drawText ("SQUELCHTRON", plate.translated (0.f, -5.f).withHeight (44.f).toNearestInt(), Justification::centred);

    g.setFont (boldFont (11.f));
    g.setColour (nuke.withAlpha (0.75f));
    g.drawText ("GUITAR  >  SQUAWK SYNTH  >  FUZZ    //    MK II", plate.withTop (plate.getBottom() - 20.f).toNearestInt(), Justification::centred);

    // labels + CC tags
    const bool blink = (Time::getMillis() / 280) % 2 == 0;
    for (int i = 0; i < SqueltronProcessor::numParams; ++i)
    {
        const auto kb = knobs[(size_t) i].getBounds();
        g.setFont (boldFont (16.f));
        g.setColour (ink);
        g.drawText (String (SqueltronProcessor::paramNames[i]).toUpperCase(),
                    kb.getX() - 10, kb.getBottom() + 0, kb.getWidth() + 20, 18, Justification::centred);

        String tag;
        const bool learning = proc.learnIndex.load() == i;
        const int cc = proc.ccFor[(size_t) i].load();
        if (learning) tag = blink ? "LEARNING..." : "";
        else          tag = cc >= 0 ? "CC " + String (cc) : "-";

        g.setFont (boldFont (10.f));
        g.setColour (learning ? Colour (0xff8a1a0a) : ink.withAlpha (0.65f));
        g.drawText (tag, kb.getX() - 10, kb.getBottom() + 17, kb.getWidth() + 20, 14, Justification::centred);
    }

    g.setFont (boldFont (10.f));
    g.setColour (ink.withAlpha (0.7f));
    g.drawText ("RIGHT-CLICK A KNOB TO MIDI LEARN  //  DOUBLE-CLICK TO RESET", 0, H - 22, W, 14, Justification::centred);

    // screws
    Random rnd (42);
    for (auto pt : { Point<float> (16, 16), Point<float> (W - 16.f, 16), Point<float> (16, H - 16.f), Point<float> (W - 16.f, H - 16.f) })
    {
        g.setGradientFill (ColourGradient (Colour (0xff8b8b80), pt.x - 3, pt.y - 3, Colour (0xff25251f), pt.x + 5, pt.y + 5, true));
        g.fillEllipse (pt.x - 6, pt.y - 6, 12, 12);
        const float an = rnd.nextFloat() * MathConstants<float>::pi;
        g.setColour (Colour (0xff101010));
        g.drawLine (pt.x - std::cos (an) * 5, pt.y - std::sin (an) * 5, pt.x + std::cos (an) * 5, pt.y + std::sin (an) * 5, 1.6f);
    }
}
