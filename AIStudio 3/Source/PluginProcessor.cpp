#include "PluginProcessor.h"
#include "PluginEditor.h"

AIStudioProcessor::AIStudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void AIStudioProcessor::prepareToPlay (double sampleRate, int)
{
    if (sampleRate > 0.0) currentSR = sampleRate;
}

bool AIStudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AIStudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! playing.load())
        return;

    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked())
        return;

    const bool useMaster = source.load() == 1;
    const auto& src = useMaster ? masterBuf : beatBuf;
    const int n = src.getNumSamples();
    if (n == 0)
        return;

    int pos = playPos.load();
    const int outCh = buffer.getNumChannels();

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (pos >= n)
        {
            if (useMaster) { playing = false; pos = 0; break; }
            pos = 0; // el beat se repite en bucle
        }
        for (int ch = 0; ch < outCh; ++ch)
            buffer.setSample (ch, i, src.getSample (juce::jmin (ch, src.getNumChannels() - 1), pos));
        ++pos;
    }
    playPos = pos;
}

void AIStudioProcessor::generateBeat (int genreIndex, int bpm, int key, int bars)
{
    const auto& spec = Beat::genres()[(size_t) juce::jlimit (0, (int) Beat::genres().size() - 1, genreIndex)];
    auto b = Beat::render (spec, bpm, key, bars, currentSR, juce::Random::getSystemRandom().nextInt());

    const juce::ScopedLock sl (lock);
    beatBuf = std::move (b);
    masterBuf.setSize (0, 0); // la mezcla anterior ya no corresponde al nuevo beat
    playPos = 0;
}

bool AIStudioProcessor::loadVocals (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Formato de audio no compatible."; return false; }

    const double maxSeconds = 600.0;
    const auto srcLen = (juce::int64) juce::jmin ((double) reader->lengthInSamples, maxSeconds * reader->sampleRate);
    const int ch = (int) juce::jmin (2u, reader->numChannels);

    juce::AudioBuffer<float> raw (ch, (int) srcLen);
    reader->read (&raw, 0, (int) srcLen, 0, true, ch > 1);

    juce::AudioBuffer<float> out;
    if (std::abs (reader->sampleRate - currentSR) < 1.0)
    {
        out = std::move (raw);
    }
    else
    {
        const double ratio = reader->sampleRate / currentSR;
        const int outLen = (int) ((double) srcLen / ratio);
        out.setSize (ch, outLen);
        for (int c = 0; c < ch; ++c)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, raw.getReadPointer (c), out.getWritePointer (c), outLen);
        }
    }

    const juce::ScopedLock sl (lock);
    vocalBuf = std::move (out);
    masterBuf.setSize (0, 0);
    return true;
}

bool AIStudioProcessor::mixAndMaster (juce::String& error)
{
    juce::AudioBuffer<float> beatCopy, voxCopy;
    {
        const juce::ScopedLock sl (lock);
        beatCopy = beatBuf;
        voxCopy = vocalBuf;
    }
    if (beatCopy.getNumSamples() == 0) { error = "Primero genera un beat."; return false; }
    if (voxCopy.getNumSamples() == 0) { error = "Primero importa tus voces."; return false; }

    auto result = MixMaster::run (beatCopy, voxCopy, currentSR, {});

    const juce::ScopedLock sl (lock);
    masterBuf = std::move (result);
    return true;
}

bool AIStudioProcessor::exportWav (const juce::File& file, bool master)
{
    juce::AudioBuffer<float> copy;
    {
        const juce::ScopedLock sl (lock);
        copy = master ? masterBuf : beatBuf;
    }
    if (copy.getNumSamples() == 0) return false;

    file.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream> (file);
    if (stream->failedToOpen()) return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), currentSR, (unsigned) copy.getNumChannels(), 24, {}, 0));
    if (writer == nullptr) return false;
    stream.release(); // el writer es ahora dueño del stream

    return writer->writeFromAudioSampleBuffer (copy, 0, copy.getNumSamples());
}

bool AIStudioProcessor::hasBeat() const    { const juce::ScopedLock sl (lock); return beatBuf.getNumSamples() > 0; }
bool AIStudioProcessor::hasVocals() const  { const juce::ScopedLock sl (lock); return vocalBuf.getNumSamples() > 0; }
bool AIStudioProcessor::hasMaster() const  { const juce::ScopedLock sl (lock); return masterBuf.getNumSamples() > 0; }

juce::AudioProcessorEditor* AIStudioProcessor::createEditor() { return new AIStudioEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AIStudioProcessor(); }
