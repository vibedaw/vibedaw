#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

// Resolves the built-in instrument by a stable identifier, so the ordinary
// load path and project restore need no special cases. Registered in the
// PluginHost format manager; never scans the filesystem.
class InternalPluginFormat : public juce::AudioPluginFormat {
public:
    static constexpr const char* formatName = "Internal";
    static constexpr const char* identifier = "internal://vibesynth";

    InternalPluginFormat() = default;
    ~InternalPluginFormat() override = default;

    static juce::PluginDescription makeDescription();
    static std::unique_ptr<juce::AudioPluginInstance> createInstance();
    static bool claimsIdentifier(const juce::String& fileOrIdentifier);

    juce::String getName() const override { return formatName; }
    void findAllTypesForFile(juce::OwnedArray<juce::PluginDescription>& results,
                             const juce::String& fileOrIdentifier) override;
    bool fileMightContainThisPluginType(const juce::String& fileOrIdentifier) override;
    juce::String getNameOfPluginFromIdentifier(const juce::String& fileOrIdentifier) override;
    bool pluginNeedsRescanning(const juce::PluginDescription&) override { return false; }
    bool doesPluginStillExist(const juce::PluginDescription& description) override;
    bool canScanForPlugins() const override { return false; }
    bool isTrivialToScan() const override { return true; }
    juce::StringArray searchPathsForPlugins(const juce::FileSearchPath&, bool, bool) override { return {}; }
    juce::FileSearchPath getDefaultLocationsToSearch() override { return {}; }
    bool requiresUnblockedMessageThreadDuringCreation(const juce::PluginDescription&) const override { return false; }

protected:
    void createPluginInstance(const juce::PluginDescription&, double initialSampleRate,
                              int initialBufferSize, PluginCreationCallback callback) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InternalPluginFormat)
};

} // namespace vibedaw
