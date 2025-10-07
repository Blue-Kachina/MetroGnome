#include "PluginEditor.h"
#include "PluginProcessor.h"

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

    loadBackgroundImages();

    auto& apvts = processor.getAPVTS();

    // Sliders
    stepsSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    stepsSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    stepsSlider.setRange(1.0, 16.0, 1.0);
    stepsSlider.setDoubleClickReturnValue(true, 8.0);
    stepsSlider.setTitle("Steps / Numerator");
    addAndMakeVisible(stepsSlider);
    stepsAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, kParamStepCount, stepsSlider);

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

    // Dance toggle
    addAndMakeVisible(danceToggle);
    danceAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, kParamDanceMode, danceToggle);

    // Step toggles 16
    for (int i = 0; i < 16; ++i)
    {
        auto* tb = new juce::ToggleButton(juce::String(i + 1));
        tb->setClickingTogglesState(true);
        tb->setColour(juce::ToggleButton::textColourId, juce::Colours::silver);
        tb->setTooltip("Enable step " + juce::String(i + 1));
        tb->setWantsKeyboardFocus(false);
        stepToggles.add(tb);
        addAndMakeVisible(tb);

        auto* att = new APVTS::ButtonAttachment(apvts, stepEnabledId(i), *tb);
        stepAttachments.add(att);
    }

    // 60 FPS timer for smooth UI
    startTimerHz(60);
}

MetroGnomeAudioProcessorEditor::~MetroGnomeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
}

void MetroGnomeAudioProcessorEditor::loadBackgroundImages()
{
    // Preload known images
    auto assetsDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile("assets").getChildFile("images");

    // Prefer project-relative path as fallback (use working directory root)
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
    const bool dance = processor.getAPVTS().getRawParameterValue(kParamDanceMode)->load() >= 0.5f;
    const int stepIdx = processor.getCurrentStepIndex();
    const bool even = (stepIdx % 2) == 0;

    juce::Image bg;
    if (dance)
        bg = even ? bgA : bgB;
    else
        bg = bgA.isValid() ? bgA : bgB;

    if (bg.isValid())
        g.drawImageWithin(bg, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll(juce::Colours::black);

    // Step lights overlay
    auto bounds = getLocalBounds();

    // Define grid area for steps near middle of UI
    auto gridArea = bounds.reduced(20).removeFromTop(300).withY(300);

    const int cols = 4;
    const int rows = 4;
    const int pad = 10;
    const int cellW = (gridArea.getWidth() - pad * (cols - 1)) / cols;
    const int cellH = (gridArea.getHeight() - pad * (rows - 1)) / rows;

    int idx = 0;
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            auto x = gridArea.getX() + c * (cellW + pad);
            auto y = gridArea.getY() + r * (cellH + pad);
            juce::Rectangle<int> cell (x, y, cellW, cellH);

            const bool enabled = processor.getAPVTS().getRawParameterValue(stepEnabledId(idx))->load() >= 0.5f;
            const bool isCurrent = (idx == (stepIdx % juce::jmax(1, (int)processor.getAPVTS().getRawParameterValue(kParamStepCount)->load())));

            auto color = enabled ? juce::Colours::limegreen : juce::Colours::darkred.darker(0.6f);
            if (isCurrent)
                color = color.brighter(0.8f);

            g.setColour (color.withAlpha(0.85f));
            g.fillRoundedRectangle(cell.toFloat().reduced(6.0f), 8.0f);

            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawRoundedRectangle(cell.toFloat().reduced(6.0f), 8.0f, 2.0f);

            ++idx;
        }
    }
}

void MetroGnomeAudioProcessorEditor::resized()
{
    // Layout controls in bottom area
    auto area = getLocalBounds().reduced(16);

    auto topBar = area.removeFromTop(60);
    stepsSlider.setBounds(topBar.removeFromLeft(160));
    volumeSlider.setBounds(topBar.removeFromLeft(160).withX(stepsSlider.getRight() + 10));

    auto mid = area.removeFromTop(360); // step grid lives in paint overlay
    juce::ignoreUnused(mid);

    auto bottom = area.removeFromBottom(120);
    auto buttons = bottom.removeFromTop(40);
    enableAllBtn.setBounds(buttons.removeFromLeft(140));
    disableAllBtn.setBounds(buttons.removeFromLeft(140).withX(enableAllBtn.getRight() + 10));
    danceToggle.setBounds(bottom.removeFromTop(30));

    // Place step toggles invisibly aligned below grid for accessibility; we still want keyboard toggling
    auto gridArea = getLocalBounds().reduced(20).removeFromTop(300).withY(300);
    const int cols = 4;
    const int rows = 4;
    const int pad = 10;
    const int cellW = (gridArea.getWidth() - pad * (cols - 1)) / cols;
    const int cellH = (gridArea.getHeight() - pad * (rows - 1)) / rows;

    int idx = 0;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
        {
            auto x = gridArea.getX() + c * (cellW + pad);
            auto y = gridArea.getY() + r * (cellH + pad);
            juce::Rectangle<int> cell (x, y, cellW, cellH);
            if (auto* tb = stepToggles[idx])
            {
                tb->setBounds(cell);
                tb->setAlpha(0.0f); // visually hidden but focusable
                tb->setColour(juce::ToggleButton::textColourId, juce::Colours::transparentBlack);
            }
            ++idx;
        }
}

void MetroGnomeAudioProcessorEditor::timerCallback()
{
    repaint();
}
