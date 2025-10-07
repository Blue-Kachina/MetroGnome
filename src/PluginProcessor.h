#pragma once

#include <JuceHeader.h>
#include <array>
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

    bool acceptsMidi() const override { return false; }
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

    // Sequencer last gate (for Phase 4 triggering), -1 means none this block
    int lastGateSample = -1;
    int lastGateStepIndex = -1;
    int lastGateBarIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetroGnomeAudioProcessor)
};
