#pragma once

#include "ui/DawWindow.h"
#include "PianoRollEditor.h"
#include "project/ClipInstance.h"
#include "project/ClipPool.h"
#include "core/TransportState.h"
#include <functional>

namespace vibedaw {

class MidiClip;
class MidiManager;
class Project;
class RecordingControls;

class ClipEditorWindow : public DawWindow,
                        public PianoRollEditor::Listener,
                        private juce::MultiTimer,
                        private ClipPool::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void clipEditorClosed(ClipEditorWindow* window) = 0;
    };
    
    ClipEditorWindow(MidiClip* clip, ClipId clipId, MidiManager* midiManager = nullptr,
                     TransportState* transport = nullptr,
                     std::function<bool(double, double&)> localBeatProvider = {},
                     bool addToDesktop = true, Project* project = nullptr);
    ~ClipEditorWindow() override;
    
    void setListener(Listener* listener) { listener_ = listener; }
    
    MidiClip* getMidiClip() const { return midiClip_; }
    ClipId getClipId() const { return clipId_; }
    
    void closeButtonPressed() override;
    void resized() override;
    
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
    Project* project_ = nullptr;
    TransportState* songTransport_ = nullptr;
    std::function<bool(double, double&)> songBeatProvider_;
    ClipId songClipId_ = InvalidClipId;
    bool usingClipTransport_ = false;
    std::unique_ptr<RecordingControls> recordingControls_;
    juce::Component recordingEditShield_;

    void updateGridResolution();
    void timerCallback(int timerId) override;
    void clipAdded(ClipId, Clip*) override {}
    void clipRemoved(ClipId) override {}
    void clipChanged(ClipId, Clip*) override {}
    void clipWillBeRemoved(ClipId id) override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipEditorWindow)
};

} // namespace vibedaw
