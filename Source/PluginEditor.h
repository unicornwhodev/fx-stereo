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

    void timerCallback() override;
    void paintVisualization(juce::Graphics&, juce::Rectangle<int> area);

    MusiqueStereoProcessor& proc;
    fx::FXLookAndFeel lnf { fx::accent::pitch };

    // Header
    juce::Label titleLabel;
    juce::Image pluginIcon, logoImg;
    juce::TextButton bypassBtn{"Bypass"}, monoBtn{"Mono"}, osBtn{"OS"}, settingsBtn{juce::CharPointer_UTF8("\xe2\x9a\x99")};

    // Preset bar
    juce::TextButton prevBtn{"<"}, nextBtn{">"}, saveBtn{"Save"}, abBtn{"A/B"};
    juce::ComboBox presetBox;
    std::shared_ptr<juce::Array<juce::var>> presets;

    // 6 knobs: Width, Balance, Mid Gain, Side Gain, Bass Mono, Haas
    juce::Slider knobs[6];
    juce::Label knobLabels[6];

    // Footer
    fx::MeterComponent inMeter, outMeter;
    juce::Slider mixSlider, outputSlider;
    juce::Label versionLabel;
    fx::LEDComponent activeLED;

    // Visualization state
    float phase = 0.0f;

    // Attachments
    std::unique_ptr<SliderAttach> widthAtt, balAtt, midAtt, sideAtt, bassAtt, haasAtt, mixAtt, outAtt;
    std::unique_ptr<ButtonAttach> bypassAtt, monoAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusiqueStereoEditor)
};
