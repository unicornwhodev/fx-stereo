#pragma once
#include <JuceHeader.h>
#include "FXAudioVisualState.h"
#include "StereoEngines.h"

class MusiqueStereoProcessor : public juce::AudioProcessor
{
public:
    MusiqueStereoProcessor();
    ~MusiqueStereoProcessor() override = default;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override
    {
#ifdef JucePlugin_Name
        return JucePlugin_Name;
#else
        return "Musique Stereo";
#endif
    }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return parameters; }
    const fx::AudioVisualState& getVisualState() const noexcept { return visualState; }
    const fx::StereoFieldState& getStereoFieldState() const noexcept { return stereoFieldState; }
    stereofx::Snapshot getStereoSnapshot() const noexcept { return stereoSnapshot; }
    static juce::StringArray getEngineNames();
    static juce::StringArray getVariantNames(int engine);
    static juce::StringArray getAllParameterIds();
    void applyPresetCompat(const juce::var& preset);

private:
    juce::AudioProcessorValueTreeState parameters;
    fx::AudioVisualState visualState;
    fx::StereoFieldState stereoFieldState;
    stereofx::Snapshot stereoSnapshot;
    juce::dsp::StateVariableTPTFilter<float> bassLowL, bassLowR, bassHighL, bassHighR;
    juce::SmoothedValue<float> widthSmoothed, balanceSmoothed, midGainSmoothed, sideGainSmoothed, bassMonoSmoothed, haasSmoothed, mixSmoothed;
    juce::SmoothedValue<float> outputSmoothed;
    double preparedSampleRate = 44100.0;
    std::vector<float> haasBuffer;
    int haasWrite = 0;
    float haasFeedbackL = 0.0f;
    float haasFeedbackR = 0.0f;
    float readHaasSample(float delaySamples) const noexcept;
    stereofx::Params readParams() const;
    void resetRuntime();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusiqueStereoProcessor)
};
