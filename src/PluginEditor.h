#pragma once

#include <JuceHeader.h>

class MetroGnomeAudioProcessor;

class MetroGnomeAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MetroGnomeAudioProcessorEditor (MetroGnomeAudioProcessor&);
    ~MetroGnomeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Timer
    void timerCallback() override;

    // Helpers
    void loadBackgroundImages();

    MetroGnomeAudioProcessor& processor;

    // UI components
    juce::Slider stepsSlider;      // number of steps
    juce::Slider timeSigSlider;    // time signature numerator (timing)
    juce::Slider volumeSlider;     // output volume
    juce::TextButton enableAllBtn { "Enable All" };
    juce::TextButton disableAllBtn { "Disable All" };
    juce::ToggleButton danceToggle { "Dance" };
    juce::OwnedArray<juce::ToggleButton> stepToggles; // 16 toggles

    // Labels for rotary controls
    juce::Label stepsLabel;
    juce::Label beatsPerBarLabel;
    juce::Label volumeLabel;


    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeSigAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAllAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> disableAllAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> danceAttachment;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> stepAttachments;

    // Images
    juce::Image bgA;
    juce::Image bgB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetroGnomeAudioProcessorEditor)
};
