#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TransportComponent.h"
#include "panels/PanelContainer.h"
#include "panels/TimelinePanel.h"
#include "panels/MixerPanel.h"
#include "panels/PianoPanel.h"
#include "core/MidiManager.h"
#include "project/Project.h"

namespace vibedaw {

class MainContent : public juce::Component {
public:
    MainContent(juce::MidiKeyboardState& keyboardState, MidiManager& midiManager, Project& project);
    ~MainContent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    bool keyPressed(const juce::KeyPress& key) override;
    
    bool handleKeyPress(const juce::KeyPress& key);
    
    void openPluginWindow();
    
private:
    void updateStatusLabel();
    
    MidiManager& midiManager_;
    Project& project_;
    
    std::unique_ptr<TransportComponent> transport_;
    std::unique_ptr<PanelContainer> panelContainer_;
    
    TimelinePanel* timelinePanel_ = nullptr;
    MixerPanel* mixerPanel_ = nullptr;
    PianoPanel* pianoPanel_ = nullptr;
    
    juce::TextButton pluginButton_;
    juce::ComboBox midiDeviceCombo_;
    juce::Label statusLabel_;
    juce::Label midiLabel_;
    
    static constexpr int topBarHeight = 40;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContent)
};

} // namespace vibedaw
