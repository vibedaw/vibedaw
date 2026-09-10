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
#include "ui/DawLookAndFeel.h"
#include "ui/components/TextPrompt.h"
#include "utils/Logger.h"

class VibeDawApplication : public juce::JUCEApplication,
                           public vibedaw::Project::Listener {
public:
    VibeDawApplication() = default;
    
    const juce::String getApplicationName() override { return vibedaw::Constants::APP_NAME; }
    const juce::String getApplicationVersion() override { return vibedaw::Constants::APP_VERSION; }
    
    void initialise(const juce::String& commandLine) override {
        LOG_INFO("VibeDAW: Initialising application");
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
        
        audioEngine = std::make_unique<vibedaw::AudioEngine>();
        midiManager = std::make_unique<vibedaw::MidiManager>();
        project = std::make_unique<vibedaw::Project>();
        keyboardState = std::make_unique<juce::MidiKeyboardState>();
        midiManager->setAudioDestination(*audioEngine, *keyboardState);
        
        audioEngine->initialise();
        
        keyboardState->addListener(audioEngine.get());
        
        project->addListener(this);
        project->initialise(*audioEngine, *midiManager);
        
        channelMixer = std::make_unique<vibedaw::ChannelMixer>(project->getChannelList(),
            project->getTrackList(), project->getClipPool(), project->getTransportState());
        channelMixer->setActiveChannel(project->getActiveChannel());
        audioEngine->setProcessor(channelMixer.get());
        
        mainWindow = std::make_unique<vibedaw::MainWindow>(
            vibedaw::Constants::APP_NAME, *keyboardState, *midiManager, *project, *audioEngine);
        
        LOG_INFO("VibeDAW: Application initialised successfully");
    }
    
    void shutdown() override {
        LOG_INFO("VibeDAW: Shutting down application");
        
        mainWindow.reset();
        midiManager->disconnect();
        keyboardState->removeListener(audioEngine.get());
        audioEngine->clearProcessor();
        channelMixer.reset();
        project->shutdown();
        midiManager.reset();
        audioEngine->shutdown();
        
        project.reset();
        audioEngine.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        
        LOG_INFO("VibeDAW: Application shutdown complete");
    }
    
    void systemRequestedQuit() override {
        LOG_INFO("VibeDAW: System requested quit");
        // Prompt before discarding unsaved project work; the asynchronous
        // confirm keeps the message loop alive until the user decides.
        if (project && project->isDirty()) {
            vibedaw::confirmAsync("Unsaved changes", "The current project has unsaved changes. Quit without saving?",
                "Quit without saving", nullptr, []() {
                    if (auto* app = juce::JUCEApplication::getInstance()) app->quit();
                });
            return;
        }
        quit();
    }
    
    void activeChannelChanged(int newActiveIndex) override {
        if (channelMixer) {
            channelMixer->setActiveChannel(newActiveIndex);
        }
    }
    
private:
    vibedaw::DawLookAndFeel lookAndFeel;
    std::unique_ptr<vibedaw::AudioEngine> audioEngine;
    std::unique_ptr<vibedaw::MidiManager> midiManager;
    std::unique_ptr<vibedaw::Project> project;
    std::unique_ptr<juce::MidiKeyboardState> keyboardState;
    std::unique_ptr<vibedaw::MainWindow> mainWindow;
    std::unique_ptr<vibedaw::ChannelMixer> channelMixer;
};

START_JUCE_APPLICATION(VibeDawApplication)
