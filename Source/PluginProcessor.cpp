#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace { static float dbToGain(float dB) { return std::pow(10.0f, dB / 20.0f); } }

MusiqueStereoProcessor::MusiqueStereoProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "MusiqueStereo", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout MusiqueStereoProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>("width", "Width",
        juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("balance", "Balance", -100.0f, 100.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mid_gain", "Mid Gain", -24.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("side_gain", "Side Gain", -24.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bass_mono", "Bass Mono",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.35f), 120.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("haas", "Haas",
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 100.0f, 100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("output", "Output", -24.0f, 12.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("mono", "Mono", false));
    return { p.begin(), p.end() };
}

void MusiqueStereoProcessor::prepareToPlay(double sampleRate, int)
{
    preparedSampleRate = sampleRate;
    const int maxHaasSamples = (int)(sampleRate * 0.035); // 35ms headroom
    haasBuffer.assign((size_t)maxHaasSamples, 0.0f);
    haasWrite = 0;

    widthSmoothed.reset(sampleRate, 0.025);
    balanceSmoothed.reset(sampleRate, 0.025);
    midGainSmoothed.reset(sampleRate, 0.03);
    sideGainSmoothed.reset(sampleRate, 0.03);
    bassMonoSmoothed.reset(sampleRate, 0.04);
    haasSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.025);

    widthSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("width")->load() / 100.0f);
    balanceSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("balance")->load() / 100.0f);
    midGainSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("mid_gain")->load());
    sideGainSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("side_gain")->load());
    bassMonoSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("bass_mono")->load());
    haasSmoothed.setCurrentAndTargetValue((float) (sampleRate * parameters.getRawParameterValue("haas")->load() / 1000.0f));
    mixSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("mix")->load() / 100.0f);

    juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
    bassLowL.prepare(spec); bassLowR.prepare(spec); bassHighL.prepare(spec); bassHighR.prepare(spec);
    bassLowL.reset(); bassLowR.reset(); bassHighL.reset(); bassHighR.reset();
    bassLowL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    bassLowR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    bassHighL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    bassHighR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    bassLowL.setResonance(0.7071f); bassLowR.setResonance(0.7071f);
    bassHighL.setResonance(0.7071f); bassHighR.setResonance(0.7071f);
}

void MusiqueStereoProcessor::releaseResources()
{
    haasBuffer.clear();
    haasWrite = 0;
    bassLowL.reset();
    bassLowR.reset();
    bassHighL.reset();
    bassHighR.reset();
}

float MusiqueStereoProcessor::readHaasSample(float delaySamples) const noexcept
{
    const int bufferSize = (int) haasBuffer.size();
    if (bufferSize <= 1)
        return 0.0f;

    float readPos = (float) haasWrite - juce::jlimit(0.0f, (float) bufferSize - 2.0f, delaySamples);
    while (readPos < 0.0f)
        readPos += (float) bufferSize;

    const int indexA = ((int) std::floor(readPos)) % bufferSize;
    const int indexB = (indexA + 1) % bufferSize;
    const float frac = readPos - (float) indexA;
    return juce::jmap(frac, haasBuffer[(size_t) indexA], haasBuffer[(size_t) indexB]);
}

bool MusiqueStereoProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MusiqueStereoProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    visualState.captureInput(b);
    const int numSamples = b.getNumSamples();
    if (numSamples <= 0)
        return;

    const bool monoFlag = parameters.getRawParameterValue("mono")->load() > 0.5f;
    if (monoFlag)
        for (int i = 0; i < numSamples; ++i)
        {
            float m = 0.5f * (b.getSample(0, i) + b.getSample(1, i));
            b.setSample(0, i, m); b.setSample(1, i, m);
        }

    if (parameters.getRawParameterValue("bypass")->load() > 0.5f)
    {
        b.applyGain(dbToGain(parameters.getRawParameterValue("output")->load()));
        visualState.captureOutput(b);
        stereoFieldState.capture(b);
        return;
    }

    const float out      = dbToGain(parameters.getRawParameterValue("output")->load());

    widthSmoothed.setTargetValue(parameters.getRawParameterValue("width")->load() / 100.0f);
    balanceSmoothed.setTargetValue(parameters.getRawParameterValue("balance")->load() / 100.0f);
    midGainSmoothed.setTargetValue(parameters.getRawParameterValue("mid_gain")->load());
    sideGainSmoothed.setTargetValue(parameters.getRawParameterValue("side_gain")->load());
    bassMonoSmoothed.setTargetValue(parameters.getRawParameterValue("bass_mono")->load());
    haasSmoothed.setTargetValue((float) (preparedSampleRate * parameters.getRawParameterValue("haas")->load() / 1000.0f));
    mixSmoothed.setTargetValue(parameters.getRawParameterValue("mix")->load() / 100.0f);
    const float safeSampleRate = juce::jmax(1.0f, (float) preparedSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        float l = b.getSample(0, i);
        float r = b.getSample(1, i);
        float dryL = l, dryR = r;

        const float width = juce::jlimit(0.0f, 2.0f, widthSmoothed.getNextValue());
        const float balance = juce::jlimit(-1.0f, 1.0f, balanceSmoothed.getNextValue());
        const float midGain = dbToGain(midGainSmoothed.getNextValue());
        const float sideGain = dbToGain(sideGainSmoothed.getNextValue());
        const float bassFreq = juce::jlimit(30.0f, juce::jmin(500.0f, safeSampleRate * 0.45f), bassMonoSmoothed.getNextValue());
        const float haasSamples = haasSmoothed.getNextValue();
        const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());

        bassLowL.setCutoffFrequency(bassFreq);
        bassLowR.setCutoffFrequency(bassFreq);
        bassHighL.setCutoffFrequency(bassFreq);
        bassHighR.setCutoffFrequency(bassFreq);

        // Mid-Side encode
        float mid  = 0.5f * (l + r);
        float side = 0.5f * (l - r);

        // Apply width (side scaling) + individual gains
        mid  *= midGain;
        side *= sideGain * width;

        // Mid-Side decode
        l = mid + side;
        r = mid - side;

        // Bass mono: complementary split to keep crossover cleaner
        const float bassL = bassLowL.processSample(0, l);
        const float bassR = bassLowR.processSample(0, r);
        const float highL = bassHighL.processSample(0, l);
        const float highR = bassHighR.processSample(0, r);
        float bassMono = 0.5f * (bassL + bassR);
        l = highL + bassMono;
        r = highR + bassMono;

        // Haas effect on the side component preserves mono sum more gracefully.
        if (!haasBuffer.empty())
        {
            const float postMid = 0.5f * (l + r);
            const float postSide = 0.5f * (l - r);
            float delayedSide = postSide;
            if (haasSamples > 0.01f)
                delayedSide = readHaasSample(haasSamples);
            haasBuffer[(size_t) haasWrite] = postSide;
            haasWrite = (haasWrite + 1) % (int) haasBuffer.size();
            l = postMid + delayedSide;
            r = postMid - delayedSide;
        }

        // Balance law: attenuate the opposite side with an equal-power curve.
        const float balL = balance > 0.0f ? std::cos(balance * juce::MathConstants<float>::halfPi) : 1.0f;
        const float balR = balance < 0.0f ? std::cos(-balance * juce::MathConstants<float>::halfPi) : 1.0f;
        l *= balL;
        r *= balR;

        // Mix dry/wet
        l = dryL * (1.0f - mix) + l * mix;
        r = dryR * (1.0f - mix) + r * mix;

        b.setSample(0, i, l * out);
        b.setSample(1, i, r * out);
    }

    visualState.captureOutput(b);
    stereoFieldState.capture(b);
}

void MusiqueStereoProcessor::getStateInformation(juce::MemoryBlock& d)
{
    auto s = parameters.copyState();
    std::unique_ptr<juce::XmlElement> x(s.createXml());
    copyXmlToBinary(*x, d);
}

void MusiqueStereoProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> x(getXmlFromBinary(data, size));
    if (x && x->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*x));
}

juce::AudioProcessorEditor* MusiqueStereoProcessor::createEditor() { return new MusiqueStereoEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MusiqueStereoProcessor(); }
