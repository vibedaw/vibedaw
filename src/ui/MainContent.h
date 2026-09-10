#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TransportComponent.h"
#include "core/TransportState.h"
#include "core/AudioEngine.h"
#include "core/MidiManager.h"
#include "project/Project.h"
#include "panels/PanelContainer.h"
#include "panels/TimelinePanel.h"
#include "panels/MixerPanel.h"
#include "panels/PianoPanel.h"
#include "sidebar/SidebarContainer.h"
#include "plugins/PluginScanner.h"
#include "sidebar/browser/BrowserSidebar.h"
#include "sidebar/clips/ClipsSidebar.h"
#include "editor/ClipEditorWindow.h"
#include "components/PluginButton.h"

namespace vibedaw {

class MainContent : public juce::Component,
                     private juce::Timer,
                     public SidebarContainerListener,
                     public BrowserSidebar::Listener,
                     public Project::Listener,
                     public ClipsContent::Listener,
                     private ClipPool::Listener,
                     public ClipEditorWindow::Listener,
                     public juce::DragAndDropContainer {
public:
    MainContent(juce::MidiKeyboardState& keyboardState, MidiManager& midiManager,
                Project& project, AudioEngine& engine);
    ~MainContent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    bool keyPressed(const juce::KeyPress& key) override;
    
    bool handleKeyPress(const juce::KeyPress& key);
    
    void openPluginWindow();
    
    TransportState& getTransportState() { return transportState_; }
    
    void sidebarContainerChanged(SidebarContainer* container) override;
    
    void pluginSelectedForLoad(const juce::String& pluginPath) override;
    void sampleSelected(const juce::File& file) override;
    void presetSelected(const juce::File& file) override;
    
    void activeChannelChanged(int newActiveIndex) override;
    
    void clipCreated(ClipId clipId, Clip* clip) override;
    void clipSelected(ClipId clipId, Clip* clip) override;
    void clipOpened(ClipId clipId, Clip* clip) override;
    void clipEditorClosed(ClipEditorWindow* window) override;
    
private:
    void clipAdded(ClipId, Clip*) override {}
    void clipRemoved(ClipId) override {}
    void clipChanged(ClipId, Clip*) override {}
    void clipWillBeRemoved(ClipId id) override;
    void projectDocumentChanged() override;
    void timerCallback() override;
    void updateStatusLabel();
    void refreshSystemStats();
    void handlePanelFocusHotkey(int panelIndex, double currentTime);
    std::vector<ClipEditorWindow*> openClipEditors_;

    // T07 project document actions. Each guard confirms unsaved work first;
    // cancelled prompts/dialogs change nothing.
    void confirmDiscardThen(std::function<void()> action);
    void actionNewProject();
    void actionOpenProject();
    void actionSaveProject();
    void actionSaveProjectAs();
    void chooseProjectFile(bool forSaving, std::function<void(const juce::File&)> onChosen);
    void closeAllClipEditors();
    void showProjectMenu();
    void runFileAction(int actionId);
    void showError(const juce::String& text);

    void updateLayout();

    MidiManager& midiManager_;
    Project& project_;
    AudioEngine* engine_ = nullptr;
    TransportState& transportState_;
    PluginScanner pluginScanner_;

    std::unique_ptr<TransportComponent> transport_;
    std::unique_ptr<SidebarContainer> leftSidebarContainer_;
    std::unique_ptr<SidebarContainer> rightSidebarContainer_;
    std::unique_ptr<PanelContainer> panelContainer_;

    TimelinePanel* timelinePanel_ = nullptr;
    MixerPanel* mixerPanel_ = nullptr;
    PianoPanel* pianoPanel_ = nullptr;

    PluginButton pluginButton_;
    juce::TextButton fileButton_ { "File" };
    std::unique_ptr<juce::FileChooser> fileDialog_;
    bool fileDialogActive_ = false;
    juce::ComboBox midiDeviceCombo_;
    juce::Label statusLabel_;
    juce::Label midiLabel_;
    juce::Label midiDot_;
    juce::Label cpuLabel_;
    juce::Label ramLabel_;
    juce::TooltipWindow tooltipWindow_ { this, 700 };

    double lastPanelFocusTime_ = 0.0;
    double lastSystemPollTime_ = 0.0;
    int lastFocusedPanelIndex_ = -1;
    static constexpr double doubleTapIntervalMs_ = 400.0;
    
    
    static constexpr int transportBarHeight = 64;
    static constexpr int statusBarHeight = 30;
    static constexpr int workspaceGap = 6;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContent)
};

} // namespace vibedaw
