#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FXTokens.h"
#include "FXLookAndFeel.h"
#include "FXComponents.h"

class MusiqueStereoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MusiqueStereoEditor(MusiqueStereoProcessor&);
    ~MusiqueStereoEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttach = APVTS::SliderAttachment;
    using ButtonAttach = APVTS::ButtonAttachment;
    using ComboAttach = APVTS::ComboBoxAttachment;

    void timerCallback() override;
    void paintVisualization(juce::Graphics&, juce::Rectangle<int> area);
    void refreshPresetBox();
    void refreshEngineUi();
    void attachKnobsForEngine(int engine);
    void setupSlider(juce::Slider&, juce::Label&, const juce::String&);
    void storeCurrentABSlot();
    void recallABSlot(bool slotA);

    MusiqueStereoProcessor& proc;
    fx::FXLookAndFeel lnf { fx::accent::pitch };

    // Header
    juce::Label titleLabel;
    juce::Image pluginIcon, logoImg;
    juce::TextButton bypassBtn{"Bypass"}, monoBtn{"Mono"}, statusBtn{"MONO OK"}, actionBtn{"MONITOR"};

    // Preset bar
    juce::TextButton prevBtn{"<"}, nextBtn{">"}, saveBtn{"Save"}, abBtn{"A/B"};
    juce::ComboBox presetBox, engineBox, variantBox;
    std::shared_ptr<juce::Array<juce::var>> presets;

    static constexpr int numKnobs = 6;
    juce::Slider knobs[numKnobs];
    juce::Label knobLabels[numKnobs];

    // Footer
    fx::MeterComponent inMeter, outMeter;
    juce::Slider mixSlider, outputSlider;
    juce::Label versionLabel;
    fx::LEDComponent activeLED;

    // Visualization state
    float phase = 0.0f;

    // Attachments
    std::array<std::unique_ptr<SliderAttach>, numKnobs> knobAtts;
    std::unique_ptr<SliderAttach> outAtt;
    std::unique_ptr<ButtonAttach> bypassAtt, monoAtt;
    std::unique_ptr<ComboAttach> engineAtt;
    juce::ValueTree abStateA, abStateB;
    bool showingA = true;
    int lastEngine = -1;
    int lastVariant = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusiqueStereoEditor)
};
