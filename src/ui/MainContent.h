#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TransportComponent.h"
#include "core/TransportState.h"
#include "core/MidiManager.h"
#include "project/Project.h"
#include "panels/PanelContainer.h"
#include "panels/TimelinePanel.h"
#include "panels/MixerPanel.h"
#include "panels/PianoPanel.h"
#include "sidebar/SidebarContainer.h"
#include "plugins/PluginScanner.h"
#include "sidebar/browser/BrowserSidebar.h"

namespace vibedaw {

class MainContent : public juce::Component,
                     private juce::Timer,
                     public SidebarContainerListener,
                     public BrowserSidebar::Listener,
                     public juce::DragAndDropContainer {
public:
    MainContent(juce::MidiKeyboardState& keyboardState, MidiManager& midiManager, Project& project);
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
    
private:
    void timerCallback() override;
    void updateStatusLabel();
    void handlePanelFocusHotkey(int panelIndex, double currentTime);
    void updateLayout();
    
    MidiManager& midiManager_;
    Project& project_;
    TransportState transportState_;
    PluginScanner pluginScanner_;
    
    std::unique_ptr<TransportComponent> transport_;
    std::unique_ptr<SidebarContainer> leftSidebarContainer_;
    std::unique_ptr<SidebarContainer> rightSidebarContainer_;
    std::unique_ptr<PanelContainer> panelContainer_;
    
    TimelinePanel* timelinePanel_ = nullptr;
    MixerPanel* mixerPanel_ = nullptr;
    PianoPanel* pianoPanel_ = nullptr;
    
    juce::TextButton pluginButton_;
    juce::ComboBox midiDeviceCombo_;
    juce::Label statusLabel_;
    juce::Label midiLabel_;
    
    double lastPanelFocusTime_ = 0.0;
    int lastFocusedPanelIndex_ = -1;
    static constexpr double doubleTapIntervalMs_ = 400.0;
    
    double lastUpdateTime_ = 0.0;
    
    static constexpr int transportBarHeight = 48;
    static constexpr int statusBarHeight = 28;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContent)
};

} // namespace vibedaw
