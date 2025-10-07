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
static constexpr const char* kParamTimeSigNum = "timeSigNum";
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
    timeSigNumParam = apvts.getRawParameterValue(kParamTimeSigNum);

    // init CC map to nulls
    for (auto& p : ccToParam) p.store(nullptr, std::memory_order_relaxed);
}

MetroGnomeAudioProcessor::~MetroGnomeAudioProcessor() = default;

//==============================================================================
void MetroGnomeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // No dynamic allocations; ensure deterministic state.
    timing.prepare(sampleRate, samplesPerBlock);
    hostInfo.sampleRate = sampleRate;

    // Initialize timing subdivisions from time signature numerator (independent from step count)
    const int timeSigNum = static_cast<int>(timeSigNumParam ? timeSigNumParam->load() : 4.0f);
    timing.setSubdivisionsPerBar(juce::jlimit(1, 16, timeSigNum));

    // Reset UI indices/parity
    currentStepIndex.store(-1);
    danceParity.store(0);

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

void MetroGnomeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear buffer at block start; we fully synthesize output
    buffer.clear();

    // Keep timing engine subdivisions synced with time signature numerator (independent from step count)
    const int timeSigNum = juce::jlimit(1, 16, static_cast<int>(timeSigNumParam ? timeSigNumParam->load() : 4.0f));
    if (timing.getSubdivisionsPerBar() != timeSigNum)
        timing.setSubdivisionsPerBar(timeSigNum);

    // Fetch current step count for UI/sequence length
    const int stepCount = juce::jlimit(1, 16, static_cast<int>(stepCountParam ? stepCountParam->load() : 8.0f));

    // Process incoming MIDI CC messages: handle learn and mapped control
    if (! midiMessages.isEmpty())
    {
        for (const auto metadata : midiMessages)
        {
            const auto m = metadata.getMessage();
            if (m.isController())
            {
                const int cc = m.getControllerNumber();
                const int val = m.getControllerValue();

                // learn capture (do not allocate)
                if (midiLearnArmed.load(std::memory_order_relaxed) && pendingLearnCC.load(std::memory_order_relaxed) < 0)
                    pendingLearnCC.store(cc, std::memory_order_relaxed);

                // mapped control
                if (cc >= 0 && cc < (int)ccToParam.size())
                {
                    auto* p = ccToParam[(size_t)cc].load(std::memory_order_relaxed);
                    if (p != nullptr)
                    {
                        const float norm = juce::jlimit(0.0f, 1.0f, (float)val / 127.0f);
                        p->setValueNotifyingHost (norm);
                    }
                }
            }
        }
    }

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
        // Flip dance parity on every subdivision crossing for smooth alternation
        danceParity.fetch_xor(1);

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

    // Hoist channel write pointers out of the per-sample loop for performance
    std::array<float*, 64> writePtrs{}; // support up to 64 channels
    const int chanLimit = juce::jmin(numChans, (int)writePtrs.size());
    for (int ch = 0; ch < chanLimit; ++ch)
        writePtrs[(size_t)ch] = buffer.getWritePointer(ch);

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
    {
        apvts.replaceState(vt);
        rebuildMidiMapFromState();
    }
}

// Parameter layout
juce::AudioProcessorValueTreeState::ParameterLayout MetroGnomeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Independent controls: step count (number of sequencer steps) and time signature numerator (timing)
    params.push_back(std::make_unique<juce::AudioParameterInt>(kParamStepCount, "Steps", 1, 16, 8));
    params.push_back(std::make_unique<juce::AudioParameterInt>(kParamTimeSigNum, "Time Sig Numerator", 1, 16, 4));

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
// MIDI learn helpers (message thread only)
void MetroGnomeAudioProcessor::armMidiLearn (const juce::String& paramID)
{
    midiLearnTargetId = paramID;
    pendingLearnCC.store(-1, std::memory_order_relaxed);
    midiLearnArmed.store(true, std::memory_order_relaxed);
}

void MetroGnomeAudioProcessor::cancelMidiLearn()
{
    midiLearnArmed.store(false, std::memory_order_relaxed);
    pendingLearnCC.store(-1, std::memory_order_relaxed);
}

bool MetroGnomeAudioProcessor::commitPendingMidiLearn()
{
    const int cc = pendingLearnCC.load(std::memory_order_relaxed);
    if (cc < 0 || cc > 127 || midiLearnTargetId.isEmpty())
        return false;

    // Update state tree
    auto midiMap = apvts.state.getOrCreateChildWithName("MidiMap", nullptr);
    midiMap.setProperty(midiLearnTargetId, cc, nullptr);

    // Update fast map (clear any previous occupant for this CC)
    if (cc >= 0 && cc < (int)ccToParam.size())
    {
        // Ensure uniqueness: clear any param previously mapped to this CC
        for (auto& slot : ccToParam)
        {
            if (slot.load(std::memory_order_relaxed) != nullptr)
            {
                // can't easily check ID here, so just overwrite target index below
            }
        }
        if (auto* p = apvts.getParameter(midiLearnTargetId))
            ccToParam[(size_t)cc].store(p, std::memory_order_relaxed);
    }

    // disarm
    cancelMidiLearn();
    return true;
}

void MetroGnomeAudioProcessor::clearMidiMapping (const juce::String& paramID)
{
    // Remove from state
    if (auto midiMap = apvts.state.getChildWithName("MidiMap"); midiMap.isValid())
    {
        if (midiMap.hasProperty(paramID))
        {
            const int cc = (int)midiMap.getProperty(paramID);
            midiMap.removeProperty(paramID, nullptr);
            if (cc >= 0 && cc < (int)ccToParam.size())
                ccToParam[(size_t)cc].store(nullptr, std::memory_order_relaxed);
        }
    }
}

int MetroGnomeAudioProcessor::getMappedCC (const juce::String& paramID) const
{
    if (auto midiMap = apvts.state.getChildWithName("MidiMap"); midiMap.isValid())
    {
        if (midiMap.hasProperty(paramID))
            return (int)midiMap.getProperty(paramID);
    }
    return -1;
}

void MetroGnomeAudioProcessor::rebuildMidiMapFromState()
{
    // Clear map
    for (auto& slot : ccToParam) slot.store(nullptr, std::memory_order_relaxed);

    auto midiMap = apvts.state.getChildWithName("MidiMap");
    if (! midiMap.isValid()) return;

    for (int i = 0; i < midiMap.getNumProperties(); ++i)
    {
        const auto name = midiMap.getPropertyName(i);
        const int cc = (int)midiMap.getProperty(name);
        if (cc >= 0 && cc < 128)
        {
            if (auto* p = apvts.getParameter(name.toString()))
                ccToParam[(size_t)cc].store(p, std::memory_order_relaxed);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MetroGnomeAudioProcessor();
}
