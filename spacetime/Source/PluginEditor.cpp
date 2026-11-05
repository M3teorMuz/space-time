#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg      { 0xff111018 };
    const juce::Colour kPanel   { 0xff1b1626 };
    const juce::Colour kAccent  { 0xffc69cff };
    const juce::Colour kAccent2 { 0xff7fe3ff };
    const juce::Colour kText    { 0xfff4f0ff };
    const juce::Colour kDim     { 0xffa69abf };

    juce::String fmtMs (float v)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, 1) + " ms";
    }
}

//==============================================================================
void SpaceTimeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                             float startAngle, float endAngle, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = d * 0.5f;
    const float alpha = s.isEnabled() ? 1.0f : 0.45f;
    const auto fill = s.findColour (juce::Slider::rotarySliderFillColourId);

    g.setColour (kPanel);
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);

    const float arcR = r - 3.0f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff40364e).withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = startAngle + pos * (endAngle - startAngle);
    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (fill.withAlpha (alpha));
    g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = r * 0.30f, outer = r * 0.68f;
    juce::Line<float> pointer (c.x + inner * std::sin (angle), c.y - inner * std::cos (angle),
                               c.x + outer * std::sin (angle), c.y - outer * std::cos (angle));
    g.setColour (kText.withAlpha (alpha));
    g.drawLine (pointer, 2.5f);
}

void SpaceTimeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? kAccent : juce::Colour (0xff2a2238));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (kAccent.withAlpha (highlighted ? 1.0f : 0.7f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);
    g.setColour (on ? juce::Colour (0xff17121f) : juce::Colour (0xffe8d9ff));
    g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
SpaceTimeAudioProcessorEditor::SpaceTimeAudioProcessorEditor (SpaceTimeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&laf);
    setSize (760, 570);

    setupKnob (preDelay, preLabel, "PRE-DELAY");
    setupKnob (decay, decayLabel, "DECAY");
    setupKnob (reverbTime, timeLabel, "REVERB TIME");
    setupKnob (mix, mixLabel, "MIX");
    setupKnob (width, widthLabel, "WIDTH");
    setupKnob (damping, dampLabel, "DAMPING");

    // DECAY - только показ (вычисляется = Reverb Time - Pre-Delay)
    {
        juce::NormalisableRange<double> r (10.0, 30000.0);
        r.setSkewForCentre (1500.0);
        decay.setNormalisableRange (r);
        decay.textFromValueFunction = [] (double v) { return fmtMs ((float) v); };
        decay.setTextBoxIsEditable (false);
        decay.setInterceptsMouseClicks (false, false);
        decay.setColour (juce::Slider::rotarySliderFillColourId, kAccent2);
        decay.setValue (985.0, juce::dontSendNotification);
    }

    // BPM
    bpm.setSliderStyle (juce::Slider::IncDecButtons);
    bpm.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
    bpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 80, 30);
    bpm.setColour (juce::Slider::textBoxTextColourId, kText);
    bpm.setColour (juce::Slider::textBoxOutlineColourId, kAccent.withAlpha (0.5f));
    bpm.setTextValueSuffix (" BPM");
    addAndMakeVisible (bpm);

    addAndMakeVisible (hostSync);
    addAndMakeVisible (autoTiming);
    addAndMakeVisible (tapButton);
    tapButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2238));
    tapButton.setColour (juce::TextButton::textColourOffId, kText);
    tapButton.onClick = [this] { audioProcessor.tapTempo(); };

    auto& ap = audioProcessor.apvts;
    bpmAtt   = std::make_unique<SliderAttachment> (ap, "bpm", bpm);
    preAtt   = std::make_unique<SliderAttachment> (ap, "preDelay", preDelay);
    timeAtt  = std::make_unique<SliderAttachment> (ap, "reverbTime", reverbTime);
    mixAtt   = std::make_unique<SliderAttachment> (ap, "mix", mix);
    widthAtt = std::make_unique<SliderAttachment> (ap, "width", width);
    dampAtt  = std::make_unique<SliderAttachment> (ap, "damping", damping);
    hostAtt  = std::make_unique<ButtonAttachment> (ap, "hostSync", hostSync);
    autoAtt  = std::make_unique<ButtonAttachment> (ap, "autoTiming", autoTiming);

    // При выключении HOST BPM / AUTO TIME ручные значения стартуют с того, что сейчас звучит.
    hostSync.onClick = [this] { if (! hostSync.getToggleState()) setParam ("bpm", (float) lastBpm); };
    autoTiming.onClick = [this]
    {
        if (! autoTiming.getToggleState())
        {
            setParam ("preDelay", lastTimes.preMs);
            setParam ("reverbTime", lastTimes.totalMs);
        }
    };

    startTimerHz (15);
    timerCallback();
}

SpaceTimeAudioProcessorEditor::~SpaceTimeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SpaceTimeAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 22);
    s.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    s.setColour (juce::Slider::textBoxTextColourId, kText);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setDoubleClickReturnValue (false, 0.0);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    l.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (l);
}

void SpaceTimeAudioProcessorEditor::setParam (const juce::String& id, float realValue)
{
    if (auto* prm = audioProcessor.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
        prm->endChangeGesture();
    }
}

void SpaceTimeAudioProcessorEditor::timerCallback()
{
    auto& ap = audioProcessor.apvts;
    lastHost = ap.getRawParameterValue ("hostSync")->load() > 0.5f;
    lastAuto = ap.getRawParameterValue ("autoTiming")->load() > 0.5f;
    lastSize = juce::jlimit (0, 5, juce::roundToInt (ap.getRawParameterValue ("size")->load()));
    lastBpm  = audioProcessor.getEffectiveBpm();
    lastBars = audioProcessor.getBeatsPerBar();

    lastTimes = spacetime::resolve (lastAuto, lastBpm, lastBars, lastSize,
                                    ap.getRawParameterValue ("preDelay")->load(),
                                    ap.getRawParameterValue ("reverbTime")->load());

    if (lastHost)
        bpm.setValue (lastBpm, juce::dontSendNotification);
    bpm.setEnabled (! lastHost);

    preDelay.setEnabled (! lastAuto);
    reverbTime.setEnabled (! lastAuto);
    if (lastAuto)
    {
        preDelay.setValue (lastTimes.preMs, juce::dontSendNotification);
        reverbTime.setValue (lastTimes.totalMs, juce::dontSendNotification);
    }
    decay.setValue (lastTimes.decayMs, juce::dontSendNotification);

    repaint (tableArea.expanded (0, 30));
}

void SpaceTimeAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto rows = tableArea.withTrimmedTop (rowH);
    if (rows.contains (e.getPosition()))
    {
        const int row = juce::jlimit (0, 5, (e.y - rows.getY()) / rowH);
        setParam ("size", (float) row);
    }
}

void SpaceTimeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    juce::ColourGradient grad (juce::Colour (0xff29203b), 0.0f, 0.0f, kBg, (float) getWidth(), (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (8.0f), 18.0f);
    g.setColour (kAccent.withAlpha (0.8f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (8.0f), 18.0f, 1.2f);
    g.setColour (kAccent.withAlpha (0.10f));
    for (int x = 20; x < getWidth(); x += 24)
        for (int y = 20; y < getHeight(); y += 24)
            g.fillEllipse ((float) x, (float) y, 1.5f, 1.5f);

    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("SPACE/TIME", 30, 20, 300, 36, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("TEMPO SYNCHRONIZED REVERB  //  V1.0", 32, 54, 400, 16, juce::Justification::centredLeft);

    // --- таблица размеров (как на калькуляторе 2Rock) ---
    auto t = tableArea;
    g.setColour (kPanel.withAlpha (0.85f));
    g.fillRoundedRectangle (t.toFloat(), 10.0f);
    g.setColour (kAccent.withAlpha (0.35f));
    g.drawRoundedRectangle (t.toFloat(), 10.0f, 1.0f);

    const int colX[5] = { t.getX() + 16, t.getX() + 215, t.getX() + 330, t.getX() + 450, t.getX() + 575 };
    auto header = t.removeFromTop (rowH);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.setColour (kDim);
    const char* heads[5] = { "REVERB SIZE", "NOTE", "PRE-DELAY", "DECAY", "REVERB TIME" };
    for (int i = 0; i < 5; ++i)
        g.drawText (heads[i], colX[i], header.getY(), 130, rowH, juce::Justification::centredLeft);

    for (int r = 0; r < 6; ++r)
    {
        auto row = t.removeFromTop (rowH);
        const bool sel = (r == lastSize);
        if (sel)
        {
            g.setColour (kAccent.withAlpha (0.20f));
            g.fillRoundedRectangle (row.toFloat().reduced (4.0f, 1.0f), 6.0f);
            g.setColour (kAccent);
            g.fillRoundedRectangle ((float) row.getX() + 5.0f, (float) row.getY() + 5.0f, 3.0f, (float) rowH - 10.0f, 1.5f);
        }
        const auto& info = spacetime::sizeTable()[r];
        const auto tm = spacetime::calculate (lastBpm, lastBars, r);
        g.setFont (juce::Font (juce::FontOptions (13.0f, sel ? juce::Font::bold : juce::Font::plain)));
        g.setColour (sel ? kText : kDim);
        g.drawText (info.shortName, colX[0], row.getY(), 190, rowH, juce::Justification::centredLeft);
        g.drawText (info.note, colX[1], row.getY(), 100, rowH, juce::Justification::centredLeft);
        g.drawText (juce::String (tm.preMs, 2) + " ms", colX[2], row.getY(), 120, rowH, juce::Justification::centredLeft);
        g.drawText (juce::String (tm.decayMs, 2) + " ms", colX[3], row.getY(), 120, rowH, juce::Justification::centredLeft);
        g.drawText (juce::String (tm.totalMs, 2) + " ms", colX[4], row.getY(), 140, rowH, juce::Justification::centredLeft);
    }

    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (juce::String (lastHost ? "TEMPO FROM HOST: " : "MANUAL TEMPO: ") + juce::String (lastBpm, 1)
                    + " BPM   |   BAR = " + juce::String (lastBars, 2) + " BEATS",
                tableArea.getX(), tableArea.getBottom() + 4, tableArea.getWidth(), 16, juce::Justification::centredLeft);
}

void SpaceTimeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (28);
    area.removeFromTop (50); // заголовок

    auto top = area.removeFromTop (40);
    bpm.setBounds (top.removeFromLeft (200).reduced (0, 4));
    top.removeFromLeft (14);
    hostSync.setBounds (top.removeFromLeft (110).reduced (0, 5));
    top.removeFromLeft (8);
    autoTiming.setBounds (top.removeFromLeft (110).reduced (0, 5));
    top.removeFromLeft (8);
    tapButton.setBounds (top.removeFromLeft (70).reduced (0, 5));

    area.removeFromTop (12);
    tableArea = area.removeFromTop (rowH * 7);
    area.removeFromTop (28);

    auto knobs = area;
    const int cw = knobs.getWidth() / 6;
    juce::Slider* sliders[6] = { &preDelay, &decay, &reverbTime, &mix, &width, &damping };
    juce::Label* labels[6] = { &preLabel, &decayLabel, &timeLabel, &mixLabel, &widthLabel, &dampLabel };
    for (int i = 0; i < 6; ++i)
    {
        auto col = knobs.removeFromLeft (cw);
        labels[i]->setBounds (col.removeFromTop (20));
        sliders[i]->setBounds (col);
    }
}
