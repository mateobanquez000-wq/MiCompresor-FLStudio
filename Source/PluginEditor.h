#pragma once
#include "PluginProcessor.h"

class CompressorEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit CompressorEditor (CompressorProcessor&);
    ~CompressorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (Knob& knob, const juce::String& paramId,
                  const juce::String& name, const juce::String& suffix);

    CompressorProcessor& proc;
    Knob threshold, ratio, attack, release, knee, makeup, mix;

    float displayedGrDb = 0.0f;
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorEditor)
};
