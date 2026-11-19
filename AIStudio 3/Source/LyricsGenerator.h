#pragma once
#include <juce_core/juce_core.h>

// Letras de canción con la API de Anthropic (requiere tu propia API key).
namespace Lyrics
{
inline juce::String generate (const juce::String& apiKey, const juce::String& genre,
                              const juce::String& language, const juce::String& theme,
                              juce::String& error)
{
    if (apiKey.trim().isEmpty())
    {
        error = "Falta la API key de Anthropic (consola: console.anthropic.com).";
        return {};
    }

    const juce::String prompt =
        "Escribe la letra original de una canción de género " + genre + " en " + language
        + ". Tema: " + (theme.isEmpty() ? juce::String ("libre") : theme)
        + ". Estructura: [Intro], [Verso 1], [Pre-estribillo], [Estribillo], [Verso 2], [Estribillo], [Puente], [Estribillo final]. "
          "Adapta el vocabulario, la métrica y el flow al género. Devuelve solo la letra con las etiquetas de sección, sin comentarios.";

    auto* msg = new juce::DynamicObject();
    msg->setProperty ("role", "user");
    msg->setProperty ("content", prompt);

    juce::Array<juce::var> msgs;
    msgs.add (juce::var (msg));

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("model", "claude-sonnet-5-5");
    obj->setProperty ("max_tokens", 1500);
    obj->setProperty ("messages", msgs);

    const juce::String body = juce::JSON::toString (juce::var (obj));

    int status = 0;
    auto url = juce::URL ("https://api.anthropic.com/v1/messages").withPOSTData (body);
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders ("content-type: application/json\r\nx-api-key: " + apiKey.trim()
                                       + "\r\nanthropic-version: 2023-06-01\r\n")
                    .withConnectionTimeoutMs (60000)
                    .withStatusCode (&status)
                    .withHttpRequestCmd ("POST");

    auto stream = url.createInputStream (opts);
    if (stream == nullptr)
    {
        error = "No se pudo conectar (¿sin internet?).";
        return {};
    }

    const auto parsed = juce::JSON::parse (stream->readEntireStreamAsString());
    if (status != 200)
    {
        error = parsed["error"]["message"].toString();
        if (error.isEmpty()) error = "Error HTTP " + juce::String (status);
        return {};
    }
    return parsed["content"][0]["text"].toString();
}
} // namespace Lyrics
