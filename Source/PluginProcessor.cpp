#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    const juce::ParameterID time     { "time", 1 };
    const juce::ParameterID feedback { "feedback", 1 };
    const juce::ParameterID mix      { "mix", 1 };
    const juce::ParameterID tone     { "tone", 1 };
    const juce::ParameterID pingPong { "pingpong", 1 };
    const juce::ParameterID mode     { "mode", 1 };
    const juce::ParameterID mod      { "mod", 1 };
    const juce::ParameterID sat      { "sat", 1 };
    const juce::ParameterID satMode  { "satmode", 1 };
    const juce::ParameterID reverse  { "reverse", 1 };
}

DelayProcessor::DelayProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DelayProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterFloat> (ids::time, "Time",
            NormalisableRange<float> (1.0f, maxDelayMs, 0.01f, 0.4f), 350.0f,
            AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (ids::feedback, "Feedback",
            NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.4f));
    l.add (std::make_unique<AudioParameterFloat> (ids::mix, "Mix",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    l.add (std::make_unique<AudioParameterFloat> (ids::tone, "Tone",
            NormalisableRange<float> (200.0f, 20000.0f, 1.0f, 0.3f), 8000.0f,
            AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (std::make_unique<AudioParameterBool> (ids::pingPong, "Ping-Pong", false));
    l.add (std::make_unique<AudioParameterChoice> (ids::mode, "Mode",
            StringArray { "Tape", "Analog", "Flanger" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ids::mod, "Mod",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    l.add (std::make_unique<AudioParameterFloat> (ids::sat, "Tube Sat",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    l.add (std::make_unique<AudioParameterChoice> (ids::satMode, "Sat Stage",
            StringArray { "Dry", "Tails", "Both" }, 1));
    l.add (std::make_unique<AudioParameterBool> (ids::reverse, "Reverse", false));
    return l;
}

void DelayProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const int size = (int) std::ceil (2.0 * maxDelayMs * 0.001 * sr) + 8; // 2x: reverse reads up to 2L back
    delayBuffer.setSize (2, size);
    delayBuffer.clear();
    writePos = 0;
    lpState[0] = lpState[1] = 0.0f;
    lp2State[0] = lp2State[1] = 0.0f;
    flangerBuffer.setSize (2, (int) std::ceil (0.02 * sr) + 4);
    flangerBuffer.clear();
    flangerPos = 0;
    phaseA = phaseB = 0.0f;
    fifoIndex = 0;
    currentSampleRate = sr;

    auto reset = [&] (juce::SmoothedValue<float>& s, const juce::ParameterID& id, double ramp)
    {
        s.reset (sr, ramp);
        s.setCurrentAndTargetValue (*apvts.getRawParameterValue (id.getParamID()));
    };
    reset (smTime, ids::time, 0.15); // slow ramp = tape-like pitch glide
    reset (smFeedback, ids::feedback, 0.03);
    reset (smMix, ids::mix, 0.03);
    reset (smTone, ids::tone, 0.03);
    reset (smMod, ids::mod, 0.05);
    reset (smSat, ids::sat, 0.03);
    std::fill (&dcX[0][0], &dcX[0][0] + 4, 0.0f);
    std::fill (&dcY[0][0], &dcY[0][0] + 4, 0.0f);
    revPhase = 0.0f;
}

bool DelayProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == l.getMainInputChannelSet();
}

// 4-point Hermite interpolation
float DelayProcessor::readInterpolated (int ch, float delaySamples) const
{
    const int size = delayBuffer.getNumSamples();
    const float* d = delayBuffer.getReadPointer (ch);

    float rp = (float) writePos - delaySamples;
    while (rp < 0.0f) rp += (float) size;

    const int i1 = (int) rp;
    const float f = rp - (float) i1;
    const int i0 = (i1 - 1 + size) % size;
    const int i2 = (i1 + 1) % size;
    const int i3 = (i1 + 2) % size;
    const float y0 = d[i0], y1 = d[i1 % size], y2 = d[i2], y3 = d[i3];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

void DelayProcessor::pushScopeSample (float s)
{
    fifo[fifoIndex++] = s;
    if (fifoIndex == fftSize)
    {
        fifoIndex = 0;
        if (! scopeReady.load())
        {
            std::copy (fifo, fifo + fftSize, scopeBlock);
            scopeReady = true;
        }
    }
}

void DelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    const int size = delayBuffer.getNumSamples();
    const int flSize = flangerBuffer.getNumSamples();
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    smTime.setTargetValue (*apvts.getRawParameterValue ("time"));
    smFeedback.setTargetValue (*apvts.getRawParameterValue ("feedback"));
    smMix.setTargetValue (*apvts.getRawParameterValue ("mix"));
    smTone.setTargetValue (*apvts.getRawParameterValue ("tone"));
    smMod.setTargetValue (*apvts.getRawParameterValue ("mod"));
    smSat.setTargetValue (*apvts.getRawParameterValue ("sat"));
    const int satMode = juce::jlimit (0, 2, (int) std::round (*apvts.getRawParameterValue ("satmode")));
    const bool reverse = *apvts.getRawParameterValue ("reverse") > 0.5f;
    float peak = 0.0f;

    float inPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, n));
    inputLevel = juce::jmax (inPeak, inputLevel.load() * 0.85f);
    const bool pingPong = *apvts.getRawParameterValue ("pingpong") > 0.5f && numCh == 2;
    const int mode = juce::jlimit (0, 2, (int) std::round (*apvts.getRawParameterValue ("mode")));

    // LFO rates (Hz): tape wow + flutter, analog (BBD) chorus-ish drift, flanger sweep
    const float rateA = mode == 0 ? 0.6f : mode == 1 ? 0.8f : 0.4f;
    const float rateB = 7.0f;
    const float incA = rateA / (float) sr, incB = rateB / (float) sr;
    const float fs = (float) sr;

    for (int i = 0; i < n; ++i)
    {
        const float timeMs = smTime.getNextValue();
        const float fb     = smFeedback.getNextValue();
        const float mix    = smMix.getNextValue();
        float cutoff       = smTone.getNextValue();
        const float mod    = std::sqrt (smMod.getNextValue()); // curve: clearly audible from the first notch

        if (mode == 1) cutoff = juce::jmin (cutoff, 5000.0f); // analog BBD is dark

        const float lfoA = std::sin (twoPi * phaseA);
        const float lfoB = std::sin (twoPi * phaseB);
        phaseA += incA; if (phaseA >= 1.0f) phaseA -= 1.0f;
        phaseB += incB; if (phaseB >= 1.0f) phaseB -= 1.0f;

        float modSamples = 0.0f;
        if (mode == 0)      modSamples = mod * fs * (0.0090f * lfoA + 0.0010f * lfoB);
        else if (mode == 1) modSamples = mod * fs * 0.0050f * lfoA;

        const float delaySamples = juce::jlimit (1.0f, maxDelayMs * 0.001f * fs,
                                                 timeMs * 0.001f * fs + modSamples);
        const float satAmt = smSat.getNextValue();

        // tube-style saturation: asymmetric tanh (even + odd harmonics), DC-blocked, crossfaded so 0 = bypass
        auto tube = [&] (float x, int stage, int ch)
        {
            const float g = 1.0f + 4.0f * satAmt, bias = 0.15f * satAmt;
            const float th = std::tanh (bias);
            // normalised to unity small-signal gain so repeats in the feedback loop still decay
            const float y = (std::tanh (g * x + bias) - th) / (g * (1.0f - th * th));
            const float out = y - dcX[stage][ch] + 0.995f * dcY[stage][ch];
            dcX[stage][ch] = y;
            dcY[stage][ch] = out;
            return x * (1.0f - satAmt) + out * satAmt;
        };

        // reverse: two Hann-windowed heads sweeping backwards over the last L samples
        float rw1 = 0.0f, rw2 = 0.0f, rd1 = 1.0f, rd2 = 1.0f;
        if (reverse)
        {
            const float p1 = revPhase, p2 = revPhase >= 0.5f ? revPhase - 0.5f : revPhase + 0.5f;
            const float s1 = std::sin (juce::MathConstants<float>::pi * p1), s2 = std::sin (juce::MathConstants<float>::pi * p2);
            rw1 = s1 * s1; rw2 = s2 * s2;
            rd1 = juce::jlimit (1.0f, (float) (size - 4), 2.0f * p1 * delaySamples);
            rd2 = juce::jlimit (1.0f, (float) (size - 4), 2.0f * p2 * delaySamples);
            revPhase += 1.0f / delaySamples;
            if (revPhase >= 1.0f) revPhase -= 1.0f;
        }
        const float a = 1.0f - std::exp (-twoPi * cutoff / fs);

        float in[2] {}, wet[2] {}, fbProc[2] {}, flangedWet[2] {};
        bool useFlangedWet = false;
        for (int ch = 0; ch < numCh; ++ch)
        {
            in[ch] = buffer.getSample (ch, i);
            if (satMode != 1) in[ch] = tube (in[ch], 0, ch);   // dry / both

            wet[ch] = reverse ? rw1 * readInterpolated (ch, rd1) + rw2 * readInterpolated (ch, rd2)
                              : readInterpolated (ch, delaySamples);
            if (satMode != 0) wet[ch] = tube (wet[ch], 1, ch); // tails / both

            // ---- feedback path character ----
            lpState[ch] += a * (wet[ch] - lpState[ch]);
            float x = lpState[ch];

            if (mode == 0)       // tape: tape-style saturation
                x = std::tanh (1.4f * x) / 1.4f;
            else if (mode == 1)  // analog: second low-pass pole + gentle clip
            {
                lp2State[ch] += a * (x - lp2State[ch]);
                x = std::tanh (1.1f * lp2State[ch]) / 1.1f;
            }
            else                 // flanger on the repeats: regenerative comb with deep notches
            {
                float* fl = flangerBuffer.getWritePointer (ch);
                const float lfo = std::sin (twoPi * (phaseA + 0.25f * (float) ch));
                const float dMs = 3.2f + 2.8f * (0.35f + 0.65f * mod) * lfo;
                float rp = (float) flangerPos - dMs * 0.001f * fs;
                while (rp < 0.0f) rp += (float) flSize;
                const int i0 = (int) rp;
                const float f = rp - (float) i0;
                const float d = fl[i0 % flSize] * (1.0f - f) + fl[(i0 + 1) % flSize] * f;
                fl[flangerPos] = x - 0.4f * d;                 // negative regeneration
                x = (x - 0.85f * d) * (1.0f / 2.42f);          // peak gain normalised to 1 (loop stays stable)
                flangedWet[ch] = x;
                useFlangedWet = true;
            }
            fbProc[ch] = x;
        }

        float mixed = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float toWrite;
            if (pingPong)
            {
                const float src = (ch == 0) ? 0.5f * (in[0] + in[1]) : 0.0f;
                toWrite = src + fbProc[1 - ch] * fb;
            }
            else
                toWrite = in[ch] + fbProc[ch] * fb;

            delayBuffer.setSample (ch, writePos, toWrite);
            const float wetOut = useFlangedWet ? flangedWet[ch] : wet[ch];   // flanger is heard on every repeat, the first one too
            const float out = in[ch] * (1.0f - mix) + wetOut * mix;
            buffer.setSample (ch, i, out);
            mixed += out;
            peak = juce::jmax (peak, std::abs (out));
        }

        pushScopeSample (mixed / (float) juce::jmax (1, numCh));

        if (++writePos >= size) writePos = 0;
        if (++flangerPos >= flSize) flangerPos = 0;
    }

    outputLevel = juce::jmax (peak, outputLevel.load() * 0.85f);

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

juce::AudioProcessorEditor* DelayProcessor::createEditor()
{
    return new DelayEditor (*this);
}

void DelayProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void DelayProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DelayProcessor(); }
