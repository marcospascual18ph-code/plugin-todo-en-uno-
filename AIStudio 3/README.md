# AI Studio — plugin para macOS (Audio Unit + app Standalone)

Elige un género y genera un beat. Importa tu voz y obtén la mezcla y el mastering automáticos. También escribe la letra de la canción.

## Qué hace

1. **Beat por género**: Trap, Drill, Reggaeton, Hip-Hop, Pop, House, Lo-fi, R&B y Afrobeats. Genera batería, bajo/808 y acordes en el tono y BPM que elijas (8 a 64 compases, en bucle).
2. **Mezcla de voces**: filtro de graves, limpieza de medios, presencia y aire, de-esser, compresor y reverb. Iguala el nivel de la voz con el beat y baja el beat cuando canta la voz.
3. **Mastering**: EQ, compresión de cola, ajuste de loudness a -14 LUFS aprox. y limitador con techo de -1 dBFS.
4. **Letra**: usa la API de Anthropic y necesita tu propia API key. Se guarda en `~/Library/Application Support/AIStudio` en texto plano.
5. **Exportación** a WAV de 24 bit (beat o mezcla masterizada).

## Compilar (necesitas un Mac con Xcode y CMake)

```bash
cd AIStudio
cmake -B build -G Xcode
cmake --build build --config Release
```

JUCE se descarga solo (versión 8.0.6) la primera vez. Al terminar, el Audio Unit se copia a `~/Library/Audio/Plug-Ins/Components/`. La app Standalone queda en `build/AIStudio_artefacts/Release/Standalone/`.

Para comprobar el AU: `auval -v aumu Aibm Aist`. Después reinicia Logic Pro o GarageBand. Se carga como instrumento, en una pista de "Software Instrument".

## Uso

1. Elige género, tono, BPM y compases, y pulsa **Generar beat**.
2. Pulsa **Importar voces...** y elige tu grabación (WAV, AIFF, MP3, M4A o FLAC).
3. Pulsa **Mezclar y masterizar** y elige "Escuchar: mezcla masterizada".
4. Pulsa **Exportar WAV...** para guardar el resultado.
5. Para la letra, pega tu API key, escribe un tema y pulsa **Generar letra**.

## Límites de esta versión

- **El beat es procedural, no una IA generativa de audio.** Suena a plantilla de género con variaciones de hi-hats y sonidos sintetizados. Para audio generado por un modelo neuronal (por ejemplo MusicGen) habría que integrar ese modelo aparte.
- La mezcla y el mastering son una cadena de procesado fija con ajustes automáticos, no una IA que escucha y decide como un ingeniero.
- Funciona offline: el plugin renderiza el beat y la mezcla, y los reproduce al pulsar Play. No procesa en vivo la pista de la DAW.
- El audio se prepara a la frecuencia de muestreo del host en ese momento. Si cambias el sample rate del proyecto, vuelve a generar el beat e importar las voces.
- No está firmado ni notarizado. Para distribuirlo a otros Macs necesitas una cuenta de desarrollador de Apple.

## Siguientes pasos posibles

- Guardar la API key en el Llavero de macOS.
- Procesado en vivo de la pista de entrada (modo efecto).
- Ajustes manuales de mezcla (cantidad de reverb, loudness objetivo, ducking).
- Autotune.
