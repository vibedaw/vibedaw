#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "core/AudioEngine.h"
#include "core/MidiManager.h"
#include "core/ChannelMixer.h"
#include "core/Constants.h"
#include "project/Project.h"
#include "project/Settings.h"
#include "plugins/PluginHost.h"
#include "ui/MainWindow.h"
#include "utils/Logger.h"

class VibeDawApplication : public juce::JUCEApplication,
                           public vibedaw::Project::Listener {
public:
    VibeDawApplication() = default;
    
    const juce::String getApplicationName() override { return vibedaw::Constants::APP_NAME; }
    const juce::String getApplicationVersion() override { return vibedaw::Constants::APP_VERSION; }
    
    void initialise(const juce::String& commandLine) override {
        LOG_INFO("VibeDAW: Initialising application");
        
        audioEngine = std::make_unique<vibedaw::AudioEngine>();
        midiManager = std::make_unique<vibedaw::MidiManager>();
        project = std::make_unique<vibedaw::Project>();
        keyboardState = std::make_unique<juce::MidiKeyboardState>();
        
        audioEngine->initialise();
        
        keyboardState->addListener(&audioEngine->getMidiMessageCollector());
        
        project->addListener(this);
        project->initialise(*audioEngine, *midiManager);
        
        channelMixer = std::make_unique<vibedaw::ChannelMixer>(project->getChannelList());
        audioEngine->setProcessor(channelMixer.get());
        
        mainWindow = std::make_unique<vibedaw::MainWindow>(
            vibedaw::Constants::APP_NAME, *keyboardState, *midiManager, *project);
        
        LOG_INFO("VibeDAW: Application initialised successfully");
    }
    
    void shutdown() override {
        LOG_INFO("VibeDAW: Shutting down application");
        
        mainWindow.reset();
        audioEngine->clearProcessor();
        channelMixer.reset();
        project->shutdown();
        midiManager.reset();
        audioEngine->shutdown();
        
        project.reset();
        audioEngine.reset();
        
        LOG_INFO("VibeDAW: Application shutdown complete");
    }
    
    void systemRequestedQuit() override {
        LOG_INFO("VibeDAW: System requested quit");
        quit();
    }
    
    void activeChannelChanged(int newActiveIndex) override {
        if (channelMixer) {
            channelMixer->setActiveChannel(newActiveIndex);
        }
    }
    
private:
    std::unique_ptr<vibedaw::AudioEngine> audioEngine;
    std::unique_ptr<vibedaw::MidiManager> midiManager;
    std::unique_ptr<vibedaw::Project> project;
    std::unique_ptr<juce::MidiKeyboardState> keyboardState;
    std::unique_ptr<vibedaw::MainWindow> mainWindow;
    std::unique_ptr<vibedaw::ChannelMixer> channelMixer;
};

START_JUCE_APPLICATION(VibeDawApplication)
