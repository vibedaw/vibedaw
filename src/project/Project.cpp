#include "Project.h"
#include "plugins/PluginHost.h"
#include "core/Constants.h"
#include "utils/Logger.h"

namespace vibedaw {

Project::Project() {
    pluginLoader_ = [](const juce::String& path) -> std::unique_ptr<PluginHost> {
        auto host = std::make_unique<PluginHost>();
        return host->loadPlugin(path) ? std::move(host) : nullptr;
    };
    channelList.addListener(this);
    LOG_INFO("Project: Created");
}

Project::~Project() {
    channelList.removeListener(this);
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

bool Project::loadPlugin(const juce::String& pluginPath, ChannelId target) {
    JUCE_ASSERT_MESSAGE_THREAD
    LOG_INFO("Project: Loading plugin: " + pluginPath);
    const bool creating = target == InvalidChannelId;
    if (pluginPath.trim().isEmpty() ||
        (creating ? channelList.getNumChannels() >= ChannelList::maxChannels
                  : channelList.getChannelById(target) == nullptr)) return false;

    AudioQuiescence::Edit edit;
    auto pluginHost = pluginLoader_(pluginPath);
    if (!pluginHost || !pluginHost->isLoaded() ||
        pluginHost->getProcessor()->getTotalNumInputChannels() > 2 ||
        pluginHost->getProcessor()->getTotalNumOutputChannels() > 2) {
        LOG_ERROR("Project: Failed to load plugin");
        return false;
    }
    
    // Re-resolve after loading: hosted code may have dispatched message-thread work.
    auto* channel = creating
        ? channelList.addChannel(pluginHost->getPluginName(), Channel::Type::Instrument)
        : channelList.getChannelById(target);
    if (!channel) return false;
    channel->setPlugin(std::move(pluginHost));
    
    if (creating) {
        settings.pluginPath = pluginPath;
        setActiveChannel(channelList.indexOfChannel(channel));
    }
    
    LOG_INFO("Project: Plugin loaded successfully");
    return true;
}

void Project::setActiveChannel(int index) {
    auto* channel = channelList.getChannel(index);
    activeChannelId_ = channel ? channel->getId() : InvalidChannelId;
    notifyActiveChannelChanged(getActiveChannel());
    LOG_INFO("Project: Active channel set to " + juce::String(index));
}

void Project::channelListChanged() {
    if (!channelList.getChannelById(activeChannelId_)) activeChannelId_ = InvalidChannelId;
    notifyActiveChannelChanged(getActiveChannel());
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
