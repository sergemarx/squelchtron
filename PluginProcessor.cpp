#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

const char* const SqueltronProcessor::paramIds[numParams]   = { "fuzz", "squelch", "cutoff", "env", "synth", "squawk", "wobble", "level" };
const char* const SqueltronProcessor::paramNames[numParams] = { "Fuzz", "Squelch", "Cutoff", "Env", "Synth", "Squawk", "Wobble", "Level" };

juce::AudioProcessorValueTreeState::ParameterLayout SqueltronProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const float defs[numParams] = { 0.45f, 0.55f, 0.50f, 0.50f, 0.50f, 0.25f, 0.0f, 0.60f };

    for (int i = 0; i < numParams; ++i)
    {
        const bool bipolar = (i == 3);   // Env amount can sweep the filter down or up
        NormalisableRange<float> r (bipolar ? -1.f : 0.f, 1.f, 0.001f);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { paramIds[i], 1 }, paramNames[i], r, defs[i]));
    }
    return { p.begin(), p.end() };
}

SqueltronProcessor::SqueltronProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < numParams; ++i)
    {
        raw[(size_t) i]  = apvts.getRawParameterValue (paramIds[i]);
        ptrs[(size_t) i] = apvts.getParameter (paramIds[i]);
        ccFor[(size_t) i] = 20 + i;      // default CC 20..27, remap with MIDI learn
    }
}

bool SqueltronProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo())
           && o == l.getMainInputChannelSet();
}

void SqueltronProcessor::prepareToPlay (double sampleRate, int)
{
    sr = (float) sampleRate;
    env = gate = lp1 = lp2 = 0.f;
    pos = false; sinceCross = 0;
    freq = freqTarget = 110.f;
    phase = subPhase = lfoPhase = 0.f;
    ic1 = ic2 = dcX = dcY = 0.f;
    firstBlock = true;
}

void SqueltronProcessor::handleMidi (juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isController()) continue;

        const int cc = msg.getControllerNumber();
        const float v = (float) msg.getControllerValue() / 127.f;

        const int li = learnIndex.load();
        if (li >= 0)
        {
            for (auto& c : ccFor) if (c.load() == cc) c = -1;   // one CC, one knob
            ccFor[(size_t) li] = cc;
            learnIndex = -1;
        }

        for (int k = 0; k < numParams; ++k)
            if (ccFor[(size_t) k].load() == cc)
                ptrs[(size_t) k]->setValueNotifyingHost (v);
    }
}

static inline float polyBlep (float t, float dt)
{
    if (t < dt)         { t /= dt;           return t + t - t * t - 1.f; }
    if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

void SqueltronProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    handleMidi (midi);

    const int nCh = buf.getNumChannels();
    const int n   = buf.getNumSamples();
    if (nCh == 0 || n == 0) return;

    std::array<float, numParams> tgt;
    for (size_t k = 0; k < (size_t) numParams; ++k) tgt[k] = raw[k]->load();
    if (firstBlock) { sm = tgt; firstBlock = false; }

    const float smC     = 1.f - std::exp (-1.f / (0.010f * sr));
    const float envAtk  = 1.f - std::exp (-1.f / (0.001f * sr));
    const float envRel  = 1.f - std::exp (-1.f / (0.080f * sr));
    const float lpA     = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 900.f / sr);
    const float maxFc   = juce::jmin (18000.f, 0.4f * sr);
    const int   minPer  = (int) (sr / 1400.f);
    const int   maxPer  = (int) (sr / 55.f);

    for (int i = 0; i < n; ++i)
    {
        for (size_t k = 0; k < (size_t) numParams; ++k) sm[k] += smC * (tgt[k] - sm[k]);
        const float fuzz = sm[0], squelch = sm[1], cutoff = sm[2], envAmt = sm[3];
        const float synthAmt = sm[4], squawk = sm[5], wobble = sm[6], level = sm[7];

        // ---- mono in
        float in = 0.f;
        for (int ch = 0; ch < nCh; ++ch) in += buf.getReadPointer (ch)[i];
        in /= (float) nCh;

        // ---- envelope follower
        const float a = std::abs (in);
        env += (a > env ? envAtk : envRel) * (a - env);

        // ---- zero-crossing pitch tracker (mono guitar)
        lp1 += lpA * (in - lp1);
        lp2 += lpA * (lp1 - lp2);
        const float thr = juce::jmax (0.004f, env * 0.15f);
        sinceCross = juce::jmin (sinceCross + 1, 1000000);
        if (! pos && lp2 > thr)
        {
            pos = true;
            if (sinceCross > minPer && sinceCross < maxPer) freqTarget = sr / (float) sinceCross;
            sinceCross = 0;
        }
        else if (pos && lp2 < -thr) pos = false;
        freq += 0.01f * (freqTarget - freq);

        const float gT = env > 0.006f ? 1.f : 0.f;
        gate += (gT > gate ? 0.01f : 0.0008f) * (gT - gate);

        // ---- oscillators: saw + square + sub
        const float dt = juce::jlimit (0.0001f, 0.45f, freq / sr);
        phase += dt;       if (phase >= 1.f)    phase -= 1.f;
        subPhase += dt * 0.5f; if (subPhase >= 1.f) subPhase -= 1.f;

        const float saw = 2.f * phase - 1.f - polyBlep (phase, dt);
        float sq = phase < 0.5f ? 1.f : -1.f;
        sq += polyBlep (phase, dt);
        sq -= polyBlep (std::fmod (phase + 0.5f, 1.f), dt);
        const float sub = subPhase < 0.5f ? 1.f : -1.f;

        const float osc = 0.55f * saw + 0.30f * sq + 0.30f * sub;
        const float synthSig = osc * gate * juce::jmin (1.f, env * 10.f);
        const float ring = in * saw;                       // the squawk

        const float mixed = in * (1.f - synthAmt) + synthSig * synthAmt * 1.5f + ring * squawk * 3.f;

        // ---- wobble LFO
        lfoPhase += (0.5f + wobble * 14.f) / sr;
        if (lfoPhase >= 1.f) lfoPhase -= 1.f;
        const float lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);

        // ---- squelch filter (TPT state variable, lowpass)
        const float envN = juce::jmin (1.f, env * 8.f);
        const float octMod = envAmt * envN * 4.f + lfo * wobble * 2.5f;
        float fc = 70.f * std::pow (2.f, cutoff * 8.f) * std::pow (2.f, octMod);
        fc = juce::jlimit (40.f, maxFc, fc);
        const float Q = 0.55f + squelch * squelch * 22.f;
        const float g = std::tan (juce::MathConstants<float>::pi * fc / sr);
        const float k = 1.f / Q;
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float v0 = mixed * 0.7f;
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        float filtered = v2 / (1.f + 0.04f * Q);

        // ---- fuzz: asymmetric clip + octave-ish fold
        const float pre  = 1.f + fuzz * fuzz * 60.f;
        const float bias = fuzz * 0.35f;
        float y = std::tanh (pre * filtered + bias) - std::tanh (bias);
        y += fuzz * 0.25f * (std::abs (y) - 0.3f);
        y *= 0.85f / (1.f + fuzz * 0.5f);

        // ---- DC block + level
        const float dcOut = y - dcX + 0.995f * dcY;
        dcX = y; dcY = dcOut;
        float out = std::tanh (dcOut * level * level * 2.5f);

        if (! std::isfinite (out)) { out = 0.f; ic1 = ic2 = dcX = dcY = 0.f; }

        for (int ch = 0; ch < nCh; ++ch) buf.getWritePointer (ch)[i] = out;
    }
}

juce::AudioProcessorEditor* SqueltronProcessor::createEditor() { return new SqueltronEditor (*this); }

void SqueltronProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    for (int i = 0; i < numParams; ++i)
        st.setProperty (juce::String ("cc_") + paramIds[i], ccFor[(size_t) i].load(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void SqueltronProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (st);
            for (int i = 0; i < numParams; ++i)
                ccFor[(size_t) i] = (int) st.getProperty (juce::String ("cc_") + paramIds[i], ccFor[(size_t) i].load());
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SqueltronProcessor(); }
