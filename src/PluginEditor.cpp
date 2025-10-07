#include "PluginEditor.h"
#include "PluginProcessor.h"

class MetroGnomeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MetroGnomeLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colours::black);
    }
};

// TU-scoped LookAndFeel instance; declared before use
static MetroGnomeLookAndFeel laf;

//==============================================================================
MetroGnomeAudioProcessorEditor::MetroGnomeAudioProcessorEditor (MetroGnomeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&laf);
    setSize (420, 220);
}

MetroGnomeAudioProcessorEditor::~MetroGnomeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MetroGnomeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawFittedText ("Metro Gnome", getLocalBounds().reduced (10), juce::Justification::centredTop, 1);

    g.setFont (juce::Font (13.0f));
    g.setColour (juce::Colours::silver);
    g.drawFittedText ("Phase 1: Skeleton (silent instrument)", getLocalBounds().withTop (40).reduced (10), juce::Justification::centredTop, 1);
}

void MetroGnomeAudioProcessorEditor::resized()
{
}
