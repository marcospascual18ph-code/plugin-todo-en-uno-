#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class AIStudioEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AIStudioEditor (AIStudioProcessor&);
    ~AIStudioEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setStatus (const juce::String& s);
    void onGenreChanged();
    void saveApiKey();

    AIStudioProcessor& proc;

    // Beat
    juce::Label beatTitle { {}, "1. BEAT" };
    juce::ComboBox genreBox, keyBox, barsBox;
    juce::Slider bpmSlider;
    juce::TextButton genBeatBtn { "Generar beat" };

    // Voces + mezcla
    juce::Label vocalTitle { {}, "2. VOCES, MEZCLA Y MASTERING" };
    juce::TextButton loadVoxBtn { "Importar voces..." };
    juce::TextButton mixBtn { "Mezclar y masterizar" };

    // Reproducción / exportación
    juce::ComboBox sourceBox;
    juce::TextButton playBtn { "Play" };
    juce::TextButton exportBtn { "Exportar WAV..." };

    // Letra
    juce::Label lyricsTitle { {}, "3. LETRA" };
    juce::ComboBox langBox;
    juce::TextEditor themeEdit, apiKeyEdit, lyricsOut;
    juce::TextButton lyricsBtn { "Generar letra" };

    juce::Label statusLabel;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::ApplicationProperties props;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AIStudioEditor)
};
