#include "PluginEditor.h"
#include "BinaryData.h"

MusiqueStereoEditor::MusiqueStereoEditor(MusiqueStereoProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);
    setSize(fx::dim::appW, fx::dim::appH);

    // Header
    titleLabel.setText("STEREO IMAGER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::header).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, fx::col::textPrimary);
    addAndMakeVisible(titleLabel);

    pluginIcon = juce::ImageCache::getFromMemory(BinaryData::icon_small_png, BinaryData::icon_small_pngSize);
    logoImg = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);

    auto setupHdrBtn = [&](juce::TextButton& b, bool toggle = false) {
        b.setColour(juce::TextButton::buttonColourId, fx::col::surfSecondary);
        b.setColour(juce::TextButton::textColourOffId, fx::col::textPrimary);
        if (toggle) b.setClickingTogglesState(true);
        addAndMakeVisible(b);
    };
    setupHdrBtn(bypassBtn, true);
    setupHdrBtn(monoBtn, true);
    setupHdrBtn(osBtn, true);
    setupHdrBtn(settingsBtn);
    fx::ui::markUnsupportedControl(settingsBtn);
    monoBtn.setTooltip("Force the output to mono before stereo processing");
    osBtn.setTooltip("Shows whether Haas is applied on the side channel and how mono-safe the image remains");
    osBtn.onClick = [] {};

    // Preset bar
    setupHdrBtn(prevBtn); setupHdrBtn(nextBtn); setupHdrBtn(saveBtn); setupHdrBtn(abBtn);
    addAndMakeVisible(presetBox);

    presets = std::make_shared<juce::Array<juce::var>>(fx::preset::loadAllPresets("fx-stereo"));
    if (presets->isEmpty()) { presetBox.addItem("Init", 1); presetBox.setSelectedId(1); }
    else
    {
        int id = 1;
        for (auto& pv : *presets)
            if (auto* o = pv.getDynamicObject())
                presetBox.addItem(o->getProperty("name").toString(), id++);
        presetBox.setSelectedItemIndex(0, juce::dontSendNotification);
        fx::preset::applyToAPVTS(proc.getAPVTS(), presets->getReference(0));
    }
    presetBox.onChange = [this] {
        int i = presetBox.getSelectedItemIndex();
        if (i >= 0 && i < presets->size()) fx::preset::applyToAPVTS(proc.getAPVTS(), presets->getReference(i));
    };
    prevBtn.onClick = [this] { int i = presetBox.getSelectedItemIndex(); if (i > 0) presetBox.setSelectedItemIndex(i - 1); };
    nextBtn.onClick = [this] { int i = presetBox.getSelectedItemIndex(); if (i < presetBox.getNumItems() - 1) presetBox.setSelectedItemIndex(i + 1); };
    saveBtn.onClick = [this] {
        auto name = juce::String("User_") + juce::Time::getCurrentTime().formatted("%H%M%S");
        juce::StringArray ids {"width","balance","mid_gain","side_gain","bass_mono","haas","mix","output","bypass","mono"};
        if (fx::preset::saveUserPreset("fx-stereo", name, ids, proc.getAPVTS()))
        {
            *presets = fx::preset::loadAllPresets("fx-stereo");
            presetBox.clear();
            int id = 1;
            for (auto& pv : *presets)
                if (auto* o = pv.getDynamicObject()) presetBox.addItem(o->getProperty("name").toString(), id++);
            presetBox.setSelectedItemIndex(presetBox.getNumItems() - 1);
        }
    };

    // Knobs
    const char* labels[6] = {"WIDTH", "BALANCE", "MID GAIN", "SIDE GAIN", "BASS MONO", "HAAS"};
    for (int i = 0; i < 6; ++i)
    {
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 16);
        addAndMakeVisible(knobs[i]);
        knobLabels[i].setText(labels[i], juce::dontSendNotification);
        knobLabels[i].setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::label).withStyle("Bold")));
        knobLabels[i].setJustificationType(juce::Justification::centred);
        knobLabels[i].setColour(juce::Label::textColourId, fx::col::textMuted);
        addAndMakeVisible(knobLabels[i]);
    }

    // Footer
    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);
    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(mixSlider);
    outputSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(outputSlider);
    activeLED.setAccent(fx::accent::pitch);
    addAndMakeVisible(activeLED);
    versionLabel.setText("Musique Stereo v1.0", juce::dontSendNotification);
    versionLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::footer)));
    versionLabel.setColour(juce::Label::textColourId, fx::col::textMuted);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel);

    // Attachments
    widthAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "width",     knobs[0]);
    balAtt   = std::make_unique<SliderAttach>(proc.getAPVTS(), "balance",   knobs[1]);
    midAtt   = std::make_unique<SliderAttach>(proc.getAPVTS(), "mid_gain",  knobs[2]);
    sideAtt  = std::make_unique<SliderAttach>(proc.getAPVTS(), "side_gain", knobs[3]);
    bassAtt  = std::make_unique<SliderAttach>(proc.getAPVTS(), "bass_mono", knobs[4]);
    haasAtt  = std::make_unique<SliderAttach>(proc.getAPVTS(), "haas",      knobs[5]);
    mixAtt   = std::make_unique<SliderAttach>(proc.getAPVTS(), "mix",       mixSlider);
    outAtt   = std::make_unique<SliderAttach>(proc.getAPVTS(), "output",    outputSlider);
    
    bypassAtt = std::make_unique<ButtonAttach>(proc.getAPVTS(), "bypass", bypassBtn);
    monoAtt   = std::make_unique<ButtonAttach>(proc.getAPVTS(), "mono",   monoBtn);

    startTimerHz(fx::anim::fftRefreshHz);
}

MusiqueStereoEditor::~MusiqueStereoEditor() { setLookAndFeel(nullptr); }

void MusiqueStereoEditor::timerCallback()
{
    const auto inputLevels = proc.getVisualState().getInputLevels();
    const auto outputLevels = proc.getVisualState().getOutputLevels();
    inMeter.setLevel(inputLevels.left, inputLevels.right);
    outMeter.setLevel(outputLevels.left, outputLevels.right);

    phase += 0.05f;
    if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;

    const bool mono = proc.getAPVTS().getRawParameterValue("mono")->load() > 0.5f;
    const float haasMs = proc.getAPVTS().getRawParameterValue("haas")->load();
    const auto stereoSnapshot = proc.getStereoFieldState().getSnapshot();
    const float correlation = stereoSnapshot.correlation;
    monoBtn.setButtonText(mono ? "MONO IN" : "STEREO IN");
    osBtn.setToggleState(haasMs > 0.05f, juce::dontSendNotification);
    osBtn.setButtonText(haasMs > 0.05f ? (correlation < 0.0f ? "MONO RISK" : "HAAS ON") : "MONO SAFE");
    osBtn.setColour(juce::TextButton::buttonColourId,
        haasMs <= 0.05f ? fx::col::surfSecondary
                        : correlation < 0.0f ? fx::col::meterClip.withAlpha(0.20f)
                                             : fx::accent::pitch.withAlpha(0.20f));
    osBtn.setColour(juce::TextButton::textColourOffId,
        haasMs <= 0.05f ? fx::col::textPrimary
                        : correlation < 0.0f ? fx::col::meterClip
                                             : fx::col::textPrimary);
    
    const bool isBypassed = proc.getAPVTS().getRawParameterValue("bypass")->load() > 0.5f;
    activeLED.setOn(!isBypassed && juce::jmax(outputLevels.left, outputLevels.right) > 0.02f);
    
    repaint(0, fx::dim::headerH + fx::dim::presetBarH, getWidth(), fx::dim::visualH);
}

void MusiqueStereoEditor::paintVisualization(juce::Graphics& g, juce::Rectangle<int> area)
{
    const float cx = area.getCentreX();
    const float cy = area.getCentreY() - 10.0f;
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.38f;
    const auto stereoField = proc.getStereoFieldState().getSnapshot();
    const auto outputLevels = proc.getVisualState().getOutputLevels();
    const float widthNorm = juce::jlimit(0.0f, 1.0f, stereoField.width / 1.25f);
    const float balNorm = stereoField.balance;
    const float correlation = stereoField.correlation;
    const float activity = juce::jlimit(0.15f, 1.0f, 0.5f * (outputLevels.left + outputLevels.right));
    const float haasMs = proc.getAPVTS().getRawParameterValue("haas")->load();
    const bool mono = proc.getAPVTS().getRawParameterValue("mono")->load() > 0.5f;

    auto drawBadge = [&](juce::Rectangle<float> rect, const juce::String& text, juce::Colour colour)
    {
        g.setColour(colour.withAlpha(0.16f));
        g.fillRoundedRectangle(rect, 8.0f);
        g.setColour(colour.withAlpha(0.6f));
        g.drawRoundedRectangle(rect, 8.0f, 1.0f);
        g.setColour(fx::col::textPrimary);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
        g.drawText(text, rect.toNearestInt(), juce::Justification::centred);
    };

    drawBadge({ (float) area.getX() + 16.0f, (float) area.getY() + 14.0f, 98.0f, 22.0f }, mono ? "INPUT MONO" : "INPUT STEREO", fx::col::textSecondary);
    drawBadge({ (float) area.getRight() - 210.0f, (float) area.getY() + 14.0f, 92.0f, 22.0f }, haasMs > 0.05f ? "SIDE HAAS" : "NO HAAS", fx::accent::pitch);
    drawBadge({ (float) area.getRight() - 110.0f, (float) area.getY() + 14.0f, 92.0f, 22.0f }, correlation < 0.0f ? "CHECK MONO" : "MONO OK", correlation < 0.0f ? fx::col::meterClip : fx::col::textSecondary);

    // Draw Vectorscope Grid
    g.setColour(fx::col::gridMajor);
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    
    g.setColour(fx::col::gridMinor);
    // L and R axes (rotated 45 degrees)
    float diag = radius * 0.7071f; // sin(45)
    g.drawLine(cx - diag, cy + diag, cx + diag, cy - diag, 1.0f); // L axis
    g.drawLine(cx - diag, cy - diag, cx + diag, cy + diag, 1.0f); // R axis
    g.drawLine(cx, cy - radius, cx, cy + radius, 1.0f); // M axis
    g.drawLine(cx - radius, cy, cx + radius, cy, 1.0f); // S axis

    // Labels
    g.setColour(fx::col::textMuted);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    g.drawText("M", (int)(cx - 10), (int)(cy - radius - 15), 20, 10, juce::Justification::centred);
    g.drawText("S", (int)(cx - radius - 15), (int)(cy - 5), 10, 10, juce::Justification::centred);
    g.drawText("L", (int)(cx - diag - 15), (int)(cy - diag - 10), 15, 10, juce::Justification::centred);
    g.drawText("R", (int)(cx + diag + 5), (int)(cy - diag - 10), 15, 10, juce::Justification::centred);

    // Draw signal-driven field trace
    juce::Path lissajous;
    bool started = false;
    const int numPoints = 220;
    const float xScale = radius * (0.15f + widthNorm * 0.95f) * activity;
    const float yScale = radius * juce::jmap(correlation, -1.0f, 1.0f, 0.22f, 0.95f) * activity;
    const float rotation = juce::jmap(balNorm, -1.0f, 1.0f, -0.3f, 0.3f);
    
    for (int i = 0; i <= numPoints; ++i)
    {
        const float t = (float) i / (float) numPoints * juce::MathConstants<float>::twoPi;
        const float wobble = 1.0f + 0.12f * std::sin(t * 3.0f + phase * 1.4f);
        const float baseX = std::cos(t) * xScale * wobble;
        const float baseY = std::sin(t) * yScale * (1.0f + 0.08f * std::sin(t * 4.0f - phase));
        const float px = cx + baseX * std::cos(rotation) - baseY * std::sin(rotation) + balNorm * radius * 0.18f;
        const float py = cy + baseX * std::sin(rotation) + baseY * std::cos(rotation);

        if (!started) { lissajous.startNewSubPath(px, py); started = true; }
        else lissajous.lineTo(px, py);
    }

    g.setColour(fx::accent::pitch.withAlpha(0.16f));
    g.strokePath(lissajous, juce::PathStrokeType(5.0f));
    g.setColour(fx::accent::pitch.withAlpha(0.75f));
    g.strokePath(lissajous, juce::PathStrokeType(1.8f));
    
    // Correlation Meter at the bottom
    float corrY = cy + radius + 25.0f;
    float corrW = radius * 1.5f;
    float corrX = cx - corrW * 0.5f;
    
    g.setColour(fx::col::gridMinor);
    g.fillRect(corrX, corrY, corrW, 6.0f);
    
    float corrMarkerX = cx + correlation * corrW * 0.5f;
    g.setColour(correlation > 0.0f ? fx::accent::pitch : fx::col::meterClip);
    
    if (correlation > 0.0f)
        g.fillRect(cx, corrY, corrMarkerX - cx, 6.0f);
    else
        g.fillRect(corrMarkerX, corrY, cx - corrMarkerX, 6.0f);
    
    g.setColour(fx::col::textPrimary);
    g.drawVerticalLine((int)corrMarkerX, corrY - 2.0f, corrY + 8.0f);
    g.drawVerticalLine((int)cx, corrY - 2.0f, corrY + 8.0f); // Center zero line
    
    g.setColour(fx::col::textMuted);
    g.drawText("-1", (int)(corrX - 20), (int)(corrY - 2), 15, 10, juce::Justification::centredRight);
    g.drawText("+1", (int)(corrX + corrW + 5), (int)(corrY - 2), 15, 10, juce::Justification::centredLeft);

    g.drawText("WIDTH " + juce::String(stereoField.width, 2), area.getX() + 18, area.getY() + 16, 92, 12, juce::Justification::left);
    g.drawText("BAL " + juce::String(balNorm, 2), area.getRight() - 122, area.getY() + 16, 104, 12, juce::Justification::right);
    g.drawText("HAAS " + juce::String(haasMs, 1) + " ms on side", area.getX() + 18, area.getBottom() - 18, 150, 12, juce::Justification::left);
}

void MusiqueStereoEditor::paint(juce::Graphics& g)
{
    g.fillAll(fx::col::bg);
    fx::paint::header(g, getWidth(), fx::accent::pitch);
    if (pluginIcon.isValid())
        g.drawImage(pluginIcon, juce::Rectangle<float>(12, 10, 40, 40), juce::RectanglePlacement::centred);
    fx::paint::presetBar(g, getWidth());
    fx::paint::graphArea(g, getWidth());
    fx::paint::graphGrid(g, getWidth());
    paintVisualization(g, juce::Rectangle<int>(0, fx::dim::headerH + fx::dim::presetBarH, getWidth(), fx::dim::visualH));
    fx::paint::controls(g, getWidth(), 6);
    fx::paint::footer(g, getWidth());
    if (logoImg.isValid())
    {
        const int fy = fx::dim::appH - fx::dim::footerH;
        g.drawImage(logoImg, juce::Rectangle<float>((float)getWidth() - 52.0f, (float)fy + 4.0f, 32.0f, 32.0f), juce::RectanglePlacement::centred);
    }
    fx::paint::footerLabel(g, "MIX", 80, 120);
    fx::paint::footerLabel(g, "OUT", 210, 120);
    fx::paint::outline(g, getLocalBounds());
}

void MusiqueStereoEditor::resized()
{
    // Header
    titleLabel.setBounds(56, 10, 160, 40);
    bypassBtn.setBounds(getWidth() - 310, 16, 64, fx::dim::btnH);
    monoBtn.setBounds(getWidth() - 286, 16, 96, fx::dim::btnH);
    osBtn.setBounds(getWidth() - 184, 16, 76, fx::dim::btnH);
    settingsBtn.setBounds(getWidth() - 102, 16, 42, fx::dim::btnH);

    // Preset bar
    const int py = fx::dim::headerH + 11;
    prevBtn.setBounds(260, py, 30, fx::dim::btnH);
    presetBox.setBounds(294, py, 250, fx::dim::btnH);
    nextBtn.setBounds(548, py, 30, fx::dim::btnH);
    saveBtn.setBounds(590, py, 56, fx::dim::btnH);
    abBtn.setBounds(652, py, 48, fx::dim::btnH);

    // Knobs
    const int ctrlTop = fx::dim::headerH + fx::dim::presetBarH + fx::dim::visualH;
    const int numKnobs = 6;
    const int kW = getWidth() / numKnobs;
    const int kY = ctrlTop + 14;
    for (int i = 0; i < numKnobs; ++i)
    {
        int x = i * kW;
        knobs[i].setBounds(x + (kW - 92) / 2, kY, 92, 90);
        knobLabels[i].setBounds(x + (kW - 120) / 2, kY + 92, 120, 16);
    }

    // Footer
    const int fy = fx::dim::appH - fx::dim::footerH;
    inMeter.setBounds(16, fy + 6, 20, fx::dim::footerH - 12);
    outMeter.setBounds(42, fy + 6, 20, fx::dim::footerH - 12);
    mixSlider.setBounds(80, fy + 8, 120, 24);
    outputSlider.setBounds(210, fy + 8, 120, 24);
    activeLED.setBounds(350, fy + 14, 12, 12);
    versionLabel.setBounds(getWidth() - 220, fy + 8, 160, 24);
}
