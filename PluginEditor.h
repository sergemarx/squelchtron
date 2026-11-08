#pragma once
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
class GrungeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
// Slider that reports right-clicks (used for MIDI learn menu)
class LearnSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
};

//==============================================================================
class SqueltronEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SqueltronEditor (SqueltronProcessor&);
    ~SqueltronEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void buildGrunge();

    SqueltronProcessor& proc;
    GrungeLookAndFeel laf;
    juce::Image grunge;

    std::array<LearnSlider, SqueltronProcessor::numParams> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, SqueltronProcessor::numParams> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqueltronEditor)
};
