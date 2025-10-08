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
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha(0.7f));
        setColour (juce::Slider::thumbColourId, juce::Colours::orange);
        setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::ToggleButton::tickColourId, juce::Colours::limegreen);

        // Remove borders and backgrounds around slider text inputs globally
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        // Use dark, bold text for overlaid text boxes
        setColour (juce::TextEditor::textColourId, juce::Colours::black);
    }

    // Ensure slider text boxes are centred over the knob, transparent, and only editable on double-click
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* l = new juce::Label();
        l->setJustificationType (juce::Justification::centred);
        l->setInterceptsMouseClicks (false, false); // let the label itself not block knob drags
        l->setColour (juce::Label::textColourId, juce::Colours::black);
        l->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);

        // Bold font for readability over the knob
        auto f = l->getFont();
        l->setFont (f.boldened());

        // Not editable on single click; editable when double-clicked; return to non-edit on loss of focus
        l->setEditable (false, true, false);

        // Ensure the label sits on top of the slider's graphics
        l->toFront (false);

        return l;
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto area = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(6.0f);
        auto radius = juce::jmin(area.getWidth(), area.getHeight()) / 2.0f;
        auto centre = area.getCentre();

        auto outline = slider.findColour(juce::Slider::rotarySliderOutlineColourId);
        auto fill    = slider.findColour(juce::Slider::rotarySliderFillColourId);
        auto thumb   = slider.findColour(juce::Slider::thumbColourId);

        // Removed separate ellipse backplate to avoid per-knob halo
        // Knob face with subtle gradient
        juce::ColourGradient grad(fill.brighter(0.25f), centre.x, centre.y - radius,
                                  fill.darker(0.5f),   centre.x, centre.y + radius, false);
        grad.addColour(0.5, fill);
        g.setGradientFill(grad);
        auto circleArea = juce::Rectangle<float>(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.fillEllipse(circleArea.reduced(4.0f));

        // Value arc
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, radius - 6.0f, radius - 6.0f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(thumb.withAlpha(0.95f));
        g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Ticks
        g.setColour(outline.brighter(0.2f).withAlpha(0.6f));
        const int ticks = 12;
        for (int i = 0; i <= ticks; ++i)
        {
            const float t = (float)i / (float)ticks;
            const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            auto p1 = centre.getPointOnCircumference(radius - 2.0f, a);
            auto p2 = centre.getPointOnCircumference(radius - 8.0f, a);
            g.drawLine({ p1, p2 }, 1.0f);
        }

        // Pointer
        auto tip = centre.getPointOnCircumference(radius - 10.0f, angle);
        g.setColour(thumb);
        g.drawLine(centre.x, centre.y, tip.x, tip.y, 2.0f);
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
    {
        // Default to base for non-rotary sliders
        auto style = slider.getSliderStyle();
        const bool isRotary = (style == juce::Slider::Rotary ||
                               style == juce::Slider::RotaryHorizontalDrag ||
                               style == juce::Slider::RotaryVerticalDrag ||
                               style == juce::Slider::RotaryHorizontalVerticalDrag);

        if (! isRotary)
            return juce::LookAndFeel_V4::getSliderLayout(slider);

        juce::Slider::SliderLayout layout;
        auto r = slider.getLocalBounds().reduced(6);
        layout.sliderBounds = r; // knob uses full given bounds

        // Size of the inline text box
        const int boxW = juce::jmin(64, r.getWidth() - 8);
        const int boxH = 22;
        juce::Rectangle<int> box(boxW, boxH);
        box.setCentre(r.getCentre());
        layout.textBoxBounds = box;
        return layout;
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
    setWantsKeyboardFocus (false);

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
    // Show volume as whole-number percent and parse % input
    volumeSlider.setTextValueSuffix("%");
    volumeSlider.textFromValueFunction = [] (double v)
    {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    volumeSlider.valueFromTextFunction = [] (const juce::String& t)
    {
        auto s = t.trim();
        if (s.endsWithChar('%')) s = s.dropLastCharacters(1);
        double pct = s.getDoubleValue();
        pct = juce::jlimit(0.0, 100.0, pct);
        return pct / 100.0;
    };
    addAndMakeVisible(volumeSlider);
    volumeAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, kParamVolume, volumeSlider);

    // Labels above rotary controls
    stepsLabel.setText("Steps", juce::dontSendNotification);
    beatsPerBarLabel.setText("Beats-Per-Bar", juce::dontSendNotification);
    volumeLabel.setText("Volume", juce::dontSendNotification);
    for (auto* lbl : { &stepsLabel, &beatsPerBarLabel, &volumeLabel })
    {
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setColour(juce::Label::textColourId, juce::Colours::white);
        lbl->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*lbl);
    }

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

    // Step toggles 16
    for (int i = 0; i < 16; ++i)
    {
        auto* tb = new juce::ToggleButton(juce::String(i + 1));
        tb->setClickingTogglesState(true);
        tb->setTriggeredOnMouseDown(true);
        tb->setInterceptsMouseClicks(true, false);
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

    // Header bar behind rotary controls using the rotary fill colour (edge-to-edge)
    {
        const int headerHeight = 100;
        auto headerArea = getLocalBounds().removeFromTop(headerHeight);
        auto headerColour = findColour(juce::Slider::rotarySliderFillColourId).darker(0.35f);
        g.setColour(headerColour);
        g.fillRect(headerArea);
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

    // Header with 3 columns for the rotary controls
    const int headerHeight = 100;
    const int labelHeight = 20;
    const int knobWidth = 180;
    const int gap = 10;

    auto headerRectFull = area.removeFromTop(headerHeight);

    // Center the three columns within the header for symmetric distribution
    const int totalColsW = knobWidth * 3 + gap * 2;
    const int startX = headerRectFull.getX() + (headerRectFull.getWidth() - totalColsW) / 2;
    juce::Rectangle<int> headerRect(startX, headerRectFull.getY(), totalColsW, headerRectFull.getHeight());

    juce::Rectangle<int> col1 = headerRect.removeFromLeft(knobWidth);
    headerRect.removeFromLeft(gap); // spacer
    juce::Rectangle<int> col2 = headerRect.removeFromLeft(knobWidth);
    headerRect.removeFromLeft(gap); // spacer
    juce::Rectangle<int> col3 = headerRect.removeFromLeft(knobWidth);

    // Labels above sliders
    stepsLabel.setBounds(col1.removeFromTop(labelHeight));
    beatsPerBarLabel.setBounds(col2.removeFromTop(labelHeight));
    volumeLabel.setBounds(col3.removeFromTop(labelHeight));

    // Sliders fill remaining space in their columns
    stepsSlider.setBounds(col1);
    timeSigSlider.setBounds(col2);
    volumeSlider.setBounds(col3);

    auto mid = area.removeFromTop(360); // step row lives in paint overlay
    juce::ignoreUnused(mid);

    auto bottom = area.removeFromBottom(160);
    auto buttons = bottom.removeFromTop(40);
    enableAllBtn.setBounds(buttons.removeFromLeft(140));
    disableAllBtn.setBounds(buttons.removeFromLeft(140).withX(enableAllBtn.getRight() + 10));
    danceToggle.setBounds(bottom.removeFromTop(30));


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
    repaint();
}
