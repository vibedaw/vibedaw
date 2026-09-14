#include "InternalPluginFormat.h"
#include "VibeSynthProcessor.h"
#include "utils/Logger.h"

namespace vibedaw {

juce::PluginDescription InternalPluginFormat::makeDescription() {
    juce::PluginDescription description;
    description.name = "VibeSynth";
    description.descriptiveName = "VibeDAW built-in polyphonic synth";
    description.pluginFormatName = formatName;
    description.fileOrIdentifier = identifier;
    description.manufacturerName = "VibeDAW";
    description.version = "1.0.0";
    description.category = "Instrument";
    description.uniqueId = 0x76696273; // 'vibs'
    description.isInstrument = true;
    description.numInputChannels = 0;
    description.numOutputChannels = 2;
    description.hasSharedContainer = false;
    return description;
}

std::unique_ptr<juce::AudioPluginInstance> InternalPluginFormat::createInstance() {
    return std::make_unique<VibeSynthProcessor>();
}

bool InternalPluginFormat::claimsIdentifier(const juce::String& fileOrIdentifier) {
    return fileOrIdentifier.trim().startsWithIgnoreCase(identifier);
}

void InternalPluginFormat::findAllTypesForFile(juce::OwnedArray<juce::PluginDescription>& results,
                                               const juce::String& fileOrIdentifier) {
    if (claimsIdentifier(fileOrIdentifier)) results.add(new juce::PluginDescription(makeDescription()));
}

bool InternalPluginFormat::fileMightContainThisPluginType(const juce::String& fileOrIdentifier) {
    return claimsIdentifier(fileOrIdentifier);
}

juce::String InternalPluginFormat::getNameOfPluginFromIdentifier(const juce::String& fileOrIdentifier) {
    return claimsIdentifier(fileOrIdentifier) ? makeDescription().name : fileOrIdentifier;
}

bool InternalPluginFormat::doesPluginStillExist(const juce::PluginDescription& description) {
    return claimsIdentifier(description.fileOrIdentifier);
}

void InternalPluginFormat::createPluginInstance(const juce::PluginDescription&,
                                                double /*initialSampleRate*/,
                                                int /*initialBufferSize*/,
                                                PluginCreationCallback callback) {
    auto instance = createInstance();
    if (instance) {
        callback(std::move(instance), {});
    } else {
        LOG_ERROR("InternalPluginFormat: Failed to create VibeSynth");
        callback(nullptr, "Failed to create the built-in instrument.");
    }
}

} // namespace vibedaw
