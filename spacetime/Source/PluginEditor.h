#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpaceTimeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class SpaceTimeAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpaceTimeAudioProcessorEditor (SpaceTimeAudioProcessor&);
    ~SpaceTimeAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    SpaceTimeAudioProcessor& audioProcessor;
    SpaceTimeLookAndFeel laf;

    juce::Slider bpm, preDelay, decay, reverbTime, mix, width, damping;
    juce::Label bpmLabel, preLabel, decayLabel, timeLabel, mixLabel, widthLabel, dampLabel;
    juce::ToggleButton hostSync { "HOST BPM" };
    juce::ToggleButton autoTiming { "AUTO TIME" };
    juce::TextButton tapButton { "TAP" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> bpmAtt, preAtt, timeAtt, mixAtt, widthAtt, dampAtt;
    std::unique_ptr<ButtonAttachment> hostAtt, autoAtt;

    juce::Rectangle<int> tableArea;
    static constexpr int rowH = 26;
    spacetime::Times lastTimes { 15.0f, 985.0f, 1000.0f };
    double lastBpm = 120.0, lastBars = 4.0;
    int lastSize = 3;
    bool lastHost = true, lastAuto = true;

    void setupKnob (juce::Slider&, juce::Label&, const juce::String& text);
    void setParam (const juce::String& id, float realValue);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceTimeAudioProcessorEditor)
};
