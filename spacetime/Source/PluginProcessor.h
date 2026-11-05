#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "ReverbCore.h"
#include "Timing.h"

class SpaceTimeAudioProcessor : public juce::AudioProcessor
{
public:
    SpaceTimeAudioProcessor();
    ~SpaceTimeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return (double) tailSeconds.load(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    void tapTempo();

    // Для интерфейса
    double getHostBpm() const { return hostBpm.load(); }
    double getBeatsPerBar() const { return beatsPerBar.load(); }
    bool hasHostBpm() const { return hostBpmKnown.load(); }
    // Текущий эффективный BPM (хост или ручной)
    double getEffectiveBpm() const;

private:
    spacetime::ReverbCore core;

    std::atomic<float>* pHostSync = nullptr;
    std::atomic<float>* pAuto = nullptr;
    std::atomic<float>* pBpm = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pPre = nullptr;
    std::atomic<float>* pTime = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pDamp = nullptr;

    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostBpmKnown { false };
    std::atomic<double> beatsPerBar { 4.0 };
    std::atomic<float> tailSeconds { 4.0f };

    juce::Array<juce::uint32> tapTimes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceTimeAudioProcessor)
};
