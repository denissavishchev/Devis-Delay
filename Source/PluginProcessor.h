#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class DelayProcessor : public juce::AudioProcessor
{
public:
    DelayProcessor();
    ~DelayProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // spectrum analyser feed (audio thread -> editor)
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    float scopeBlock[fftSize] {};
    std::atomic<bool> scopeReady { false };
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> inputLevel { 0.0f }; // peak of the incoming signal, drives the glowing tube

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr float maxDelayMs = 2000.0f;

    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    double sr = 44100.0;

    juce::SmoothedValue<float> smTime, smFeedback, smMix, smTone, smMod, smSat;
    float dcX[2][2] {}, dcY[2][2] {};
    float revPhase = 0.0f;
    float lpState[2] { 0.0f, 0.0f };
    float lp2State[2] { 0.0f, 0.0f };

    // short buffer for the flanger that sits in the feedback path
    juce::AudioBuffer<float> flangerBuffer;
    int flangerPos = 0;

    float phaseA = 0.0f, phaseB = 0.0f;

    float fifo[fftSize] {};
    int fifoIndex = 0;
    void pushScopeSample (float s);

    float readInterpolated (int channel, float delaySamples) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayProcessor)
};
