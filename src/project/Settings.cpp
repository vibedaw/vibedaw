#include "Settings.h"
#include "core/Constants.h"
#include "utils/Logger.h"
#include <juce_core/juce_core.h>

namespace vibedaw {

juce::File Settings::getDefaultSettingsFile() {
    juce::File configDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                          .getChildFile(Constants::CONFIG_DIR);
    
    if (!configDir.exists()) {
        configDir.createDirectory();
        LOG_INFO("Settings: Created config directory: " + configDir.getFullPathName());
    }
    
    return configDir.getChildFile(Constants::SETTINGS_FILE);
}

void Settings::loadFromFile(const juce::File& file) {
    if (!file.exists()) {
        LOG_INFO("Settings: File does not exist, using defaults");
        pluginPath = juce::String(Constants::DEFAULT_VST3_PATH).replace("~", 
                     juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName());
        return;
    }
    
    auto json = juce::JSON::parse(file);
    
    if (json.hasProperty("pluginPath")) {
        pluginPath = json["pluginPath"].toString();
    } else {
        pluginPath = juce::String(Constants::DEFAULT_VST3_PATH).replace("~",
                     juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName());
    }
    
    if (json.hasProperty("audioDeviceType")) {
        audioDeviceType = json["audioDeviceType"].toString();
    }
    
    if (json.hasProperty("audioOutputDevice")) {
        audioOutputDevice = json["audioOutputDevice"].toString();
    }
    
    if (json.hasProperty("midiInputDevice")) {
        midiInputDevice = json["midiInputDevice"].toString();
    }
    
    if (json.hasProperty("sampleRate")) {
        sampleRate = json["sampleRate"];
    }
    
    if (json.hasProperty("bufferSize")) {
        bufferSize = json["bufferSize"];
    }
    
    if (json.hasProperty("leftSidebars")) {
        auto sidebarsJson = json["leftSidebars"];
        if (sidebarsJson.isArray()) {
            for (int i = 0; i < sidebarsJson.size(); ++i) {
                auto sb = sidebarsJson[i];
                SidebarSettings settings;
                settings.name = sb["name"].toString();
                settings.width = sb["width"];
                settings.expanded = sb["expanded"];
                settings.order = sb["order"];
                leftSidebars.add(settings);
            }
        }
    }
    
    if (json.hasProperty("rightSidebars")) {
        auto sidebarsJson = json["rightSidebars"];
        if (sidebarsJson.isArray()) {
            for (int i = 0; i < sidebarsJson.size(); ++i) {
                auto sb = sidebarsJson[i];
                SidebarSettings settings;
                settings.name = sb["name"].toString();
                settings.width = sb["width"];
                settings.expanded = sb["expanded"];
                settings.order = sb["order"];
                rightSidebars.add(settings);
            }
        }
    }
    
    LOG_INFO("Settings: Loaded from " + file.getFullPathName());
}

void Settings::saveToFile(const juce::File& file) {
    juce::DynamicObject::Ptr jsonObj = new juce::DynamicObject();
    
    jsonObj->setProperty("pluginPath", pluginPath);
    jsonObj->setProperty("audioDeviceType", audioDeviceType);
    jsonObj->setProperty("audioOutputDevice", audioOutputDevice);
    jsonObj->setProperty("midiInputDevice", midiInputDevice);
    jsonObj->setProperty("sampleRate", sampleRate);
    jsonObj->setProperty("bufferSize", bufferSize);
    
    juce::Array<juce::var> leftSidebarsJson;
    for (const auto& sb : leftSidebars) {
        juce::DynamicObject::Ptr sbObj = new juce::DynamicObject();
        sbObj->setProperty("name", sb.name);
        sbObj->setProperty("width", sb.width);
        sbObj->setProperty("expanded", sb.expanded);
        sbObj->setProperty("order", sb.order);
        leftSidebarsJson.add(juce::var(sbObj.get()));
    }
    jsonObj->setProperty("leftSidebars", leftSidebarsJson);
    
    juce::Array<juce::var> rightSidebarsJson;
    for (const auto& sb : rightSidebars) {
        juce::DynamicObject::Ptr sbObj = new juce::DynamicObject();
        sbObj->setProperty("name", sb.name);
        sbObj->setProperty("width", sb.width);
        sbObj->setProperty("expanded", sb.expanded);
        sbObj->setProperty("order", sb.order);
        rightSidebarsJson.add(juce::var(sbObj.get()));
    }
    jsonObj->setProperty("rightSidebars", rightSidebarsJson);
    
    juce::String jsonText = juce::JSON::toString(juce::var(jsonObj.get()), true);
    
    if (file.replaceWithText(jsonText)) {
        LOG_INFO("Settings: Saved to " + file.getFullPathName());
    } else {
        LOG_ERROR("Settings: Failed to save to " + file.getFullPathName());
    }
}

} // namespace vibedaw
