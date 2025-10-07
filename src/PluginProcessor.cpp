#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MetroGnomeAudioProcessor::MetroGnomeAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if JucePlugin_IsSynth
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#else
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
#endif
    )
{
}

MetroGnomeAudioProcessor::~MetroGnomeAudioProcessor() = default;

//==============================================================================
void MetroGnomeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // No dynamic allocations; ensure deterministic state.
    timing.prepare(sampleRate, samplesPerBlock);
    hostInfo.sampleRate = sampleRate;
    timing.setSubdivisionsPerBar(4); // default quarter notes in 4/4
}

void MetroGnomeAudioProcessor::releaseResources()
{
}

bool MetroGnomeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsSynth
    // For instrument, require no inputs and allow mono/stereo output.
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
#else
    // For non-synth, require symmetric layout.
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet()
        && (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
            || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
#endif
}

void MetroGnomeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Read host transport info deterministically without allocations
    if (auto* playHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo info;
        if (playHead->getCurrentPosition (info))
        {
            // Always update play/stop state
            hostInfo.isPlaying = info.isPlaying;

            // Update known-good fields only; keep cached values if host omits (returns 0/<=0)
            if (info.bpm > 0.0)
                hostInfo.tempoBPM = info.bpm;

            if (info.timeSigNumerator > 0)
                hostInfo.timeSigNumerator = info.timeSigNumerator;

            // PPQ: allow 0.0 at the exact start when playing; otherwise, if host provides non-zero, accept it.
            if (info.isPlaying || info.ppqPosition != 0.0)
                hostInfo.ppqPosition = info.ppqPosition;
        }
    }

    // Compute subdivision crossing for this block (will be used by sequencer in Phase 3)
    const auto crossing = timing.findFirstSubdivisionCrossing(hostInfo, buffer.getNumSamples());
    (void)crossing; // placeholder until Phase 3 emits gates

    // Emit silence deterministically for now (Phase 1/2 audio path remains silent)
    buffer.clear();
}

//==============================================================================
juce::AudioProcessorEditor* MetroGnomeAudioProcessor::createEditor()
{
    return new MetroGnomeAudioProcessorEditor (*this);
}

//==============================================================================
void MetroGnomeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // No parameters yet; write empty blob for forward compatibility.
    juce::MemoryOutputStream mos (destData, false);
    mos.writeInt (1); // version
}

void MetroGnomeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, static_cast<size_t>(sizeInBytes), false);
    if (mis.getTotalLength() >= sizeof (int))
        (void) mis.readInt(); // version
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MetroGnomeAudioProcessor();
}
