#include "Project.h"
#include "plugins/PluginHost.h"
#include "core/Constants.h"
#include "utils/Logger.h"

namespace vibedaw {

Project::Project() {
    LOG_INFO("Project: Created");
}

Project::~Project() {
    shutdown();
    LOG_INFO("Project: Destroyed");
}

bool Project::initialise(AudioEngine& audioEngine, MidiManager& midiManager) {
    LOG_INFO("Project: Initialising");
    
    loadSettings();
    
    if (settings.midiInputDevice.isNotEmpty()) {
        if (!midiManager.connectToDevice(settings.midiInputDevice)) {
            LOG_WARN("Project: Failed to connect to MIDI device: " + settings.midiInputDevice);
        }
    }
    
    if (channelList.getNumChannels() > 0) {
        setActiveChannel(0);
    }
    
    LOG_INFO("Project: Initialised");
    return true;
}

void Project::shutdown() {
    LOG_INFO("Project: Shutting down");
    saveSettings();
    LOG_INFO("Project: Shutdown complete");
}

void Project::loadSettings() {
    auto settingsFile = Settings::getDefaultSettingsFile();
    settings.loadFromFile(settingsFile);
    LOG_INFO("Project: Settings loaded");
}

void Project::saveSettings() {
    auto settingsFile = Settings::getDefaultSettingsFile();
    settings.saveToFile(settingsFile);
}

bool Project::loadPlugin(const juce::String& pluginPath) {
    LOG_INFO("Project: Loading plugin: " + pluginPath);
    
    auto pluginHost = std::make_unique<PluginHost>();
    if (!pluginHost->loadPlugin(pluginPath)) {
        LOG_ERROR("Project: Failed to load plugin");
        return false;
    }
    
    auto* channel = channelList.addChannel(pluginHost->getPluginName(), Channel::Type::Instrument);
    channel->setPlugin(std::move(pluginHost));
    
    settings.pluginPath = pluginPath;
    
    setActiveChannel(channelList.indexOfChannel(channel));
    
    LOG_INFO("Project: Plugin loaded successfully");
    return true;
}

void Project::setActiveChannel(int index) {
    if (activeChannelIndex_ == index) {
        return;
    }
    
    if (index >= channelList.getNumChannels()) {
        index = channelList.getNumChannels() - 1;
    }
    
    if (index < 0 && channelList.getNumChannels() > 0) {
        index = 0;
    }
    
    activeChannelIndex_ = index;
    notifyActiveChannelChanged(index);
    LOG_INFO("Project: Active channel set to " + juce::String(index));
}

void Project::addListener(Listener* listener) {
    listeners_.add(listener);
}

void Project::removeListener(Listener* listener) {
    listeners_.remove(listener);
}

void Project::notifyActiveChannelChanged(int newIndex) {
    listeners_.call([newIndex](Listener& l) { l.activeChannelChanged(newIndex); });
}

} // namespace vibedaw