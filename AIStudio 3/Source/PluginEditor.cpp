#include "PluginEditor.h"
#include "LyricsGenerator.h"

AIStudioEditor::AIStudioEditor (AIStudioProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (760, 680);

    juce::PropertiesFile::Options opts;
    opts.applicationName = "AIStudio";
    opts.folderName = "AIStudio";
    opts.osxLibrarySubFolder = "Application Support";
    opts.filenameSuffix = ".settings";
    props.setStorageParameters (opts);

    for (auto* l : { &beatTitle, &vocalTitle, &lyricsTitle })
    {
        l->setFont (juce::FontOptions (15.0f, juce::Font::bold));
        addAndMakeVisible (l);
    }

    // ---------- 1. Beat ----------
    int id = 1;
    for (auto& g : Beat::genres()) genreBox.addItem (g.name, id++);
    genreBox.setSelectedId (1, juce::dontSendNotification);
    genreBox.onChange = [this] { onGenreChanged(); };

    const char* keys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) keyBox.addItem (juce::String ("Tono ") + keys[i], i + 1);
    keyBox.setSelectedId (10, juce::dontSendNotification);

    for (int b : { 8, 16, 32, 64 }) barsBox.addItem (juce::String (b) + " compases", b);
    barsBox.setSelectedId (16, juce::dontSendNotification);

    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 24);
    bpmSlider.setTextValueSuffix (" BPM");
    onGenreChanged();

    genBeatBtn.onClick = [this]
    {
        setStatus ("Generando beat...");
        proc.generateBeat (genreBox.getSelectedId() - 1, (int) bpmSlider.getValue(),
                           keyBox.getSelectedId() - 1, barsBox.getSelectedId());
        sourceBox.setSelectedId (1, juce::dontSendNotification);
        proc.source = 0;
        proc.rewind();
        setStatus ("Beat listo. Pulsa Play para escucharlo (se repite en bucle).");
    };

    // ---------- 2. Voces, mezcla y mastering ----------
    loadVoxBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Elige tu grabación de voz", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.mp3;*.m4a;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  juce::String err;
                                  if (proc.loadVocals (f, err))
                                      setStatus ("Voces importadas: " + f.getFileName());
                                  else
                                      setStatus ("Error al importar: " + err);
                              });
    };

    mixBtn.onClick = [this]
    {
        if (! proc.hasBeat())   { setStatus ("Primero genera un beat."); return; }
        if (! proc.hasVocals()) { setStatus ("Primero importa tus voces."); return; }

        mixBtn.setEnabled (false);
        setStatus ("Mezclando y masterizando... (puede tardar unos segundos)");
        juce::Component::SafePointer<AIStudioEditor> safe (this);
        juce::Thread::launch ([safe, this]
        {
            juce::String err;
            const bool ok = proc.mixAndMaster (err);
            juce::MessageManager::callAsync ([safe, ok, err]
            {
                if (safe == nullptr) return;
                safe->mixBtn.setEnabled (true);
                if (ok)
                {
                    safe->sourceBox.setSelectedId (2, juce::dontSendNotification);
                    safe->proc.source = 1;
                    safe->proc.rewind();
                    safe->setStatus ("Mezcla masterizada lista (-14 LUFS aprox., pico -1 dBFS). Pulsa Play.");
                }
                else
                    safe->setStatus ("Error: " + err);
            });
        });
    };

    // ---------- Reproducción / exportación ----------
    sourceBox.addItem ("Escuchar: beat", 1);
    sourceBox.addItem ("Escuchar: mezcla masterizada", 2);
    sourceBox.setSelectedId (1, juce::dontSendNotification);
    sourceBox.onChange = [this]
    {
        const bool master = sourceBox.getSelectedId() == 2;
        if (master && ! proc.hasMaster())
        {
            sourceBox.setSelectedId (1, juce::dontSendNotification);
            setStatus ("Aún no hay mezcla. Pulsa 'Mezclar y masterizar'.");
            return;
        }
        proc.source = master ? 1 : 0;
        proc.rewind();
    };

    playBtn.onClick = [this]
    {
        if (proc.playing) proc.playing = false;
        else { proc.rewind(); proc.playing = true; }
    };

    exportBtn.onClick = [this]
    {
        const bool master = sourceBox.getSelectedId() == 2;
        if (master ? ! proc.hasMaster() : ! proc.hasBeat()) { setStatus ("No hay nada que exportar todavía."); return; }

        chooser = std::make_unique<juce::FileChooser> ("Exportar WAV (24 bit)",
                      juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                          .getChildFile (master ? "mezcla_master.wav" : "beat.wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, master] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  f = f.withFileExtension ("wav");
                                  setStatus (proc.exportWav (f, master) ? "Exportado: " + f.getFullPathName()
                                                                         : "No se pudo exportar el archivo.");
                              });
    };

    // ---------- 3. Letra ----------
    for (auto* l : { "Español", "English", "Português", "Français", "Italiano" })
        langBox.addItem (l, langBox.getNumItems() + 1);
    langBox.setSelectedId (1, juce::dontSendNotification);

    themeEdit.setTextToShowWhenEmpty ("Tema o idea de la canción (opcional)", juce::Colours::grey);
    apiKeyEdit.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    apiKeyEdit.setTextToShowWhenEmpty ("API key de Anthropic (sk-ant-...)", juce::Colours::grey);
    apiKeyEdit.setText (props.getUserSettings()->getValue ("apiKey"), false);
    apiKeyEdit.onFocusLost = [this] { saveApiKey(); };
    apiKeyEdit.onReturnKey = [this] { saveApiKey(); };

    lyricsOut.setMultiLine (true);
    lyricsOut.setReadOnly (false);
    lyricsOut.setReturnKeyStartsNewLine (true);
    lyricsOut.setScrollbarsShown (true);
    lyricsOut.setTextToShowWhenEmpty ("Aquí aparecerá la letra...", juce::Colours::grey);

    lyricsBtn.onClick = [this]
    {
        saveApiKey();
        lyricsBtn.setEnabled (false);
        setStatus ("Escribiendo la letra...");
        const auto key = apiKeyEdit.getText();
        const auto genre = genreBox.getText();
        const auto lang = langBox.getText();
        const auto theme = themeEdit.getText();
        juce::Component::SafePointer<AIStudioEditor> safe (this);
        juce::Thread::launch ([safe, key, genre, lang, theme]
        {
            juce::String err;
            const auto text = Lyrics::generate (key, genre, lang, theme, err);
            juce::MessageManager::callAsync ([safe, text, err]
            {
                if (safe == nullptr) return;
                safe->lyricsBtn.setEnabled (true);
                if (text.isNotEmpty()) { safe->lyricsOut.setText (text); safe->setStatus ("Letra lista. Puedes editarla."); }
                else safe->setStatus ("Error: " + err);
            });
        });
    };

    for (auto* c : std::initializer_list<juce::Component*> {
             &genreBox, &keyBox, &barsBox, &bpmSlider, &genBeatBtn, &loadVoxBtn, &mixBtn,
             &sourceBox, &playBtn, &exportBtn, &langBox, &themeEdit, &apiKeyEdit, &lyricsOut,
             &lyricsBtn, &statusLabel })
        addAndMakeVisible (c);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    setStatus ("Elige un género y pulsa 'Generar beat'.");
    startTimerHz (10);
}

AIStudioEditor::~AIStudioEditor()
{
    saveApiKey();
    stopTimer();
}

void AIStudioEditor::saveApiKey()
{
    if (auto* s = props.getUserSettings())
    {
        s->setValue ("apiKey", apiKeyEdit.getText());
        props.saveIfNeeded();
    }
}

void AIStudioEditor::onGenreChanged()
{
    const auto& g = Beat::genres()[(size_t) (genreBox.getSelectedId() - 1)];
    bpmSlider.setRange (g.bpmMin, g.bpmMax, 1.0);
    bpmSlider.setValue ((g.bpmMin + g.bpmMax) / 2, juce::dontSendNotification);
}

void AIStudioEditor::setStatus (const juce::String& s) { statusLabel.setText (s, juce::dontSendNotification); }

void AIStudioEditor::timerCallback()
{
    const juce::String t = proc.playing ? "Stop" : "Play";
    if (playBtn.getButtonText() != t) playBtn.setButtonText (t);
}

void AIStudioEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));
    g.setColour (juce::Colour (0xff2a2d36));
    for (int y : { 100, 182, 262 })
        g.drawHorizontalLine (y, 16.0f, (float) getWidth() - 16.0f);
}

void AIStudioEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    // Beat
    beatTitle.setBounds (r.removeFromTop (24));
    auto row = r.removeFromTop (32);
    genreBox.setBounds (row.removeFromLeft (170).reduced (0, 2));
    row.removeFromLeft (8);
    keyBox.setBounds (row.removeFromLeft (110).reduced (0, 2));
    row.removeFromLeft (8);
    barsBox.setBounds (row.removeFromLeft (140).reduced (0, 2));
    row.removeFromLeft (8);
    genBeatBtn.setBounds (row.reduced (0, 2));
    r.removeFromTop (6);
    bpmSlider.setBounds (r.removeFromTop (28));
    r.removeFromTop (22);

    // Voces
    vocalTitle.setBounds (r.removeFromTop (24));
    row = r.removeFromTop (34);
    loadVoxBtn.setBounds (row.removeFromLeft (200).reduced (0, 2));
    row.removeFromLeft (8);
    mixBtn.setBounds (row.removeFromLeft (220).reduced (0, 2));
    r.removeFromTop (6);
    row = r.removeFromTop (34);
    sourceBox.setBounds (row.removeFromLeft (260).reduced (0, 2));
    row.removeFromLeft (8);
    playBtn.setBounds (row.removeFromLeft (90).reduced (0, 2));
    row.removeFromLeft (8);
    exportBtn.setBounds (row.removeFromLeft (160).reduced (0, 2));
    r.removeFromTop (22);

    // Letra
    lyricsTitle.setBounds (r.removeFromTop (24));
    row = r.removeFromTop (32);
    langBox.setBounds (row.removeFromLeft (130).reduced (0, 2));
    row.removeFromLeft (8);
    themeEdit.setBounds (row.reduced (0, 2));
    r.removeFromTop (6);
    row = r.removeFromTop (32);
    lyricsBtn.setBounds (row.removeFromRight (150).reduced (0, 2));
    row.removeFromRight (8);
    apiKeyEdit.setBounds (row.reduced (0, 2));
    r.removeFromTop (6);

    statusLabel.setBounds (r.removeFromBottom (26));
    lyricsOut.setBounds (r);
}
