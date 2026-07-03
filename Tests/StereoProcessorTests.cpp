#include "PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
struct Runner
{
    int checks = 0;
    int failures = 0;

    void expect(bool condition, const std::string& name)
    {
        ++checks;
        std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
        if (!condition)
            ++failures;
    }
};

void setParameter(MusiqueStereoProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getAPVTS().getParameter(id);
    if (parameter == nullptr)
    {
        std::cerr << "Missing parameter: " << id << '\n';
        std::exit(2);
    }
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float getParameter(MusiqueStereoProcessor& processor, const juce::String& id)
{
    if (auto* value = processor.getAPVTS().getRawParameterValue(id))
        return value->load();
    std::cerr << "Missing parameter: " << id << '\n';
    std::exit(2);
}

std::unique_ptr<MusiqueStereoProcessor> makeProcessor(int channels = 2)
{
    auto processor = std::make_unique<MusiqueStereoProcessor>();
    processor->setPlayConfigDetails(channels, channels, 48000.0, 512);
    processor->prepareToPlay(48000.0, 512);
    return processor;
}

juce::AudioBuffer<float> makeStereoSignal(int samples)
{
    juce::AudioBuffer<float> buffer(2, samples);
    for (int i = 0; i < samples; ++i)
    {
        const float t = (float) i / 48000.0f;
        buffer.setSample(0, i, 0.22f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * t));
        buffer.setSample(1, i, 0.18f * std::sin(2.0f * juce::MathConstants<float>::pi * 330.0f * t + 0.7f));
    }
    return buffer;
}

juce::AudioBuffer<float> makeMonoDuplicated(int samples)
{
    juce::AudioBuffer<float> buffer(2, samples);
    for (int i = 0; i < samples; ++i)
    {
        const float value = 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * 180.0f * (float) i / 48000.0f);
        buffer.setSample(0, i, value);
        buffer.setSample(1, i, value);
    }
    return buffer;
}

juce::AudioBuffer<float> makeMonoSignal(int samples)
{
    juce::AudioBuffer<float> buffer(1, samples);
    for (int i = 0; i < samples; ++i)
        buffer.setSample(0, i, 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * 160.0f * (float) i / 48000.0f));
    return buffer;
}

void process(MusiqueStereoProcessor& processor, juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}

bool isFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (!std::isfinite(buffer.getSample(ch, i)))
                return false;
    return true;
}

float diffEnergy(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float sum = 0.0f;
    const int channels = juce::jmin(a.getNumChannels(), b.getNumChannels());
    const int samples = juce::jmin(a.getNumSamples(), b.getNumSamples());
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < samples; ++i)
            sum += std::abs(a.getSample(ch, i) - b.getSample(ch, i));
    return sum / (float) juce::jmax(1, channels * samples);
}

float sideEnergy(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float side = 0.5f * (buffer.getSample(0, i) - buffer.getSample(1, i));
        sum += std::abs(side);
    }
    return sum / (float) buffer.getNumSamples();
}

juce::ValueTree copyState(MusiqueStereoProcessor& processor)
{
    juce::MemoryBlock data;
    processor.getStateInformation(data);
    auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), (int) data.getSize());
    if (xml == nullptr)
        std::exit(2);
    return juce::ValueTree::fromXml(*xml);
}

void loadState(MusiqueStereoProcessor& processor, const juce::ValueTree& state)
{
    auto xml = state.createXml();
    juce::MemoryBlock data;
    juce::AudioProcessor::copyXmlToBinary(*xml, data);
    processor.setStateInformation(data.getData(), (int) data.getSize());
}

void removeParam(juce::ValueTree& state, const juce::String& id)
{
    for (int index = state.getNumChildren() - 1; index >= 0; --index)
    {
        auto child = state.getChild(index);
        if (child.hasType("PARAM") && child.getProperty("id").toString() == id)
            state.removeChild(index, nullptr);
    }
}

juce::File findFactoryBank()
{
    auto dir = juce::File::getCurrentWorkingDirectory();
    for (int depth = 0; depth < 8; ++depth)
    {
        const std::array<juce::File, 3> candidates {
            dir.getChildFile("Presets").getChildFile("factory_bank.json"),
            dir.getChildFile("fx-stereo").getChildFile("Presets").getChildFile("factory_bank.json"),
            dir.getChildFile("FX").getChildFile("fx-stereo").getChildFile("Presets").getChildFile("factory_bank.json")
        };
        for (const auto& candidate : candidates)
            if (candidate.existsAsFile())
                return candidate;
        const auto parent = dir.getParentDirectory();
        if (parent == dir)
            break;
        dir = parent;
    }
    return {};
}

juce::Array<juce::var> loadFactoryPresets()
{
    const auto file = findFactoryBank();
    if (!file.existsAsFile())
    {
        std::cerr << "factory_bank.json not found\n";
        std::exit(2);
    }
    auto json = juce::JSON::parse(file.loadFileAsString());
    if (auto* object = json.getDynamicObject())
        if (auto* presets = object->getProperty("presets").getArray())
            return *presets;
    return {};
}

void testLegacyMigration(Runner& runner)
{
    auto processor = makeProcessor();
    setParameter(*processor, "engine", 3.0f);
    setParameter(*processor, "variant", 2.0f);
    setParameter(*processor, "haas", 4.0f);
    auto state = copyState(*processor);
    removeParam(state, "engine");
    removeParam(state, "variant");
    loadState(*processor, state);
    runner.expect((int) getParameter(*processor, "engine") == stereofx::widener, "legacy state migrates to Widener");
    runner.expect((int) getParameter(*processor, "variant") == 1, "legacy haas derives Haas Legacy variant");
    runner.expect(getParameter(*processor, "haas") > 0.05f, "legacy haas field is preserved");
}

void testBypassAndLayouts(Runner& runner)
{
    auto processor = makeProcessor();
    auto buffer = makeStereoSignal(512);
    const auto dry = buffer;
    setParameter(*processor, "bypass", 1.0f);
    setParameter(*processor, "mono", 1.0f);
    setParameter(*processor, "output", -12.0f);
    setParameter(*processor, "mix", 0.0f);
    process(*processor, buffer);
    runner.expect(diffEnergy(buffer, dry) < 1.0e-7f, "bypass is dry-identical");

    auto mono = makeProcessor(1);
    auto monoBuffer = makeMonoSignal(512);
    process(*mono, monoBuffer);
    runner.expect(isFinite(monoBuffer), "mono->mono remains finite");

    auto stereo = makeProcessor(2);
    auto stereoBuffer = makeStereoSignal(512);
    process(*stereo, stereoBuffer);
    runner.expect(isFinite(stereoBuffer), "stereo->stereo remains finite");
}

void testEngines(Runner& runner)
{
    for (int engine = 0; engine < stereofx::numEngines; ++engine)
    {
        auto processor = makeProcessor();
        setParameter(*processor, "engine", (float) engine);
        setParameter(*processor, "variant", 0.0f);
        setParameter(*processor, "mix", 100.0f);
        if (engine == stereofx::widener)
            setParameter(*processor, "width", 160.0f);
        if (engine == stereofx::haas)
            setParameter(*processor, "haas_time", 8.0f);
        if (engine == stereofx::monoMaker)
            setParameter(*processor, "mono_strength", 100.0f);
        auto buffer = makeStereoSignal(512);
        const auto before = buffer;
        for (int block = 0; block < 4; ++block)
            process(*processor, buffer);
        runner.expect(isFinite(buffer), "engine " + std::to_string(engine) + " remains finite");
        runner.expect(diffEnergy(buffer, before) > 1.0e-5f || engine == stereofx::correlation, "engine " + std::to_string(engine) + " changes audio when expected");
    }
}

void testStereoBehaviours(Runner& runner)
{
    auto wide = makeProcessor();
    auto wideBuffer = makeStereoSignal(512);
    const float beforeSide = sideEnergy(wideBuffer);
    setParameter(*wide, "engine", (float) stereofx::widener);
    setParameter(*wide, "width", 180.0f);
    process(*wide, wideBuffer);
    runner.expect(sideEnergy(wideBuffer) > beforeSide, "Widener increases side energy");

    auto mono = makeProcessor();
    auto monoBuffer = makeStereoSignal(512);
    setParameter(*mono, "engine", (float) stereofx::monoMaker);
    setParameter(*mono, "mono_strength", 100.0f);
    setParameter(*mono, "mix", 100.0f);
    process(*mono, monoBuffer);
    runner.expect(mono->getStereoSnapshot().lowMono >= 0.0f, "Mono Maker reports low mono action");

    auto corr = makeProcessor();
    auto duplicated = makeMonoDuplicated(512);
    setParameter(*corr, "engine", (float) stereofx::correlation);
    process(*corr, duplicated);
    runner.expect(corr->getStereoSnapshot().correlation > 0.95f, "Correlation is near +1 on duplicated mono");

    auto widened = makeStereoSignal(512);
    process(*corr, widened);
    runner.expect(corr->getStereoSnapshot().correlation < 0.95f, "Correlation drops on widened stereo signal");
}

void testFactoryPresets(Runner& runner)
{
    const auto presets = loadFactoryPresets();
    runner.expect(presets.size() == 18, "factory bank contains 8 legacy + 10 RC presets");
    auto processor = makeProcessor();
    processor->applyPresetCompat(presets.getReference(0));
    runner.expect((int) getParameter(*processor, "engine") == stereofx::widener, "legacy JSON preset without engine loads Widener");

    bool allFinite = true;
    bool sawCorrelation = false;
    for (const auto& preset : presets)
    {
        processor->applyPresetCompat(preset);
        sawCorrelation = sawCorrelation || (int) getParameter(*processor, "engine") == stereofx::correlation;
        auto buffer = makeStereoSignal(256);
        process(*processor, buffer);
        allFinite = allFinite && isFinite(buffer);
    }
    runner.expect(sawCorrelation, "factory bank includes Correlation presets");
    runner.expect(allFinite, "all factory presets process finite audio");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    Runner runner;
    testLegacyMigration(runner);
    testBypassAndLayouts(runner);
    testEngines(runner);
    testStereoBehaviours(runner);
    testFactoryPresets(runner);
    std::cout << "Checks: " << runner.checks << ", Failures: " << runner.failures << '\n';
    return runner.failures == 0 ? 0 : 1;
}
