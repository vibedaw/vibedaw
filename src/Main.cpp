#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "core/AudioEngine.h"
#include "core/MidiManager.h"
#include "core/Constants.h"
#include "project/Project.h"
#include "project/Settings.h"
#include "plugins/PluginHost.h"
#include "ui/MainWindow.h"
#include "utils/Logger.h"

class VibeDawApplication : public juce::JUCEApplication {
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
        
        project->initialise(*audioEngine, *midiManager);
        
        if (project->getMasterTrack() && project->getMasterTrack()->getPlugin()) {
            auto* plugin = project->getMasterTrack()->getPlugin();
            plugin->prepareToPlay(audioEngine->getCurrentSampleRate(), 
                                   audioEngine->getCurrentBufferSize());
            audioEngine->setProcessor(plugin->getProcessor());
        }
        
        mainWindow = std::make_unique<vibedaw::MainWindow>(
            vibedaw::Constants::APP_NAME, *keyboardState, *midiManager, *project);
        
        LOG_INFO("VibeDAW: Application initialised successfully");
    }
    
    void shutdown() override {
        LOG_INFO("VibeDAW: Shutting down application");
        
        mainWindow.reset();
        
        if (audioEngine && project && project->getMasterTrack()) {
            audioEngine->clearProcessor();
        }
        
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
    
private:
    std::unique_ptr<vibedaw::AudioEngine> audioEngine;
    std::unique_ptr<vibedaw::MidiManager> midiManager;
    std::unique_ptr<vibedaw::Project> project;
    std::unique_ptr<juce::MidiKeyboardState> keyboardState;
    std::unique_ptr<vibedaw::MainWindow> mainWindow;
};

START_JUCE_APPLICATION(VibeDawApplication)
