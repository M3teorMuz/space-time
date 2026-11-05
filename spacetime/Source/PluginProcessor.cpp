#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using namespace juce;

    String formatTime (float v)
    {
        return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (v, 1) + " ms";
    }
    float parseTime (const String& text)
    {
        const auto t = text.trim();
        const float v = t.getFloatValue();
        if (t.endsWithIgnoreCase ("ms")) return v;
        if (t.endsWithIgnoreCase ("s"))  return v * 1000.0f;
        return v;
    }
    String formatPercent (float v) { return String (roundToInt (v * 100.0f)) + " %"; }
    float parsePercent (const String& text) { return text.trim().getFloatValue() / 100.0f; }
}

SpaceTimeAudioProcessor::SpaceTimeAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pHostSync = apvts.getRawParameterValue ("hostSync");
    pAuto     = apvts.getRawParameterValue ("autoTiming");
    pBpm      = apvts.getRawParameterValue ("bpm");
    pSize     = apvts.getRawParameterValue ("size");
    pPre      = apvts.getRawParameterValue ("preDelay");
    pTime     = apvts.getRawParameterValue ("reverbTime");
    pMix      = apvts.getRawParameterValue ("mix");
    pWidth    = apvts.getRawParameterValue ("width");
    pDamp     = apvts.getRawParameterValue ("damping");
}

juce::AudioProcessorValueTreeState::ParameterLayout SpaceTimeAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "hostSync", 1 }, "Host BPM", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autoTiming", 1 }, "Auto Time", true));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "Manual BPM",
        NormalisableRange<float> (20.0f, 300.0f, 0.1f), 120.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    StringArray sizes;
    for (int i = 0; i < 6; ++i)
        sizes.add (spacetime::sizeTable()[i].name);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "size", 1 }, "Reverb Size", sizes, 3));

    NormalisableRange<float> preRange (0.0f, 500.0f, 0.01f);
    preRange.setSkewForCentre (40.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "preDelay", 1 }, "Pre-Delay", preRange, 15.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatTime (v); })
                                       .withValueFromStringFunction ([] (const String& s) { return parseTime (s); })));

    NormalisableRange<float> timeRange (20.0f, 30000.0f, 0.1f);
    timeRange.setSkewForCentre (1500.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reverbTime", 1 }, "Reverb Time", timeRange, 1000.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatTime (v); })
                                       .withValueFromStringFunction ([] (const String& s) { return parseTime (s); })));

    auto percent = AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return formatPercent (v); })
                       .withValueFromStringFunction ([] (const String& s) { return parsePercent (s); });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f, percent));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, percent));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "damping", 1 }, "Damping", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f, percent));

    return { p.begin(), p.end() };
}

double SpaceTimeAudioProcessor::getEffectiveBpm() const
{
    if (pHostSync->load() > 0.5f && hostBpmKnown.load())
        return hostBpm.load();
    return (double) pBpm->load();
}

void SpaceTimeAudioProcessor::prepareToPlay (double sampleRate, int)
{
    core.prepare (sampleRate);
}

void SpaceTimeAudioProcessor::releaseResources()
{
    core.reset();
}

bool SpaceTimeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void SpaceTimeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // --- информация от хоста (FL Studio) ---
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm())
            {
                if (*b >= 20.0 && *b <= 300.0)
                {
                    hostBpm.store (*b);
                    hostBpmKnown.store (true);
                }
            }
            if (auto ts = pos->getTimeSignature())
            {
                if (ts->numerator > 0 && ts->denominator > 0)
                    beatsPerBar.store ((double) ts->numerator * 4.0 / (double) ts->denominator);
            }
        }
    }

    const double bpm = getEffectiveBpm();
    const int sizeIndex = juce::jlimit (0, 5, juce::roundToInt (pSize->load()));
    const auto t = spacetime::resolve (pAuto->load() > 0.5f, bpm, beatsPerBar.load(), sizeIndex,
                                       pPre->load(), pTime->load());

    tailSeconds.store (juce::jlimit (0.5f, 30.0f, (t.totalMs * 0.001f) * 1.2f + 0.2f));

    core.setParameters (t.preMs, t.decayMs, pDamp->load(), pWidth->load(), pMix->load(), sizeIndex);

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh >= 2)
        core.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else if (numCh == 1)
        core.process (buffer.getWritePointer (0), nullptr, buffer.getNumSamples());
}

void SpaceTimeAudioProcessor::tapTempo()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (! tapTimes.isEmpty() && (now - tapTimes.getLast()) > 2500)
        tapTimes.clear();
    tapTimes.add (now);
    while (tapTimes.size() > 5)
        tapTimes.remove (0);
    if (tapTimes.size() < 2)
        return;

    const double avgMs = (double) (tapTimes.getLast() - tapTimes.getFirst()) / (double) (tapTimes.size() - 1);
    const float bpm = (float) spacetime::clampBpm (60000.0 / avgMs);

    if (auto* sync = apvts.getParameter ("hostSync"))
        sync->setValueNotifyingHost (0.0f);
    if (auto* value = apvts.getParameter ("bpm"))
        value->setValueNotifyingHost (value->convertTo0to1 (juce::roundToInt (bpm * 10.0f) / 10.0f));
}

juce::AudioProcessorEditor* SpaceTimeAudioProcessor::createEditor()
{
    return new SpaceTimeAudioProcessorEditor (*this);
}

void SpaceTimeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SpaceTimeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpaceTimeAudioProcessor();
}
