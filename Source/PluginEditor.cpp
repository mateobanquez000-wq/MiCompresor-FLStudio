#include "PluginEditor.h"

namespace
{
    const juce::Colour colBgTop   (0xff1b1e22);
    const juce::Colour colBgBot   (0xff111315);
    const juce::Colour colPanel   (0xff23272c);
    const juce::Colour colAccent  (0xffff7a1a);
    const juce::Colour colText    (0xffe8e6e1);
    const juce::Colour colMuted   (0xff8d8a85);
    const juce::Colour colTrack   (0xff3a3f45);

    juce::Font makeFont (float size, bool bold = false)
    {
        auto opts = juce::FontOptions (size);
        if (bold) opts = opts.withStyle ("Bold");
        return juce::Font (opts);
    }
}

CompressorEditor::CompressorEditor (CompressorProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    addKnob (threshold, "threshold", "THRESHOLD", " dB");
    addKnob (ratio,     "ratio",     "RATIO",     ":1");
    addKnob (attack,    "attack",    "ATTACK",    " ms");
    addKnob (release,   "release",   "RELEASE",   " ms");
    addKnob (knee,      "knee",      "KNEE",      " dB");
    addKnob (makeup,    "makeup",    "MAKEUP",    " dB");
    addKnob (mix,       "mix",       "MIX",       " %");

    setSize (780, 320);
    startTimerHz (30);
}

void CompressorEditor::addKnob (Knob& k, const juce::String& paramId,
                                const juce::String& name, const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 20);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, colAccent);
    k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, colTrack);
    k.slider.setColour (juce::Slider::thumbColourId, colText);
    k.slider.setColour (juce::Slider::textBoxTextColourId, colText);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, colMuted);
    k.label.setFont (makeFont (12.0f, true));

    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, k.slider);
}

void CompressorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colBgTop, 0.0f, 0.0f,
                                             colBgBot, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Cabecera
    g.setColour (colText);
    g.setFont (makeFont (22.0f, true));
    g.drawText ("MI COMPRESOR", 24, 14, 300, 28, juce::Justification::centredLeft);

    g.setColour (colMuted);
    g.setFont (makeFont (12.0f));
    g.drawText ("Compresor feed-forward con soft knee y mezcla paralela",
                24, 40, 400, 18, juce::Justification::centredLeft);

    g.setColour (colAccent);
    g.fillRect (24, 62, 40, 2);

    // Panel de perillas
    auto panel = getLocalBounds().reduced (16).withTrimmedTop (60).toFloat();
    g.setColour (colPanel);
    g.fillRoundedRectangle (panel, 10.0f);

    // Medidor de reduccion de ganancia
    auto m = meterArea.toFloat();
    g.setColour (colBgBot);
    g.fillRoundedRectangle (m, 4.0f);

    const float fraction = juce::jlimit (0.0f, 1.0f, -displayedGrDb / 24.0f);
    g.setColour (colAccent);
    g.fillRoundedRectangle (m.withHeight (m.getHeight() * fraction), 4.0f);

    g.setColour (colMuted);
    g.setFont (makeFont (11.0f, true));
    g.drawText ("GR", meterArea.withY (meterArea.getY() - 20).withHeight (16),
                juce::Justification::centred);

    g.setColour (colText);
    g.setFont (makeFont (12.0f));
    g.drawText (juce::String (displayedGrDb, 1) + " dB",
                meterArea.withY (meterArea.getBottom() + 4).withHeight (16),
                juce::Justification::centred);
}

void CompressorEditor::resized()
{
    auto body = getLocalBounds().reduced (16).withTrimmedTop (60).reduced (12);

    auto meterColumn = body.removeFromRight (64);
    meterArea = meterColumn.withTrimmedTop (24).withTrimmedBottom (28).reduced (14, 0);

    Knob* knobs[] = { &threshold, &ratio, &attack, &release, &knee, &makeup, &mix };
    const int cellWidth = body.getWidth() / 7;

    for (auto* k : knobs)
    {
        auto cell = body.removeFromLeft (cellWidth);
        k->label.setBounds (cell.removeFromTop (22));
        k->slider.setBounds (cell);
    }
}

void CompressorEditor::timerCallback()
{
    const float gr = proc.gainReductionMeterDb.load();

    // Sube rapido (mas reduccion), baja suave
    if (gr < displayedGrDb)
        displayedGrDb = gr;
    else
        displayedGrDb += (gr - displayedGrDb) * 0.2f;

    repaint (meterArea.expanded (10, 36));
}
