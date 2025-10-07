#pragma once

#include <JuceHeader.h>
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

private:
    // Timing engine and cached host info (preallocated, no dynamic work in processBlock)
    metrog::TimingEngine timing;
    metrog::HostTransportInfo hostInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetroGnomeAudioProcessor)
};
