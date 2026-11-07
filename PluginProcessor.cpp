#include "PluginProcessor.h"
#include "PluginEditor.h"

CompressorProcessor::CompressorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CompressorProcessor::createLayout()
{
    using P  = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    using ID = juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<P> (ID { "threshold", 1 }, "Threshold (dB)", NR (-60.0f, 0.0f, 0.1f), -18.0f));
    layout.add (std::make_unique<P> (ID { "ratio",     1 }, "Ratio",         NR (1.0f, 20.0f, 0.1f, 0.5f), 4.0f));
    layout.add (std::make_unique<P> (ID { "attack",    1 }, "Attack (ms)",   NR (0.1f, 100.0f, 0.01f, 0.4f), 10.0f));
    layout.add (std::make_unique<P> (ID { "release",   1 }, "Release (ms)",  NR (10.0f, 1000.0f, 0.1f, 0.4f), 120.0f));
    layout.add (std::make_unique<P> (ID { "knee",      1 }, "Knee (dB)",     NR (0.0f, 24.0f, 0.1f), 6.0f));
    layout.add (std::make_unique<P> (ID { "makeup",    1 }, "Makeup (dB)",   NR (0.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<P> (ID { "mix",       1 }, "Mix (%)",       NR (0.0f, 100.0f, 0.1f), 100.0f));

    return layout;
}

void CompressorProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    smoothedGainReductionDb = 0.0f;
}

bool CompressorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void CompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const float thresholdDb = apvts.getRawParameterValue ("threshold")->load();
    const float ratio       = apvts.getRawParameterValue ("ratio")->load();
    const float attackMs    = apvts.getRawParameterValue ("attack")->load();
    const float releaseMs   = apvts.getRawParameterValue ("release")->load();
    const float kneeDb      = juce::jmax (apvts.getRawParameterValue ("knee")->load(), 0.01f);
    const float makeupGain  = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("makeup")->load());
    const float mix         = apvts.getRawParameterValue ("mix")->load() / 100.0f;

    const float attackCoef  = (float) std::exp (-1.0 / (0.001 * attackMs  * currentSampleRate));
    const float releaseCoef = (float) std::exp (-1.0 / (0.001 * releaseMs * currentSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        // Detector: pico mas alto entre canales (stereo linked)
        float peak = 0.0f;
        for (int c = 0; c < numChannels; ++c)
            peak = juce::jmax (peak, std::abs (buffer.getReadPointer (c)[i]));

        const float inDb = juce::Decibels::gainToDecibels (peak, -100.0f);
        const float over = inDb - thresholdDb;

        // Curva estatica con soft knee
        float outDb;
        if (2.0f * over < -kneeDb)
            outDb = inDb;
        else if (2.0f * std::abs (over) <= kneeDb)
        {
            const float x = over + kneeDb * 0.5f;
            outDb = inDb + (1.0f / ratio - 1.0f) * x * x / (2.0f * kneeDb);
        }
        else
            outDb = thresholdDb + over / ratio;

        const float targetGrDb = outDb - inDb; // siempre <= 0

        // Suavizado: ataque cuando aumenta la reduccion, release cuando baja
        const float coef = (targetGrDb < smoothedGainReductionDb) ? attackCoef : releaseCoef;
        smoothedGainReductionDb = coef * smoothedGainReductionDb + (1.0f - coef) * targetGrDb;

        const float gain = juce::Decibels::decibelsToGain (smoothedGainReductionDb) * makeupGain;

        for (int c = 0; c < numChannels; ++c)
        {
            auto* data = buffer.getWritePointer (c);
            const float dry = data[i];
            data[i] = dry * (1.0f - mix) + dry * gain * mix;
        }
    }

    gainReductionMeterDb.store (smoothedGainReductionDb);
}

juce::AudioProcessorEditor* CompressorProcessor::createEditor()
{
    return new CompressorEditor (*this);
}

void CompressorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CompressorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompressorProcessor();
}
