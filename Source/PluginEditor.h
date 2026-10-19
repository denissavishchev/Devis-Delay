#pragma once
#include "PluginProcessor.h"

namespace theme
{
    const juce::Colour orange   { 0xffa9c83d };   // accent = the spectrum LCD green
    const juce::Colour orangeD  { 0xff6f9a12 };
    const juce::Colour white    { 0xfff1efe6 };
    const juce::Colour ink      { 0xff2a2926 };
    const juce::Colour keyTop   { 0xff4a4b49 };
    const juce::Colour keyBot   { 0xff262725 };
    const juce::Colour digits   { 0xffd2cbb6 };
    const juce::Colour lcdA     { 0xffb9d44f };
    const juce::Colour lcdB     { 0xff8fae2e };
    const juce::Colour lcdInk   { 0xff1b3a10 };
}

class IndustrialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IndustrialLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 26; } // > half the cap height: cap stays inside the track, a strip of track stays visible
};

class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView (DelayProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    DelayProcessor& proc;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::vector<float> fftData, levels;
};

class TubeView : public juce::Component, private juce::Timer
{
public:
    explicit TubeView (DelayProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    DelayProcessor& proc;
    float glow = 0.0f; // 0..1, filament brightness
};

class DelayEditor : public juce::AudioProcessorEditor
{
public:
    explicit DelayEditor (DelayProcessor&);
    ~DelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DelayProcessor& proc;
    IndustrialLookAndFeel laf;
    SpectrumView spectrum;
    TubeView tube;

    struct Strip
    {
        juce::String name;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    std::array<Strip, 6> strips;

    std::array<juce::TextButton, 3> modeButtons, satButtons;
    juce::ToggleButton reverseBtn { "REVERSE" }, pingPong { "PING-PONG" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingAttach, reverseAttach;
    juce::ParameterAttachment modeAttach, satAttach;

    juce::Rectangle<int> specArea, slidersArea, tubeArea, modeArea, brandArea, modeKeys, modeSmall, satKeys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayEditor)
};
