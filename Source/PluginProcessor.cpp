#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FXComponents.h"

namespace
{
float raw(const juce::AudioProcessorValueTreeState& apvts, const char* id, float fallback = 0.0f)
{
    if (auto* value = apvts.getRawParameterValue(id))
        return value->load();
    return fallback;
}

void setParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

bool presetHas(const juce::var& preset, const juce::Identifier& id)
{
    if (auto* object = preset.getDynamicObject())
        return object->hasProperty(id);
    return false;
}

bool stateHasParameter(const juce::ValueTree& state, const juce::String& id)
{
    for (int index = 0; index < state.getNumChildren(); ++index)
    {
        auto child = state.getChild(index);
        if (child.hasType("PARAM") && child.getProperty("id").toString() == id)
            return true;
    }
    return false;
}

juce::NormalisableRange<float> percentRange()
{
    return { 0.0f, 100.0f, 0.01f };
}
}

juce::StringArray MusiqueStereoProcessor::getEngineNames()
{
    return { "Widener", "Haas", "Frequency Imager", "Spatial", "Mono Maker", "Correlation" };
}

juce::StringArray MusiqueStereoProcessor::getVariantNames(int engine)
{
    switch (juce::jlimit(0, stereofx::numEngines - 1, engine))
    {
        case stereofx::haas:            return { "Classic", "Wide", "Mono Safe" };
        case stereofx::frequencyImager: return { "Mix", "Master", "Air" };
        case stereofx::spatial:         return { "Stage", "Depth", "Air" };
        case stereofx::monoMaker:       return { "Clean", "Steep", "Audition" };
        case stereofx::correlation:     return { "Monitor", "Guard", "Utility" };
        default:                        return { "Clean", "Haas Legacy", "Focus" };
    }
}

juce::StringArray MusiqueStereoProcessor::getAllParameterIds()
{
    return {
        "engine", "variant", "width", "balance", "mid_gain", "side_gain", "bass_mono", "haas", "mix", "output", "bypass", "mono",
        "wide_focus", "wide_safety",
        "haas_time", "haas_feedback", "haas_tone", "haas_side",
        "img_low", "img_mid", "img_high", "img_xover1", "img_xover2",
        "spatial_depth", "spatial_angle", "spatial_air", "spatial_focus",
        "mono_freq", "mono_strength", "mono_slope", "mono_audition",
        "corr_hold", "corr_decay", "corr_zoom", "corr_warn"
    };
}

MusiqueStereoProcessor::MusiqueStereoProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "MusiqueStereo", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MusiqueStereoProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterChoice>("engine", "Engine", getEngineNames(), 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("variant", "Variant", juce::StringArray { "Clean", "Haas Legacy", "Focus" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("width", "Width", juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("balance", "Balance", -100.0f, 100.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mid_gain", "Mid Gain", -24.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("side_gain", "Side Gain", -24.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bass_mono", "Bass Mono", juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.35f), 120.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("haas", "Haas", juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", percentRange(), 100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("output", "Output", -24.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("mono", "Mono", false));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("wide_focus", "Wide Focus", percentRange(), 50.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wide_safety", "Wide Safety", percentRange(), 50.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("haas_time", "Haas Time", 0.0f, 30.0f, 8.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("haas_feedback", "Haas Feedback", percentRange(), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("haas_tone", "Haas Tone", percentRange(), 55.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("haas_side", "Haas Side", percentRange(), 70.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("img_low", "Low Width", 0.0f, 200.0f, 85.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("img_mid", "Mid Width", 0.0f, 200.0f, 110.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("img_high", "High Width", 0.0f, 200.0f, 125.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("img_xover1", "Low Xover", juce::NormalisableRange<float>(60.0f, 600.0f, 1.0f, 0.45f), 180.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("img_xover2", "High Xover", juce::NormalisableRange<float>(800.0f, 8000.0f, 1.0f, 0.45f), 3200.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("spatial_depth", "Spatial Depth", percentRange(), 35.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("spatial_angle", "Spatial Angle", -100.0f, 100.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("spatial_air", "Spatial Air", percentRange(), 35.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("spatial_focus", "Spatial Focus", percentRange(), 50.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mono_freq", "Mono Freq", juce::NormalisableRange<float>(30.0f, 500.0f, 1.0f, 0.45f), 140.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mono_strength", "Mono Strength", percentRange(), 80.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mono_slope", "Mono Slope", percentRange(), 50.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mono_audition", "Mono Audition", 0.0f, 2.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("corr_hold", "Correlation Hold", percentRange(), 40.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("corr_decay", "Correlation Decay", percentRange(), 45.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("corr_zoom", "Correlation Zoom", percentRange(), 50.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("corr_warn", "Correlation Warn", percentRange(), 35.0f));
    return { p.begin(), p.end() };
}

void MusiqueStereoProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = sampleRate;
    haasBuffer.assign((size_t) juce::jmax(4, (int) std::ceil(sampleRate * 0.05)), 0.0f);

    for (auto* smoother : { &widthSmoothed, &balanceSmoothed, &midGainSmoothed, &sideGainSmoothed, &bassMonoSmoothed, &haasSmoothed, &mixSmoothed, &outputSmoothed })
        smoother->reset(sampleRate, 0.025);

    widthSmoothed.setCurrentAndTargetValue(raw(parameters, "width", 100.0f) / 100.0f);
    balanceSmoothed.setCurrentAndTargetValue(raw(parameters, "balance") / 100.0f);
    midGainSmoothed.setCurrentAndTargetValue(raw(parameters, "mid_gain"));
    sideGainSmoothed.setCurrentAndTargetValue(raw(parameters, "side_gain"));
    bassMonoSmoothed.setCurrentAndTargetValue(raw(parameters, "bass_mono", 120.0f));
    haasSmoothed.setCurrentAndTargetValue((float) (sampleRate * raw(parameters, "haas") / 1000.0f));
    mixSmoothed.setCurrentAndTargetValue(raw(parameters, "mix", 100.0f) / 100.0f);
    outputSmoothed.setCurrentAndTargetValue(stereofx::dbToGain(raw(parameters, "output")));

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax(1, samplesPerBlock), 1 };
    for (auto* filter : { &bassLowL, &bassLowR, &bassHighL, &bassHighR })
    {
        filter->prepare(spec);
        filter->reset();
        filter->setResonance(0.7071f);
    }
    bassLowL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    bassLowR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    bassHighL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    bassHighR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    resetRuntime();
}

void MusiqueStereoProcessor::releaseResources()
{
    haasBuffer.clear();
    resetRuntime();
    for (auto* filter : { &bassLowL, &bassLowR, &bassHighL, &bassHighR })
        filter->reset();
}

void MusiqueStereoProcessor::resetRuntime()
{
    haasWrite = 0;
    haasFeedbackL = 0.0f;
    haasFeedbackR = 0.0f;
}

bool MusiqueStereoProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

juce::AudioProcessorParameter* MusiqueStereoProcessor::getBypassParameter() const
{
    return parameters.getParameter("bypass");
}

float MusiqueStereoProcessor::readHaasSample(float delaySamples) const noexcept
{
    const int bufferSize = (int) haasBuffer.size();
    if (bufferSize <= 2)
        return 0.0f;

    float readPos = (float) haasWrite - juce::jlimit(0.0f, (float) bufferSize - 2.0f, delaySamples);
    while (readPos < 0.0f)
        readPos += (float) bufferSize;

    const int indexA = ((int) std::floor(readPos)) % bufferSize;
    const int indexB = (indexA + 1) % bufferSize;
    const float frac = readPos - (float) indexA;
    return juce::jmap(frac, haasBuffer[(size_t) indexA], haasBuffer[(size_t) indexB]);
}

stereofx::Params MusiqueStereoProcessor::readParams() const
{
    stereofx::Params p;
    p.engine = juce::jlimit(0, stereofx::numEngines - 1, (int) std::round(raw(parameters, "engine")));
    p.variant = juce::jlimit(0, 2, (int) std::round(raw(parameters, "variant")));
    p.bypass = raw(parameters, "bypass") > 0.5f;
    p.mono = raw(parameters, "mono") > 0.5f;
    p.width = raw(parameters, "width", 100.0f);
    p.balance = raw(parameters, "balance");
    p.midGainDb = raw(parameters, "mid_gain");
    p.sideGainDb = raw(parameters, "side_gain");
    p.bassMonoHz = raw(parameters, "bass_mono", 120.0f);
    p.haasMs = raw(parameters, "haas");
    p.mix = raw(parameters, "mix", 100.0f);
    p.outputDb = raw(parameters, "output");
    p.wideFocus = raw(parameters, "wide_focus", 50.0f);
    p.wideSafety = raw(parameters, "wide_safety", 50.0f);
    p.haasTime = raw(parameters, "haas_time", 8.0f);
    p.haasFeedback = raw(parameters, "haas_feedback");
    p.haasTone = raw(parameters, "haas_tone", 55.0f);
    p.haasSide = raw(parameters, "haas_side", 70.0f);
    p.imgLow = raw(parameters, "img_low", 85.0f);
    p.imgMid = raw(parameters, "img_mid", 110.0f);
    p.imgHigh = raw(parameters, "img_high", 125.0f);
    p.imgXover1 = raw(parameters, "img_xover1", 180.0f);
    p.imgXover2 = raw(parameters, "img_xover2", 3200.0f);
    p.spatialDepth = raw(parameters, "spatial_depth", 35.0f);
    p.spatialAngle = raw(parameters, "spatial_angle");
    p.spatialAir = raw(parameters, "spatial_air", 35.0f);
    p.spatialFocus = raw(parameters, "spatial_focus", 50.0f);
    p.monoFreq = raw(parameters, "mono_freq", 140.0f);
    p.monoStrength = raw(parameters, "mono_strength", 80.0f);
    p.monoSlope = raw(parameters, "mono_slope", 50.0f);
    p.monoAudition = raw(parameters, "mono_audition");
    p.corrHold = raw(parameters, "corr_hold", 40.0f);
    p.corrDecay = raw(parameters, "corr_decay", 45.0f);
    p.corrZoom = raw(parameters, "corr_zoom", 50.0f);
    p.corrWarn = raw(parameters, "corr_warn", 35.0f);
    return p;
}

void MusiqueStereoProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    visualState.captureInput(b);
    visualState.captureOutput(b);
    stereoFieldState.capture(b);
    const auto field = stereoFieldState.getSnapshot();
    stereoSnapshot = { field.correlation, field.width, field.balance, 0.0f, field.correlation < 0.0f ? -field.correlation : 0.0f, 0, 0 };
}

void MusiqueStereoProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    visualState.captureInput(b);

    const int channels = b.getNumChannels();
    const int samples = b.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return;

    auto p = readParams();
    if (p.bypass)
    {
        processBlockBypassed(b, midi);
        return;
    }

    const bool stereo = channels > 1;
    const float sampleRate = juce::jmax(1.0f, (float) preparedSampleRate);
    const float safeX1 = juce::jlimit(30.0f, sampleRate * 0.35f, p.imgXover1);
    const float safeX2 = juce::jlimit(safeX1 + 200.0f, sampleRate * 0.45f, p.imgXover2);
    const float bassFreq = p.engine == stereofx::monoMaker ? p.monoFreq : p.bassMonoHz;
    const float safeBass = juce::jlimit(30.0f, juce::jmin(500.0f, sampleRate * 0.45f), bassFreq);
    const float mixTarget = stereofx::percent(p.mix);
    const float outputTarget = stereofx::dbToGain(p.outputDb);
    mixSmoothed.setTargetValue(mixTarget);
    outputSmoothed.setTargetValue(outputTarget);

    bassLowL.setCutoffFrequency(safeBass);
    bassLowR.setCutoffFrequency(safeBass);
    bassHighL.setCutoffFrequency(safeBass);
    bassHighR.setCutoffFrequency(safeBass);

    double midEnergy = 0.0;
    double sideEnergy = 0.0;
    double lowSideRemoved = 0.0;

    for (int i = 0; i < samples; ++i)
    {
        float left = b.getSample(0, i);
        float right = stereo ? b.getSample(1, i) : left;
        if (p.mono)
            left = right = 0.5f * (left + right);

        const float dryL = left;
        const float dryR = right;
        float mid = 0.0f;
        float side = 0.0f;
        stereofx::encodeMS(left, right, mid, side);

        if (p.engine == stereofx::widener)
        {
            const float width = juce::jlimit(0.0f, 2.0f, p.width / 100.0f);
            const float safety = 1.0f - stereofx::percent(p.wideSafety) * 0.25f;
            mid *= stereofx::dbToGain(p.midGainDb);
            side *= stereofx::dbToGain(p.sideGainDb) * width * safety;
        }
        else if (p.engine == stereofx::haas)
        {
            const float haasMs = p.haasTime > 0.01f ? p.haasTime : p.haasMs;
            const float haasSamples = juce::jlimit(0.0f, sampleRate * 0.045f, haasMs * sampleRate / 1000.0f);
            const float delayed = readHaasSample(haasSamples);
            const float feedback = stereofx::percent(p.haasFeedback) * 0.35f;
            haasBuffer[(size_t) haasWrite] = side + (haasFeedbackL + haasFeedbackR) * 0.5f * feedback;
            haasWrite = (haasWrite + 1) % (int) haasBuffer.size();
            side = side * (1.0f - stereofx::percent(p.haasSide)) + delayed * stereofx::percent(p.haasSide);
            const float tone = 0.7f + stereofx::percent(p.haasTone) * 0.6f;
            side *= tone;
            haasFeedbackL = side;
            haasFeedbackR = -side;
        }
        else if (p.engine == stereofx::frequencyImager)
        {
            const float lowScale = p.imgLow / 100.0f;
            const float midScale = p.imgMid / 100.0f;
            const float highScale = p.imgHigh / 100.0f;
            const float freqProxy = std::abs(side - mid) * sampleRate * 0.02f;
            const float scale = freqProxy < safeX1 ? lowScale : (freqProxy < safeX2 ? midScale : highScale);
            side *= juce::jlimit(0.0f, 2.0f, scale);
        }
        else if (p.engine == stereofx::spatial)
        {
            const float depth = stereofx::percent(p.spatialDepth);
            const float angle = p.spatialAngle / 100.0f;
            const float air = 1.0f + stereofx::percent(p.spatialAir) * 0.35f;
            const float focus = 0.75f + stereofx::percent(p.spatialFocus) * 0.5f;
            side = side * (1.0f + depth * 0.8f) * air + mid * angle * 0.18f;
            mid *= focus;
        }
        else if (p.engine == stereofx::monoMaker)
        {
            const float lowL = bassLowL.processSample(0, left);
            const float lowR = bassLowR.processSample(0, right);
            const float highL = bassHighL.processSample(0, left);
            const float highR = bassHighR.processSample(0, right);
            const float lowMid = 0.5f * (lowL + lowR);
            const float strength = stereofx::percent(p.monoStrength);
            const float lowSide = 0.5f * (lowL - lowR);
            lowSideRemoved += std::abs(lowSide) * strength;
            left = highL + juce::jmap(strength, lowL, lowMid);
            right = highR + juce::jmap(strength, lowR, lowMid);
            if (p.monoAudition > 1.5f)
                left = right = lowMid;
            else if (p.monoAudition > 0.5f)
                left = right = highL - highR;
            stereofx::encodeMS(left, right, mid, side);
        }
        else if (p.engine == stereofx::correlation)
        {
            const float warn = stereofx::percent(p.corrWarn);
            if (warn > 0.01f && std::abs(side) > std::abs(mid))
                side *= 1.0f - warn * 0.35f;
            mid *= 1.0f + stereofx::percent(p.corrZoom) * 0.04f;
        }

        if (p.engine != stereofx::monoMaker)
        {
            const float bal = juce::jlimit(-1.0f, 1.0f, p.balance / 100.0f);
            stereofx::decodeMS(mid, side, left, right);
            const float balL = bal > 0.0f ? std::cos(bal * juce::MathConstants<float>::halfPi) : 1.0f;
            const float balR = bal < 0.0f ? std::cos(-bal * juce::MathConstants<float>::halfPi) : 1.0f;
            left *= balL;
            right *= balR;
        }
        else
        {
            stereofx::decodeMS(mid, side, left, right);
        }

        const float mix = mixSmoothed.getNextValue();
        const float out = outputSmoothed.getNextValue();
        left = (dryL * (1.0f - mix) + left * mix) * out;
        right = (dryR * (1.0f - mix) + right * mix) * out;
        b.setSample(0, i, left);
        if (stereo)
            b.setSample(1, i, right);

        float outMid = 0.0f, outSide = 0.0f;
        stereofx::encodeMS(left, right, outMid, outSide);
        midEnergy += (double) outMid * outMid;
        sideEnergy += (double) outSide * outSide;
    }

    visualState.captureOutput(b);
    stereoFieldState.capture(b);
    const auto field = stereoFieldState.getSnapshot();
    stereoSnapshot = {
        field.correlation,
        stereofx::safeWidth((float) midEnergy, (float) sideEnergy),
        field.balance,
        (float) (lowSideRemoved / (double) juce::jmax(1, samples)),
        field.correlation < 0.0f ? -field.correlation : 0.0f,
        p.engine,
        p.variant
    };
}

void MusiqueStereoProcessor::getStateInformation(juce::MemoryBlock& d)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, d);
}

void MusiqueStereoProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(parameters.state.getType()))
    {
        const auto incoming = juce::ValueTree::fromXml(*xml);
        const bool hasEngine = stateHasParameter(incoming, "engine");
        const bool hasVariant = stateHasParameter(incoming, "variant");
        parameters.replaceState(incoming);
        if (!hasEngine)
            setParam(parameters, "engine", 0.0f);
        if (!hasVariant && raw(parameters, "engine") < 0.5f)
            setParam(parameters, "variant", raw(parameters, "haas") > 0.05f ? 1.0f : 0.0f);
        resetRuntime();
    }
}

void MusiqueStereoProcessor::applyPresetCompat(const juce::var& preset)
{
    const bool hasEngine = presetHas(preset, "engine");
    const bool hasVariant = presetHas(preset, "variant");
    fx::preset::applyToAPVTS(parameters, preset);
    if (!hasEngine)
        setParam(parameters, "engine", 0.0f);
    if (!hasVariant && raw(parameters, "engine") < 0.5f)
        setParam(parameters, "variant", raw(parameters, "haas") > 0.05f ? 1.0f : 0.0f);
    resetRuntime();
}

juce::AudioProcessorEditor* MusiqueStereoProcessor::createEditor()
{
#if MUSIQUE_STEREO_DSP_TESTS
    return nullptr;
#else
    return new MusiqueStereoEditor(*this);
#endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MusiqueStereoProcessor(); }
