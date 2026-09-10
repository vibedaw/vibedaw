#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PianoRollEditor.h"
#include "project/ClipInstance.h"
#include "core/TransportState.h"
#include <functional>

namespace vibedaw {

class MidiClip;
class MidiManager;
class Project;

class ClipEditorWindow : public juce::DocumentWindow,
                        public PianoRollEditor::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void clipEditorClosed(ClipEditorWindow* window) = 0;
    };
    
    ClipEditorWindow(MidiClip* clip, ClipId clipId, MidiManager* midiManager = nullptr,
                     TransportState* transport = nullptr,
                     std::function<bool(double, double&)> localBeatProvider = {});
    ~ClipEditorWindow() override;
    
    void setListener(Listener* listener) { listener_ = listener; }
    
    MidiClip* getMidiClip() const { return midiClip_; }
    ClipId getClipId() const { return clipId_; }
    
    void closeButtonPressed() override;
    
    void clipModified(ClipId clipId) override;
    
private:
    MidiClip* midiClip_;
    ClipId clipId_;
    Listener* listener_ = nullptr;
    
    std::unique_ptr<PianoRollEditor> editor_;
    std::unique_ptr<juce::Component> toolbar_;
    std::unique_ptr<juce::Label> clipNameLabel_;
    std::unique_ptr<juce::ComboBox> gridResolutionCombo_;
    juce::ToggleButton followButton_;

    void setupToolbar();
    void updateGridResolution();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipEditorWindow)
};

} // namespace vibedaw