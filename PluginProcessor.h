#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

//==============================================================================
// SQUELCHTRON - guitar -> pitch-tracked squawk synth -> resonant filter -> fuzz
//==============================================================================
class SqueltronProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numParams = 8;
    static const char* const paramIds[numParams];
    static const char* const paramNames[numParams];

    SqueltronProcessor();
    ~SqueltronProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Squelchtron"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- public so the editor can use them ---
    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<int>, numParams> ccFor;   // MIDI CC per knob, -1 = none
    std::atomic<int> learnIndex { -1 };              // knob waiting for a CC, -1 = none
    void clearCC (int i) { ccFor[(size_t) i] = -1; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (juce::MidiBuffer& midi);

    std::array<std::atomic<float>*, numParams> raw {};
    std::array<juce::RangedAudioParameter*, numParams> ptrs {};
    std::array<float, numParams> sm {};      // smoothed params
    bool firstBlock = true;

    // DSP state
    float sr = 44100.f;
    float env = 0.f, gate = 0.f;
    float lp1 = 0.f, lp2 = 0.f;
    bool  pos = false;
    int   sinceCross = 0;
    float freq = 110.f, freqTarget = 110.f;
    float phase = 0.f, subPhase = 0.f, lfoPhase = 0.f;
    float ic1 = 0.f, ic2 = 0.f;              // filter state
    float dcX = 0.f, dcY = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqueltronProcessor)
};
