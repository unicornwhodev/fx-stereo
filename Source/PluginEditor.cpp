#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
float paramValue(juce::AudioProcessorValueTreeState& apvts, const char* id, float fallback = 0.0f)
{
    if (auto* p = apvts.getRawParameterValue(id))
        return p->load();
    return fallback;
}

void setParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

struct KnobSpec
{
    const char* id;
    const char* label;
};

std::array<KnobSpec, 6> specsForEngine(int engine)
{
    switch (juce::jlimit(0, stereofx::numEngines - 1, engine))
    {
        case stereofx::haas:
            return {{{ "haas_time", "TIME" }, { "haas_side", "SIDE" }, { "haas_tone", "TONE" }, { "haas_feedback", "FDBK" }, { "wide_safety", "SAFETY" }, { "mix", "MIX" }}};
        case stereofx::frequencyImager:
            return {{{ "img_low", "LOW" }, { "img_mid", "MID" }, { "img_high", "HIGH" }, { "img_xover1", "XOVER" }, { "wide_safety", "SAFETY" }, { "mix", "MIX" }}};
        case stereofx::spatial:
            return {{{ "spatial_depth", "DEPTH" }, { "spatial_angle", "ANGLE" }, { "spatial_air", "AIR" }, { "spatial_focus", "FOCUS" }, { "width", "WIDTH" }, { "mix", "MIX" }}};
        case stereofx::monoMaker:
            return {{{ "mono_freq", "FREQ" }, { "mono_strength", "STRENGTH" }, { "mono_slope", "SLOPE" }, { "mono_audition", "AUDITION" }, { "width", "WIDTH" }, { "mix", "MIX" }}};
        case stereofx::correlation:
            return {{{ "corr_hold", "HOLD" }, { "corr_decay", "DECAY" }, { "corr_zoom", "ZOOM" }, { "corr_warn", "WARN" }, { "balance", "BALANCE" }, { "mix", "MIX" }}};
        default:
            return {{{ "width", "WIDTH" }, { "balance", "BALANCE" }, { "mid_gain", "MID" }, { "side_gain", "SIDE" }, { "wide_safety", "SAFETY" }, { "mix", "MIX" }}};
    }
}

juce::String engineShortName(int engine)
{
    static const juce::StringArray names { "WIDENER", "HAAS", "IMAGER", "SPATIAL", "MONO", "CORR" };
    return names[juce::jlimit(0, names.size() - 1, engine)];
}
}

MusiqueStereoEditor::MusiqueStereoEditor(MusiqueStereoProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);
    setSize(fx::dim::appW, fx::dim::appH);

    titleLabel.setText("STEREO", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::header).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, fx::col::textPrimary);
    addAndMakeVisible(titleLabel);

    pluginIcon = juce::ImageCache::getFromMemory(BinaryData::icon_small_png, BinaryData::icon_small_pngSize);
    logoImg = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);

    auto setupBtn = [&](juce::TextButton& b, bool toggle = false) {
        b.setColour(juce::TextButton::buttonColourId, fx::col::surfSecondary);
        b.setColour(juce::TextButton::textColourOffId, fx::col::textPrimary);
        if (toggle)
            b.setClickingTogglesState(true);
        addAndMakeVisible(b);
    };
    setupBtn(bypassBtn, true);
    setupBtn(monoBtn, true);
    setupBtn(statusBtn);
    setupBtn(actionBtn);
    statusBtn.setTooltip("Live correlation and mono compatibility status");
    actionBtn.setTooltip("Cycle the active engine variant");

    setupBtn(prevBtn);
    setupBtn(nextBtn);
    setupBtn(saveBtn);
    setupBtn(abBtn);
    addAndMakeVisible(presetBox);
    addAndMakeVisible(engineBox);
    addAndMakeVisible(variantBox);
    engineBox.addItemList(MusiqueStereoProcessor::getEngineNames(), 1);

    presets = std::make_shared<juce::Array<juce::var>>(fx::preset::loadAllPresets("fx-stereo"));
    refreshPresetBox();

    presetBox.onChange = [this] {
        const int i = presetBox.getSelectedItemIndex();
        if (i >= 0 && i < presets->size())
        {
            storeCurrentABSlot();
            proc.applyPresetCompat(presets->getReference(i));
            abStateA = proc.getAPVTS().copyState();
            abStateB = abStateA.createCopy();
            showingA = true;
            abBtn.setButtonText("A/B");
            refreshEngineUi();
        }
    };
    prevBtn.onClick = [this] { const int i = presetBox.getSelectedItemIndex(); if (i > 0) presetBox.setSelectedItemIndex(i - 1); };
    nextBtn.onClick = [this] { const int i = presetBox.getSelectedItemIndex(); if (i < presetBox.getNumItems() - 1) presetBox.setSelectedItemIndex(i + 1); };
    saveBtn.onClick = [this] {
        const auto name = juce::String("User_") + juce::Time::getCurrentTime().formatted("%H%M%S");
        if (fx::preset::saveUserPreset("fx-stereo", name, MusiqueStereoProcessor::getAllParameterIds(), proc.getAPVTS()))
        {
            *presets = fx::preset::loadAllPresets("fx-stereo");
            refreshPresetBox();
            presetBox.setSelectedItemIndex(presetBox.getNumItems() - 1);
        }
    };
    abBtn.onClick = [this] {
        storeCurrentABSlot();
        recallABSlot(!showingA);
    };
    engineBox.onChange = [this] {
        setParam(proc.getAPVTS(), "engine", (float) engineBox.getSelectedItemIndex());
        setParam(proc.getAPVTS(), "variant", 0.0f);
        refreshEngineUi();
    };
    variantBox.onChange = [this] {
        setParam(proc.getAPVTS(), "variant", (float) variantBox.getSelectedItemIndex());
        refreshEngineUi();
    };
    actionBtn.onClick = [this] {
        const int engine = (int) paramValue(proc.getAPVTS(), "engine");
        const int count = MusiqueStereoProcessor::getVariantNames(engine).size();
        const int next = ((int) paramValue(proc.getAPVTS(), "variant") + 1) % juce::jmax(1, count);
        setParam(proc.getAPVTS(), "variant", (float) next);
        refreshEngineUi();
    };

    for (int i = 0; i < numKnobs; ++i)
        setupSlider(knobs[i], knobLabels[i], {});

    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);
    outputSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(outputSlider);
    activeLED.setAccent(fx::accent::pitch);
    addAndMakeVisible(activeLED);

    versionLabel.setText("Musique Stereo v1.1", juce::dontSendNotification);
    versionLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::footer)));
    versionLabel.setColour(juce::Label::textColourId, fx::col::textMuted);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel);

    outAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "output", outputSlider);
    bypassAtt = std::make_unique<ButtonAttach>(proc.getAPVTS(), "bypass", bypassBtn);
    monoAtt = std::make_unique<ButtonAttach>(proc.getAPVTS(), "mono", monoBtn);

    abStateA = proc.getAPVTS().copyState();
    abStateB = abStateA.createCopy();
    refreshEngineUi();
    startTimerHz(fx::anim::fftRefreshHz);
}

MusiqueStereoEditor::~MusiqueStereoEditor()
{
    setLookAndFeel(nullptr);
}

void MusiqueStereoEditor::setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    addAndMakeVisible(slider);
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::label).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, fx::col::textMuted);
    addAndMakeVisible(label);
}

void MusiqueStereoEditor::refreshPresetBox()
{
    presetBox.clear(juce::dontSendNotification);
    if (presets->isEmpty())
    {
        presetBox.addItem("Init", 1);
        presetBox.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    int id = 1;
    for (auto& pv : *presets)
        if (auto* o = pv.getDynamicObject())
            presetBox.addItem(o->getProperty("name").toString(), id++);
    presetBox.setSelectedItemIndex(0, juce::dontSendNotification);
}

void MusiqueStereoEditor::attachKnobsForEngine(int engine)
{
    const auto specs = specsForEngine(engine);
    for (auto& attachment : knobAtts)
        attachment.reset();

    for (int i = 0; i < numKnobs; ++i)
    {
        knobLabels[i].setText(specs[(size_t) i].label, juce::dontSendNotification);
        knobAtts[(size_t) i] = std::make_unique<SliderAttach>(proc.getAPVTS(), specs[(size_t) i].id, knobs[i]);
    }
}

void MusiqueStereoEditor::refreshEngineUi()
{
    const int engine = juce::jlimit(0, stereofx::numEngines - 1, (int) paramValue(proc.getAPVTS(), "engine"));
    const auto variants = MusiqueStereoProcessor::getVariantNames(engine);
    const int variant = juce::jlimit(0, variants.size() - 1, (int) paramValue(proc.getAPVTS(), "variant"));

    if (engineBox.getSelectedItemIndex() != engine)
        engineBox.setSelectedItemIndex(engine, juce::dontSendNotification);
    if (engine != lastEngine)
    {
        variantBox.clear(juce::dontSendNotification);
        variantBox.addItemList(variants, 1);
        attachKnobsForEngine(engine);
        lastEngine = engine;
    }
    if (variantBox.getSelectedItemIndex() != variant)
        variantBox.setSelectedItemIndex(variant, juce::dontSendNotification);
    actionBtn.setButtonText(variants[variant].toUpperCase());
    lastVariant = variant;
}

void MusiqueStereoEditor::storeCurrentABSlot()
{
    if (showingA)
        abStateA = proc.getAPVTS().copyState();
    else
        abStateB = proc.getAPVTS().copyState();
}

void MusiqueStereoEditor::recallABSlot(bool slotA)
{
    auto state = slotA ? abStateA : abStateB;
    if (state.isValid())
    {
        proc.getAPVTS().replaceState(state.createCopy());
        showingA = slotA;
        abBtn.setButtonText(showingA ? "A" : "B");
        refreshEngineUi();
    }
}

void MusiqueStereoEditor::timerCallback()
{
    const auto inputLevels = proc.getVisualState().getInputLevels();
    const auto outputLevels = proc.getVisualState().getOutputLevels();
    inMeter.setLevel(inputLevels.left, inputLevels.right);
    outMeter.setLevel(outputLevels.left, outputLevels.right);

    phase += 0.05f;
    if (phase > juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;

    const bool mono = paramValue(proc.getAPVTS(), "mono") > 0.5f;
    const auto snap = proc.getStereoSnapshot();
    monoBtn.setButtonText(mono ? "MONO IN" : "STEREO IN");
    statusBtn.setButtonText(snap.correlation < -0.05f ? "MONO RISK" : (snap.engine == stereofx::monoMaker ? "LOW MONO" : "MONO OK"));
    statusBtn.setColour(juce::TextButton::textColourOffId, snap.correlation < -0.05f ? fx::col::meterClip : fx::col::textPrimary);
    activeLED.setOn(paramValue(proc.getAPVTS(), "bypass") < 0.5f && juce::jmax(outputLevels.left, outputLevels.right) > 0.02f);

    const int engine = (int) paramValue(proc.getAPVTS(), "engine");
    const int variant = (int) paramValue(proc.getAPVTS(), "variant");
    if (engine != lastEngine || variant != lastVariant)
        refreshEngineUi();
    repaint(0, fx::dim::headerH + fx::dim::presetBarH, getWidth(), fx::dim::visualH);
}

void MusiqueStereoEditor::paintVisualization(juce::Graphics& g, juce::Rectangle<int> area)
{
    const float cx = area.getCentreX();
    const float cy = area.getCentreY() - 8.0f;
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.36f;
    const auto snap = proc.getStereoSnapshot();
    const float activity = juce::jlimit(0.15f, 1.0f, 0.5f * (proc.getVisualState().getOutputLevels().left + proc.getVisualState().getOutputLevels().right));
    const float corr = juce::jlimit(-1.0f, 1.0f, snap.correlation);
    const float widthNorm = juce::jlimit(0.0f, 1.0f, snap.width / 1.35f);

    auto badge = [&](juce::Rectangle<float> rect, const juce::String& text, juce::Colour colour) {
        g.setColour(colour.withAlpha(0.16f));
        g.fillRoundedRectangle(rect, 8.0f);
        g.setColour(colour.withAlpha(0.6f));
        g.drawRoundedRectangle(rect, 8.0f, 1.0f);
        g.setColour(fx::col::textPrimary);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
        g.drawText(text, rect.toNearestInt(), juce::Justification::centred);
    };
    badge({ (float) area.getX() + 16.0f, (float) area.getY() + 14.0f, 104.0f, 22.0f }, engineShortName(snap.engine), fx::accent::pitch);
    badge({ (float) area.getRight() - 124.0f, (float) area.getY() + 14.0f, 104.0f, 22.0f }, corr < -0.05f ? "CHECK MONO" : "MONO OK", corr < -0.05f ? fx::col::meterClip : fx::col::textSecondary);

    g.setColour(fx::col::gridMajor);
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    g.setColour(fx::col::gridMinor);
    g.drawLine(cx, cy - radius, cx, cy + radius, 1.0f);
    g.drawLine(cx - radius, cy, cx + radius, cy, 1.0f);
    const float diag = radius * 0.7071f;
    g.drawLine(cx - diag, cy + diag, cx + diag, cy - diag, 1.0f);
    g.drawLine(cx - diag, cy - diag, cx + diag, cy + diag, 1.0f);

    juce::Path trace;
    for (int i = 0; i <= 220; ++i)
    {
        const float t = (float) i / 220.0f * juce::MathConstants<float>::twoPi;
        const float x = std::cos(t) * radius * (0.18f + widthNorm * 0.92f) * activity;
        const float y = std::sin(t) * radius * juce::jmap(corr, -1.0f, 1.0f, 0.22f, 0.95f) * activity;
        const float wobble = std::sin(t * 3.0f + phase) * radius * 0.035f;
        const float px = cx + x + snap.balance * radius * 0.16f;
        const float py = cy + y + wobble;
        if (i == 0)
            trace.startNewSubPath(px, py);
        else
            trace.lineTo(px, py);
    }
    g.setColour(fx::accent::pitch.withAlpha(0.18f));
    g.strokePath(trace, juce::PathStrokeType(5.0f));
    g.setColour(fx::accent::pitch.withAlpha(0.78f));
    g.strokePath(trace, juce::PathStrokeType(1.8f));

    const float meterW = radius * 1.65f;
    const float meterX = cx - meterW * 0.5f;
    const float meterY = cy + radius + 22.0f;
    g.setColour(fx::col::gridMinor);
    g.fillRect(meterX, meterY, meterW, 6.0f);
    const float marker = cx + corr * meterW * 0.5f;
    g.setColour(corr >= 0.0f ? fx::accent::pitch : fx::col::meterClip);
    if (corr >= 0.0f)
        g.fillRect(cx, meterY, marker - cx, 6.0f);
    else
        g.fillRect(marker, meterY, cx - marker, 6.0f);
    g.setColour(fx::col::textPrimary);
    g.drawVerticalLine((int) marker, meterY - 2.0f, meterY + 8.0f);
    g.drawVerticalLine((int) cx, meterY - 2.0f, meterY + 8.0f);

    g.setColour(fx::col::textMuted);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    g.drawText("WIDTH " + juce::String(snap.width, 2), area.getX() + 18, area.getBottom() - 20, 110, 12, juce::Justification::left);
    g.drawText("CORR " + juce::String(corr, 2), area.getRight() - 128, area.getBottom() - 20, 110, 12, juce::Justification::right);
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
    fx::paint::controls(g, getWidth(), numKnobs);
    fx::paint::footer(g, getWidth());
    if (logoImg.isValid())
    {
        const int fy = fx::dim::appH - fx::dim::footerH;
        g.drawImage(logoImg, juce::Rectangle<float>((float) getWidth() - 52.0f, (float) fy + 4.0f, 32.0f, 32.0f), juce::RectanglePlacement::centred);
    }
    fx::paint::footerLabel(g, "OUT", 80, 180);
    fx::paint::outline(g, getLocalBounds());
}

void MusiqueStereoEditor::resized()
{
    titleLabel.setBounds(56, 10, 160, 40);
    bypassBtn.setBounds(getWidth() - 382, 16, 64, fx::dim::btnH);
    monoBtn.setBounds(getWidth() - 312, 16, 96, fx::dim::btnH);
    statusBtn.setBounds(getWidth() - 210, 16, 92, fx::dim::btnH);
    actionBtn.setBounds(getWidth() - 112, 16, 94, fx::dim::btnH);

    const int py = fx::dim::headerH + 11;
    prevBtn.setBounds(210, py, 30, fx::dim::btnH);
    presetBox.setBounds(244, py, 204, fx::dim::btnH);
    nextBtn.setBounds(452, py, 30, fx::dim::btnH);
    engineBox.setBounds(494, py, 158, fx::dim::btnH);
    variantBox.setBounds(660, py, 126, fx::dim::btnH);
    saveBtn.setBounds(798, py, 56, fx::dim::btnH);
    abBtn.setBounds(860, py, 48, fx::dim::btnH);

    const int ctrlTop = fx::dim::headerH + fx::dim::presetBarH + fx::dim::visualH;
    const int kW = getWidth() / numKnobs;
    const int kY = ctrlTop + 14;
    for (int i = 0; i < numKnobs; ++i)
    {
        const int x = i * kW;
        knobs[i].setBounds(x + (kW - 92) / 2, kY, 92, 90);
        knobLabels[i].setBounds(x + (kW - 126) / 2, kY + 92, 126, 16);
    }

    const int fy = fx::dim::appH - fx::dim::footerH;
    inMeter.setBounds(16, fy + 6, 20, fx::dim::footerH - 12);
    outMeter.setBounds(42, fy + 6, 20, fx::dim::footerH - 12);
    outputSlider.setBounds(80, fy + 8, 180, 24);
    activeLED.setBounds(280, fy + 14, 12, 12);
    versionLabel.setBounds(getWidth() - 230, fy + 8, 170, 24);
}
