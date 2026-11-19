#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include "BeatGenerator.h"
#include "MixMaster.h"

class AIStudioProcessor : public juce::AudioProcessor
{
public:
    AIStudioProcessor();
    ~AIStudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AI Studio"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // --- Funciones del estudio ---
    void generateBeat (int genreIndex, int bpm, int key, int bars);
    bool loadVocals (const juce::File& file, juce::String& error);
    bool mixAndMaster (juce::String& error);
    bool exportWav (const juce::File& file, bool master);

    bool hasBeat() const;
    bool hasVocals() const;
    bool hasMaster() const;

    std::atomic<bool> playing { false };
    std::atomic<int> source { 0 }; // 0 = beat, 1 = mezcla masterizada
    void rewind() { playPos = 0; }

private:
    mutable juce::CriticalSection lock;
    juce::AudioBuffer<float> beatBuf, vocalBuf, masterBuf;
    std::atomic<int> playPos { 0 };
    double currentSR = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AIStudioProcessor)
};
