#include "Project.h"
#include "plugins/PluginHost.h"
#include "core/Constants.h"
#include "utils/Logger.h"

namespace vibedaw {

Project::Project() {
    masterTrack = std::make_unique<Track>("Master");
    LOG_INFO("Project: Created");
}

Project::~Project() {
    shutdown();
    LOG_INFO("Project: Destroyed");
}

bool Project::initialise(AudioEngine& audioEngine, MidiManager& midiManager) {
    LOG_INFO("Project: Initialising");
    
    loadSettings();
    
    if (audioEngine.getCurrentSampleRate() > 0.0 && masterTrack) {
        masterTrack->prepareToPlay(audioEngine.getCurrentSampleRate(), 
                                    audioEngine.getCurrentBufferSize());
    }
    
    if (settings.pluginPath.isNotEmpty()) {
        if (!loadPlugin(settings.pluginPath)) {
            LOG_WARN("Project: Failed to load plugin from settings, continuing without plugin");
        }
    }
    
    if (masterTrack && masterTrack->getPlugin()) {
        audioEngine.setProcessor(masterTrack->getPlugin()->getProcessor());
    }
    
    if (settings.midiInputDevice.isNotEmpty()) {
        if (!midiManager.connectToDevice(settings.midiInputDevice)) {
            LOG_WARN("Project: Failed to connect to MIDI device: " + settings.midiInputDevice);
        }
    }
    
    LOG_INFO("Project: Initialised");
    return true;
}

void Project::shutdown() {
    LOG_INFO("Project: Shutting down");
    saveSettings();
    if (masterTrack) {
        masterTrack->releaseResources();
    }
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
    
    auto plugin = std::make_unique<PluginHost>();
    if (!plugin->loadPlugin(pluginPath)) {
        LOG_ERROR("Project: Failed to load plugin");
        return false;
    }
    
    settings.pluginPath = pluginPath;
    
    if (masterTrack) {
        masterTrack->setPlugin(std::move(plugin));
    }
    
    return true;
}

juce::AudioProcessor* Project::getProcessorGraph() {
    if (masterTrack && masterTrack->getPlugin()) {
        return masterTrack->getPlugin()->getProcessor();
    }
    return nullptr;
}

} // namespace vibedaw
