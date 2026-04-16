#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <functional>

namespace vibedaw {

struct ScannedPlugin {
    juce::String name;
    juce::String path;
    juce::String manufacturer;
    
    ScannedPlugin() = default;
    ScannedPlugin(const juce::String& n, const juce::String& p, const juce::String& m = {})
        : name(n), path(p), manufacturer(m) {}
};

class PluginScanner {
public:
    using ScanCallback = std::function<void(const juce::String& path)>;
    using CompleteCallback = std::function<void()>;
    
    PluginScanner();
    ~PluginScanner() = default;
    
    void scanDefaultDirectories();
    void scanDirectory(const juce::File& directory, bool recursive = true);
    void clearScannedPlugins();
    
    const std::vector<ScannedPlugin>& getScannedPlugins() const { return scannedPlugins_; }
    int getNumScannedPlugins() const { return static_cast<int>(scannedPlugins_.size()); }
    const ScannedPlugin* getPlugin(int index) const;
    
    void addSearchPath(const juce::String& path);
    void clearSearchPaths() { searchPaths_.clear(); }
    const std::vector<juce::String>& getSearchPaths() const { return searchPaths_; }
    
    void setScanCallback(ScanCallback callback) { scanCallback_ = std::move(callback); }
    void setCompleteCallback(CompleteCallback callback) { completeCallback_ = std::move(callback); }
    
private:
    std::vector<ScannedPlugin> scannedPlugins_;
    std::vector<juce::String> searchPaths_;
    ScanCallback scanCallback_;
    CompleteCallback completeCallback_;
    
    void addPlugin(const juce::File& file);
    bool isVST3File(const juce::File& file) const;
    juce::String extractPluginName(const juce::File& file) const;
    juce::String extractManufacturer(const juce::File& file) const;
    juce::File expandPath(const juce::String& path) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScanner)
};

} // namespace vibedaw
