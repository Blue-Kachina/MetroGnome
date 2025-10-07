#include "PluginEditor.h"
#include "PluginProcessor.h"

#if __has_include("BinaryData.h")
 #include "BinaryData.h"
 #define METROG_HAVE_BINDATA 1
#else
 #define METROG_HAVE_BINDATA 0
#endif

#if __has_include("MetroAssets.h")
 #include "MetroAssets.h"
 #define METROG_HAVE_METROASSETS 1
#else
 #define METROG_HAVE_METROASSETS 0
#endif

using APVTS = juce::AudioProcessorValueTreeState;

class MetroGnomeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MetroGnomeLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colours::black);
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::dimgrey.brighter(0.2f));
        setColour (juce::Slider::thumbColourId, juce::Colours::orange);
        setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::ToggleButton::tickColourId, juce::Colours::limegreen);
    }
};

// TU-scoped LookAndFeel instance; declared before use
static MetroGnomeLookAndFeel laf;

//==============================================================================
// Param IDs (match processor)
static constexpr const char* kParamStepCount = "stepCount";
static constexpr const char* kParamEnableAll = "enableAll";
static constexpr const char* kParamDisableAll = "disableAll";
static constexpr const char* kParamVolume = "volume";
static constexpr const char* kParamDanceMode = "danceMode";
static constexpr const char* kParamTimeSigNum = "timeSigNum";
static juce::String stepEnabledId (int idx) { return juce::String("stepEnabled_") + juce::String(idx + 1); }

//==============================================================================
MetroGnomeAudioProcessorEditor::MetroGnomeAudioProcessorEditor (MetroGnomeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&laf);
    setWantsKeyboardFocus (true);

    // Fixed size per Phase 5
    setResizable (false, false);
    setSize (536, 1024);
    setOpaque (true); // we'll always paint background

    loadBackgroundImages();

    enableAllBtn.setButtonText("Enable All");
    disableAllBtn.setButtonText("Disable All");

    auto& apvts = processor.getAPVTS();

    // Sliders
    stepsSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    stepsSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    stepsSlider.setRange(1.0, 16.0, 1.0);
    stepsSlider.setDoubleClickReturnValue(true, 8.0);
    stepsSlider.setTitle("Steps");
    stepsSlider.setTooltip("Number of sequencer steps (independent from timing)");
    addAndMakeVisible(stepsSlider);
    stepsAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, kParamStepCount, stepsSlider);

    timeSigSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    timeSigSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    timeSigSlider.setRange(1.0, 16.0, 1.0);
    timeSigSlider.setDoubleClickReturnValue(true, 4.0);
    timeSigSlider.setTitle("Time Sig (n/x)");
    timeSigSlider.setTooltip("Time signature numerator driving the step advance rate");
    addAndMakeVisible(timeSigSlider);
    timeSigAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, kParamTimeSigNum, timeSigSlider);

    volumeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    volumeSlider.setRange(0.0, 1.0, 0.0);
    volumeSlider.setDoubleClickReturnValue(true, 0.8);
    volumeSlider.setTitle("Volume");
    addAndMakeVisible(volumeSlider);
    volumeAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, kParamVolume, volumeSlider);

    // Buttons
    addAndMakeVisible(enableAllBtn);
    addAndMakeVisible(disableAllBtn);
    enableAllBtn.setTooltip("Enable all steps");
    disableAllBtn.setTooltip("Disable all steps");
    enableAllAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, kParamEnableAll, enableAllBtn);
    disableAllAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, kParamDisableAll, disableAllBtn);

    // Ensure momentary action triggers parameter change explicitly
    enableAllBtn.onClick = [this]
    {
        if (auto* p = processor.getAPVTS().getParameter(kParamEnableAll))
            p->setValueNotifyingHost(1.0f);
    };
    disableAllBtn.onClick = [this]
    {
        if (auto* p = processor.getAPVTS().getParameter(kParamDisableAll))
            p->setValueNotifyingHost(1.0f);
    };

    // Dance toggle
    addAndMakeVisible(danceToggle);
    danceAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, kParamDanceMode, danceToggle);

    // Minimal MIDI learn UI
    addAndMakeVisible(learnVolumeBtn);
    addAndMakeVisible(clearVolumeBtn);
    addAndMakeVisible(learnStepsBtn);
    addAndMakeVisible(clearStepsBtn);

    learnVolumeBtn.onClick = [this]
    {
        processor.armMidiLearn(kParamVolume);
        learnVolumeBtn.setButtonText("Listening… move a CC");
    };
    clearVolumeBtn.onClick = [this]
    {
        processor.clearMidiMapping(kParamVolume);
    };
    learnStepsBtn.onClick = [this]
    {
        processor.armMidiLearn(kParamStepCount);
        learnStepsBtn.setButtonText("Listening… move a CC");
    };
    clearStepsBtn.onClick = [this]
    {
        processor.clearMidiMapping(kParamStepCount);
    };

    // Step toggles 16
    for (int i = 0; i < 16; ++i)
    {
        auto* tb = new juce::ToggleButton(juce::String(i + 1));
        tb->setClickingTogglesState(true);
        tb->setInterceptsMouseClicks(true, false);
        tb->setColour(juce::ToggleButton::textColourId, juce::Colours::silver);
        tb->setTooltip("Enable step " + juce::String(i + 1));
        tb->setWantsKeyboardFocus(false);
        stepToggles.add(tb);
        addAndMakeVisible(tb);

        // Explicitly notify host on click to ensure parameter toggles
        tb->onClick = [this, i]
        {
            if (auto* p = processor.getAPVTS().getParameter(stepEnabledId(i)))
                p->setValueNotifyingHost(stepToggles[i]->getToggleState() ? 1.0f : 0.0f);
        };

        auto* att = new APVTS::ButtonAttachment(apvts, stepEnabledId(i), *tb);
        stepAttachments.add(att);
    }

    // 60 FPS timer for smooth UI
    startTimerHz(60);

    // Ensure overlay step toggles are positioned on first open
    resized();
}

MetroGnomeAudioProcessorEditor::~MetroGnomeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
}

void MetroGnomeAudioProcessorEditor::loadBackgroundImages()
{
    // Try embedded BinaryData first for reliable asset access inside DAWs
#if METROG_HAVE_BINDATA
    // Try direct symbol names generated by juce_add_binary_data
    if (! bgA.isValid())
    {
        #if defined(BinaryData_metrognome_a_png)
        if (BinaryData::metrognome_a_pngSize > 0)
            bgA = juce::ImageFileFormat::loadFrom(BinaryData::metrognome_a_png, BinaryData::metrognome_a_pngSize);
        #else
        // Also try via getNamedResource in case symbol names differ
        int sz = 0;
        if (auto* d = BinaryData::getNamedResource("metrognome-a_png", sz))
            bgA = juce::ImageFileFormat::loadFrom(d, (size_t)sz);
        if (! bgA.isValid())
            if (auto* d2 = BinaryData::getNamedResource("metrognome_a_png", sz))
                bgA = juce::ImageFileFormat::loadFrom(d2, (size_t)sz);
        #endif
    }
    if (! bgB.isValid())
    {
        #if defined(BinaryData_metrognome_b_png)
        if (BinaryData::metrognome_b_pngSize > 0)
            bgB = juce::ImageFileFormat::loadFrom(BinaryData::metrognome_b_png, BinaryData::metrognome_b_pngSize);
        #else
        int sz = 0;
        if (auto* d = BinaryData::getNamedResource("metrognome-b_png", sz))
            bgB = juce::ImageFileFormat::loadFrom(d, (size_t)sz);
        if (! bgB.isValid())
            if (auto* d2 = BinaryData::getNamedResource("metrognome_b_png", sz))
                bgB = juce::ImageFileFormat::loadFrom(d2, (size_t)sz);
        #endif
    }
#endif

#if METROG_HAVE_METROASSETS
    // Our in-repo fallback embedded resources (namespace MetroAssets)
    if (! bgA.isValid())
    {
        if (MetroAssets::metrognomea_pngSize > 0)
            bgA = juce::ImageFileFormat::loadFrom(MetroAssets::metrognomea_png, MetroAssets::metrognomea_pngSize);
        if (! bgA.isValid())
        {
            int sz = 0;
            if (auto* d = MetroAssets::getNamedResource("metrognome-a_png", sz))
                bgA = juce::ImageFileFormat::loadFrom(d, (size_t)sz);
            if (! bgA.isValid())
                if (auto* d2 = MetroAssets::getNamedResource("metrognome_a_png", sz))
                    bgA = juce::ImageFileFormat::loadFrom(d2, (size_t)sz);
            if (! bgA.isValid())
                if (auto* d3 = MetroAssets::getNamedResource("metrognomea_png", sz))
                    bgA = juce::ImageFileFormat::loadFrom(d3, (size_t)sz);
        }
    }
    if (! bgB.isValid())
    {
        if (MetroAssets::metrognomeb_pngSize > 0)
            bgB = juce::ImageFileFormat::loadFrom(MetroAssets::metrognomeb_png, MetroAssets::metrognomeb_pngSize);
        if (! bgB.isValid())
        {
            int sz = 0;
            if (auto* d = MetroAssets::getNamedResource("metrognome-b_png", sz))
                bgB = juce::ImageFileFormat::loadFrom(d, (size_t)sz);
            if (! bgB.isValid())
                if (auto* d2 = MetroAssets::getNamedResource("metrognome_b_png", sz))
                    bgB = juce::ImageFileFormat::loadFrom(d2, (size_t)sz);
            if (! bgB.isValid())
                if (auto* d3 = MetroAssets::getNamedResource("metrognomeb_png", sz))
                    bgB = juce::ImageFileFormat::loadFrom(d3, (size_t)sz);
        }
    }
#endif

    // Fallback to disk paths (useful during development and standalone helper)
    auto assetsDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile("assets").getChildFile("images");
    juce::File projDir = juce::File::getCurrentWorkingDirectory();
    juce::File altDir = projDir.getChildFile("assets").getChildFile("images");

    auto loadIfExists = [] (const juce::File& f) -> juce::Image
    {
        if (f.existsAsFile())
            return juce::ImageFileFormat::loadFrom(f);
        return {};
    };

    if (! bgA.isValid())
    {
        bgA = loadIfExists(assetsDir.getChildFile("metrognome-a.png"));
        if (! bgA.isValid()) bgA = loadIfExists(altDir.getChildFile("metrognome-a.png"));
    }
    if (! bgB.isValid())
    {
        bgB = loadIfExists(assetsDir.getChildFile("metrognome-b.png"));
        if (! bgB.isValid()) bgB = loadIfExists(altDir.getChildFile("metrognome-b.png"));
    }
}

void MetroGnomeAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Choose background based on dance toggle and current step parity
    const auto* dancePtr = processor.getAPVTS().getRawParameterValue(kParamDanceMode);
    const bool dance = (dancePtr != nullptr) ? (dancePtr->load() >= 0.5f) : false;
    const int parity = processor.getDanceParity();
    const int stepIdx = processor.getCurrentStepIndex();

    juce::Image bg;
    if (dance)
        bg = (parity % 2 == 0) ? bgA : bgB;
    else
        bg = bgA.isValid() ? bgA : bgB;

    if (bg.isValid())
        g.drawImageWithin(bg, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);
    else {
        g.fillAll(juce::Colours::black);
       #if JUCE_DEBUG
        juce::String msg = "Background image not found. Tried BinaryData, MetroAssets, and disk paths.";
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawFittedText(msg, getLocalBounds().reduced(20), juce::Justification::centred, 3);
       #endif
    }

    // Step lights overlay - single row responsive layout
    auto bounds = getLocalBounds();

    // Define row area for steps near middle of UI
    auto rowArea = bounds.reduced(20);
    rowArea = rowArea.withTrimmedTop(260).withHeight(90); // y ~260..350

    const int pad = 0;

    // Safe default for step count if APVTS not yet initialised
    int safeStepCount = 8;
    if (const auto* sc = processor.getAPVTS().getRawParameterValue(kParamStepCount))
        safeStepCount = juce::jlimit(1, 16, (int)sc->load());

    const int n = safeStepCount;
    const int cellW = (rowArea.getWidth() - pad * (n - 1)) / n;
    const int cellH = rowArea.getHeight();

    for (int idx = 0; idx < n; ++idx)
    {
        auto x = rowArea.getX() + idx * (cellW + pad);
        auto y = rowArea.getY();
        juce::Rectangle<int> cell (x, y, cellW, cellH);

        bool enabled = false;
        if (auto* p = processor.getAPVTS().getRawParameterValue(stepEnabledId(idx)))
            enabled = p->load() >= 0.5f;

        const bool isCurrent = (idx == (stepIdx % juce::jmax(1, n)));

        auto color = enabled ? juce::Colours::limegreen : juce::Colours::darkred.darker(0.6f);
        if (isCurrent)
            color = color.brighter(0.8f);

        g.setColour (color.withAlpha(0.85f));
        g.fillRoundedRectangle(cell.toFloat(), 10.0f);

        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(cell.toFloat(), 10.0f, 2.0f);
    }
}

void MetroGnomeAudioProcessorEditor::resized()
{
    // Layout controls in bottom area
    auto area = getLocalBounds().reduced(16);

    auto topBar = area.removeFromTop(60);
    stepsSlider.setBounds(topBar.removeFromLeft(160));
    timeSigSlider.setBounds(topBar.removeFromLeft(160).withX(stepsSlider.getRight() + 10));
    volumeSlider.setBounds(topBar.removeFromLeft(160).withX(timeSigSlider.getRight() + 10));

    auto mid = area.removeFromTop(360); // step row lives in paint overlay
    juce::ignoreUnused(mid);

    auto bottom = area.removeFromBottom(160);
    auto buttons = bottom.removeFromTop(40);
    enableAllBtn.setBounds(buttons.removeFromLeft(140));
    disableAllBtn.setBounds(buttons.removeFromLeft(140).withX(enableAllBtn.getRight() + 10));
    danceToggle.setBounds(bottom.removeFromTop(30));

    // MIDI learn row 1 (Volume)
    auto learnRow1 = bottom.removeFromTop(30);
    learnVolumeBtn.setBounds(learnRow1.removeFromLeft(200));
    clearVolumeBtn.setBounds(learnRow1.removeFromLeft(60).withX(learnVolumeBtn.getRight() + 8));

    // MIDI learn row 2 (Steps)
    auto learnRow2 = bottom.removeFromTop(30);
    learnStepsBtn.setBounds(learnRow2.removeFromLeft(200));
    clearStepsBtn.setBounds(learnRow2.removeFromLeft(60).withX(learnStepsBtn.getRight() + 8));

    // Place step toggles over the single row cells for click-to-toggle interaction
    auto rowArea = getLocalBounds().reduced(20);
    rowArea = rowArea.withTrimmedTop(260).withHeight(90);
    const int pad = 0;

    int safeStepCount = 8;
    if (const auto* sc = processor.getAPVTS().getRawParameterValue(kParamStepCount))
        safeStepCount = juce::jlimit(1, 16, (int)sc->load());

    const int n = safeStepCount;
    const int cellW = (rowArea.getWidth() - pad * (n - 1)) / n;
    const int cellH = rowArea.getHeight();

    for (int idx = 0; idx < stepToggles.size(); ++idx)
    {
        if (auto* tb = stepToggles[idx])
        {
            if (idx < n)
            {
                auto x = rowArea.getX() + idx * (cellW + pad);
                auto y = rowArea.getY();
                juce::Rectangle<int> cell (x, y, cellW, cellH);
                tb->setBounds(cell);
                tb->toFront(false);
                tb->setAlpha(0.001f); // visually hidden but clickable
                tb->setColour(juce::ToggleButton::textColourId, juce::Colours::transparentBlack);
            }
            else
            {
                tb->setBounds(juce::Rectangle<int>(0, 0, 0, 0)); // hide
            }
        }
    }
}

void MetroGnomeAudioProcessorEditor::timerCallback()
{
    // Commit pending MIDI learn (from audio thread)
    if (processor.hasPendingMidiLearn())
    {
        if (processor.commitPendingMidiLearn())
        {
            // update button texts with mapped CC
            const int volCC = processor.getMappedCC(kParamVolume);
            const int stepCC = processor.getMappedCC(kParamStepCount);
            if (volCC >= 0) learnVolumeBtn.setButtonText(juce::String("MIDI: Volume (CC ") + juce::String(volCC) + ")");
            else            learnVolumeBtn.setButtonText("MIDI Learn: Volume");
            if (stepCC >= 0) learnStepsBtn.setButtonText(juce::String("MIDI: Steps (CC ") + juce::String(stepCC) + ")");
            else             learnStepsBtn.setButtonText("MIDI Learn: Steps");
        }
    }

    repaint();
}
