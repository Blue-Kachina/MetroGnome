#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifndef METROG_DEBUG_TIMING
#define METROG_DEBUG_TIMING 0
#endif

// Param IDs
static constexpr const char* kParamStepCount = "stepCount";
static constexpr const char* kParamEnableAll = "enableAll";
static constexpr const char* kParamDisableAll = "disableAll";
static constexpr const char* kParamVolume = "volume";
static constexpr const char* kParamDanceMode = "danceMode";
static juce::String stepEnabledId (int idx) { return juce::String("stepEnabled_") + juce::String(idx + 1); }

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
    , apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Cache raw parameter pointers for RT-safe access in audio thread
    stepCountParam = apvts.getRawParameterValue(kParamStepCount);
    enableAllParam = apvts.getRawParameterValue(kParamEnableAll);
    disableAllParam = apvts.getRawParameterValue(kParamDisableAll);
    for (int i = 0; i < 16; ++i)
        stepEnabledParams[i] = apvts.getRawParameterValue(stepEnabledId(i).toRawUTF8());
    volumeParam = apvts.getRawParameterValue(kParamVolume);
    danceModeParam = apvts.getRawParameterValue(kParamDanceMode);
}

MetroGnomeAudioProcessor::~MetroGnomeAudioProcessor() = default;

//==============================================================================
void MetroGnomeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // No dynamic allocations; ensure deterministic state.
    timing.prepare(sampleRate, samplesPerBlock);
    hostInfo.sampleRate = sampleRate;

    // Initialize timing subdivisions to current step count
    const int stepCount = static_cast<int>(stepCountParam ? stepCountParam->load() : 8.0f);
    timing.setSubdivisionsPerBar(juce::jlimit(1, 16, stepCount));

    // Initialize click synth parameters (short sine burst with exponential decay)
    const double clickMs = 10.0; // 10 ms max length
    clickMaxSamples = static_cast<int>(std::round((clickMs * 0.001) * sampleRate));
    if (clickMaxSamples < 1) clickMaxSamples = 1;
    const double decayMs = 4.0; // ~4 ms decay constant
    const double tauSamples = (decayMs * 0.001) * sampleRate;
    if (tauSamples > 0.0)
        clickDecay = std::exp(-1.0 / tauSamples);
    else
        clickDecay = 0.0;
    const double freq = 3000.0; // 3 kHz click tone
    sinePhase = 0.0;
    sinePhaseInc = juce::MathConstants<double>::twoPi * freq / std::max(1.0, sampleRate);
    clickActive = false;
    clickEnv = 0.0;
    clickSampleIndex = 0;
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

    // Clear buffer at block start; we fully synthesize output
    buffer.clear();

    // Keep timing engine subdivisions synced with step count parameter (read atomically)
    const int stepCount = juce::jlimit(1, 16, static_cast<int>(stepCountParam ? stepCountParam->load() : 8.0f));
    if (timing.getSubdivisionsPerBar() != stepCount)
        timing.setSubdivisionsPerBar(stepCount);

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

    // Handle enable/disable-all actions atomically (momentary behavior)
    if (enableAllParam && enableAllParam->load() >= 0.5f)
    {
        for (int i = 0; i < 16; ++i)
            if (stepEnabledParams[i]) stepEnabledParams[i]->store(1.0f);
        enableAllParam->store(0.0f);
    }
    if (disableAllParam && disableAllParam->load() >= 0.5f)
    {
        for (int i = 0; i < 16; ++i)
            if (stepEnabledParams[i]) stepEnabledParams[i]->store(0.0f);
        disableAllParam->store(0.0f);
    }

    // Reset last gate at start of block
    lastGateSample = -1;
    lastGateStepIndex = -1;
    lastGateBarIndex = -1;

    // Compute subdivision crossing for this block and emit a gate if the target step is enabled
    const auto crossing = timing.findFirstSubdivisionCrossing(hostInfo, buffer.getNumSamples());

    if (crossing.crosses)
    {
        const int stepIdx = (stepCount > 0) ? (crossing.subdivisionIndex % stepCount) : 0;
        // Update UI-visible current step index regardless of enabled state
        currentStepIndex.store(stepIdx);

        const bool stepEnabled = (stepEnabledParams[stepIdx] != nullptr) && (stepEnabledParams[stepIdx]->load() >= 0.5f);
        if (stepEnabled)
        {
            lastGateSample = crossing.firstCrossingSample;
            lastGateStepIndex = stepIdx;
            lastGateBarIndex = crossing.barIndex;
#if METROG_DEBUG_TIMING
            DBG ("[Gate] bar=" << lastGateBarIndex
                 << " step=" << lastGateStepIndex
                 << " sample@=" << lastGateSample
                 << " stepCount=" << stepCount);
#endif
        }
    }

    // Render click if active and/or retrigger at gate sample within this block (zero-latency)
    const int numSamples = buffer.getNumSamples();
    const int numChans = buffer.getNumChannels();
    const float vol = juce::jlimit(0.0f, 1.0f, volumeParam ? volumeParam->load() : 0.8f);

    for (int s = 0; s < numSamples; ++s)
    {
        if (lastGateSample == s)
        {
            // Retrigger click envelope exactly at gate sample within this block
            clickActive = true;
            clickEnv = 1.0;
            clickSampleIndex = 0;
            sinePhase = 0.0; // reset for sharp transient
        }

        float sampleValue = 0.0f;
        if (clickActive)
        {
            const float tone = static_cast<float>(std::sin(sinePhase));
            sinePhase += sinePhaseInc;
            if (sinePhase >= juce::MathConstants<double>::twoPi)
                sinePhase -= juce::MathConstants<double>::twoPi;

            const float env = static_cast<float>(clickEnv);
            sampleValue = env * tone * vol;

            // advance envelope
            clickEnv *= clickDecay;
            ++clickSampleIndex;
            if (clickSampleIndex >= clickMaxSamples || clickEnv < 1.0e-4)
                clickActive = false;
        }

        if (sampleValue != 0.0f)
        {
            for (int ch = 0; ch < numChans; ++ch)
            {
                buffer.getWritePointer(ch)[s] += sampleValue;
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* MetroGnomeAudioProcessor::createEditor()
{
    return new MetroGnomeAudioProcessorEditor (*this);
}

//==============================================================================
void MetroGnomeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Serialize parameter tree
    if (auto state = apvts.copyState(); true)
    {
        juce::MemoryOutputStream mos (destData, false);
        state.writeToStream(mos);
    }
}

void MetroGnomeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, static_cast<size_t>(sizeInBytes), false);
    juce::ValueTree vt = juce::ValueTree::readFromStream(mis);
    if (vt.isValid())
        apvts.replaceState(vt);
}

// Parameter layout
juce::AudioProcessorValueTreeState::ParameterLayout MetroGnomeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>(kParamStepCount, "Steps", 1, 16, 8));

    // Action buttons (momentary)
    params.push_back(std::make_unique<juce::AudioParameterBool>(kParamEnableAll, "Enable All", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(kParamDisableAll, "Disable All", false));

    // Output level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(kParamVolume, "Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.8f));

    // UI: Dance mode toggle
    params.push_back(std::make_unique<juce::AudioParameterBool>(kParamDanceMode, "Dance Mode", false));

    for (int i = 0; i < 16; ++i)
    {
        const auto id = stepEnabledId(i);
        const auto name = juce::String("Step ") + juce::String(i + 1) + juce::String(" Enabled");
        // Default: enabled for all steps
        params.push_back(std::make_unique<juce::AudioParameterBool>(id, name, true));
    }

    return { params.begin(), params.end() };
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MetroGnomeAudioProcessor();
}
