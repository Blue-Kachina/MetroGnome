#pragma once

#include <JuceHeader.h>

class MetroGnomeAudioProcessor;

class MetroGnomeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MetroGnomeAudioProcessorEditor (MetroGnomeAudioProcessor&);
    ~MetroGnomeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MetroGnomeAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetroGnomeAudioProcessorEditor)
};
