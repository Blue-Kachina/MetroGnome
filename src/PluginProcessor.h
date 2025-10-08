#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "Timing.h"

class MetroGnomeAudioProcessor : public juce::AudioProcessor
{
public:
    MetroGnomeAudioProcessor();
    ~MetroGnomeAudioProcessor() override;

    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock; // for double precision fallback

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameter access (for future UI)
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    // UI helpers
    int getCurrentStepIndex() const noexcept { return currentStepIndex.load(); }
    int getDanceParity() const noexcept { return danceParity.load(); }

    // MIDI learn API (UI thread)
    void armMidiLearn (const juce::String& paramID);
    void cancelMidiLearn();
    bool hasPendingMidiLearn() const noexcept { return pendingLearnCC.load() >= 0; }
    // Applies pending learned CC to current target; returns true if applied
    bool commitPendingMidiLearn();
    // Clear stored mapping for a parameter
    void clearMidiMapping (const juce::String& paramID);
    // Query mapped CC for UI (-1 if none)
    int getMappedCC (const juce::String& paramID) const;

private:
    // Parameter layout
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Timing engine and cached host info (preallocated, no dynamic work in processBlock)
    metrog::TimingEngine timing;
    metrog::HostTransportInfo hostInfo;

    // Parameters
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* stepCountParam = nullptr;
    std::array<std::atomic<float>*, 16> stepEnabledParams{};
    std::atomic<float>* enableAllParam = nullptr;
    std::atomic<float>* disableAllParam = nullptr;
    std::atomic<float>* volumeParam = nullptr; // 0..1 linear volume
    std::atomic<float>* danceModeParam = nullptr; // UI-only toggle
    std::atomic<float>* timeSigNumParam = nullptr; // 1..16 independent timing numerator

    // MIDI learn state (real-time safe communication)
    std::atomic<bool> midiLearnArmed { false };
    juce::String midiLearnTargetId; // set on message thread
    std::atomic<int> pendingLearnCC { -1 }; // set in audio thread

    // Fast CC->parameter map for audio thread (size 128)
    std::array<std::atomic<juce::RangedAudioParameter*>, 128> ccToParam{};

    // Helpers (message thread)
    void rebuildMidiMapFromState();

    // Sequencer last gate (for Phase 4 triggering), -1 means none this block
    int lastGateSample = -1;
    int lastGateStepIndex = -1;
    int lastGateBarIndex = -1;

    // UI timing info for dance mode (updated on every subdivision crossing)
    std::atomic<int> currentStepIndex { -1 };
    std::atomic<int> danceParity { 0 }; // flips on every subdivision crossing for smooth dance alternation

    // Global subdivision counter to ensure full sequence progression regardless of time signature
    std::atomic<int> globalSubdivisionCounter { 0 };

    // Simple click synthesizer state (RT-safe, no allocations)
    bool clickActive = false;
    int clickSampleIndex = 0;
    int clickMaxSamples = 0;   // computed from sample rate (e.g., 10 ms)
    double clickEnv = 0.0;     // exponential decay envelope
    double clickDecay = 0.999; // per-sample multiplier
    double sinePhase = 0.0;
    double sinePhaseInc = 0.0; // 3 kHz default

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetroGnomeAudioProcessor)
};
