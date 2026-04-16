#include "PluginScanner.h"
#include "core/Constants.h"
#include "utils/Logger.h"
#include <juce_core/juce_core.h>

namespace vibedaw {

PluginScanner::PluginScanner() {
    for (const auto& path : Constants::getDefaultVST3SearchPaths()) {
        addSearchPath(path);
    }
}

void PluginScanner::scanDefaultDirectories() {
    LOG_INFO("PluginScanner: Scanning default directories...");
    
    scannedPlugins_.clear();
    
    for (const auto& path : searchPaths_) {
        juce::File dir = expandPath(path);
        if (dir.exists() && dir.isDirectory()) {
            LOG_INFO("PluginScanner: Scanning " + dir.getFullPathName());
            scanDirectory(dir, true);
        } else {
            LOG_INFO("PluginScanner: Directory does not exist: " + dir.getFullPathName());
        }
    }
    
    LOG_INFO("PluginScanner: Found " + juce::String(scannedPlugins_.size()) + " plugins");
    
    if (completeCallback_) {
        completeCallback_();
    }
}

void PluginScanner::scanDirectory(const juce::File& directory, bool recursive) {
    if (!directory.exists() || !directory.isDirectory()) {
        return;
    }
    
    juce::Array<juce::File> files;
    
    if (recursive) {
        directory.findChildFiles(files, juce::File::findFilesAndDirectories, true);
    } else {
        directory.findChildFiles(files, juce::File::findFiles, false);
    }
    
    for (const auto& file : files) {
        if (isVST3File(file)) {
            addPlugin(file);
        }
    }
}

void PluginScanner::clearScannedPlugins() {
    scannedPlugins_.clear();
}

const ScannedPlugin* PluginScanner::getPlugin(int index) const {
    if (index >= 0 && index < static_cast<int>(scannedPlugins_.size())) {
        return &scannedPlugins_[index];
    }
    return nullptr;
}

void PluginScanner::addSearchPath(const juce::String& path) {
    if (std::find(searchPaths_.begin(), searchPaths_.end(), path) == searchPaths_.end()) {
        searchPaths_.push_back(path);
    }
}

void PluginScanner::addPlugin(const juce::File& file) {
    for (const auto& plugin : scannedPlugins_) {
        if (plugin.path == file.getFullPathName()) {
            return;
        }
    }
    
    ScannedPlugin plugin;
    plugin.name = extractPluginName(file);
    plugin.path = file.getFullPathName();
    plugin.manufacturer = extractManufacturer(file);
    
    scannedPlugins_.push_back(plugin);
    
    LOG_INFO("PluginScanner: Found plugin: " + plugin.name + " at " + plugin.path);
    
    if (scanCallback_) {
        scanCallback_(plugin.path);
    }
}

bool PluginScanner::isVST3File(const juce::File& file) const {
    return file.hasFileExtension(".vst3") || 
           (file.isDirectory() && file.getFileName().endsWithIgnoreCase(".vst3"));
}

juce::String PluginScanner::extractPluginName(const juce::File& file) const {
    juce::String name = file.getFileNameWithoutExtension();
    
    if (name.endsWithIgnoreCase(".vst3")) {
        name = name.dropLastCharacters(5);
    }
    
    return name;
}

juce::String PluginScanner::extractManufacturer(const juce::File& file) const {
    auto parent = file.getParentDirectory();
    juce::String parentName = parent.getFileName();
    
    if (parentName.endsWithIgnoreCase(".vst3") || 
        parentName.equalsIgnoreCase("vst3")) {
        return {};
    }
    
    return parentName;
}

juce::File PluginScanner::expandPath(const juce::String& path) const {
    if (path.startsWith("~")) {
        auto homeDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        auto relativePath = path.substring(1);
        while (relativePath.startsWith("/") || relativePath.startsWith(juce::File::getSeparatorString()))
            relativePath = relativePath.substring(1);
        return homeDir.getChildFile(relativePath);
    }
    return juce::File(path);
}

} // namespace vibedaw
