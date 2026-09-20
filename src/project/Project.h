#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Settings.h"
#include "Track.h"
#include "TrackList.h"
#include "Channel.h"
#include "ChannelList.h"
#include "ClipPool.h"
#include "ProjectDocument.h"
#include "core/AudioEngine.h"
#include "core/MidiManager.h"
#include "core/TransportState.h"
#include <optional>

namespace vibedaw {

class ChannelMixer;
class MidiRecorder;

// Owns the live arrangement models and the project document state. The models
// are replaced in place (clear + repopulate) on project load: ChannelMixer and
// ArrangementPublisher hold references to these sub-models for the lifetime of
// the application, so structural document changes must never swap the objects.
class Project : private ChannelList::Listener,
                private TrackList::Listener,
                private ClipPool::Listener,
                private juce::ChangeListener,
                private TransportListener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void activeChannelChanged(int newActiveIndex) = 0;
        // Document state changed (dirty flag, file path/name); title updates.
        virtual void projectDocumentChanged() {}
    };

    Project();
    ~Project();

    bool initialise(AudioEngine& audioEngine, MidiManager& midiManager);
    void shutdown();

    void setActiveChannel(int index);
    int getActiveChannel() const { return channelList.indexOfChannel(channelList.getChannelById(activeChannelId_)); }
    ChannelId getActiveChannelId() const { return activeChannelId_; }

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    TrackList& getTrackList() { return trackList; }
    const TrackList& getTrackList() const { return trackList; }

    ChannelList& getChannelList() { return channelList; }
    const ChannelList& getChannelList() const { return channelList; }
    MasterBus& getMasterBus() { return channelList.getMasterBus(); }

    ClipPool& getClipPool() { return clipPool; }
    const ClipPool& getClipPool() const { return clipPool; }

    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }

    void loadSettings();
    void saveSettings();

    // Invalid target creates and selects; a stable target replaces without selecting.
    bool loadPlugin(const juce::String& pluginPath, ChannelId target = InvalidChannelId);
    TransportState& getTransportState() { return transport; }
    const TransportState& getTransportState() const { return transport; }
    MidiRecorder& getMidiRecorder() { return *recorder_; }

    // T07 project document: New/Open/Save/Save As. File selection, discard
    // confirmation and error surfacing are the caller's (UI) responsibility.
    // Prepare/commit is two-phase: a failed prepare leaves the session intact.
    juce::String getProjectName() const;
    const juce::File& getProjectFile() const { return projectFile_; }
    bool isDirty() const;
    void markDirty();
    void clearDirty();
    void newProject();
    bool prepareLoad(const juce::File& file, juce::String& error);
    void commitLoad();
    bool saveProject(juce::String& error);
    bool saveProjectAs(const juce::File& file, juce::String& error);

private:
    friend struct ProjectTestAccess;
    std::function<std::unique_ptr<PluginHost>(const juce::String&)> pluginLoader_;
    // Restores one saved plugin identity+state into a host; null means the
    // plugin is unavailable and the channel keeps it as recovery data.
    std::function<std::unique_ptr<PluginHost>(const juce::PluginDescription&, const juce::MemoryBlock&)> pluginRestorer_;
    TransportState transport;
    TrackList trackList;
    ChannelList channelList;
    ClipPool clipPool;
    Settings settings;
    std::unique_ptr<MidiRecorder> recorder_;

    ChannelId activeChannelId_ = InvalidChannelId;

    bool dirty_ = false;
    bool restoring_ = false;
    juce::File projectFile_;
    juce::File pendingProjectFile_;
    std::optional<ProjectDocument::Staged> staged_;

    void applyStaged(const ProjectDocument::Staged& staged);
    // First-run sound: new/empty sessions start with the built-in instrument.
    void seedDefaultInstrument();
    void notifyDocumentChanged();

    // ChannelList::Listener
    void channelAdded(Channel*) override { markDirty(); channelListChanged(); }
    void channelRemoved(int) override { markDirty(); channelListChanged(); }
    void channelChanged(Channel* channel) override {
        markDirty();
        if (channel->getId() == activeChannelId_) notifyActiveChannelChanged(getActiveChannel());
    }
    void channelListChanged() override;
    void mixerChannelsChanged() override { markDirty(); }
    void mixerChannelChanged(MixerChannel*) override { markDirty(); }
    // TrackList::Listener
    void trackAdded(Track* track) override;
    void trackRemoved(int) override { markDirty(); }
    void trackChanged(Track*) override { markDirty(); }
    void trackListChanged() override;
    // Per-track instance edits arrive as change messages.
    void changeListenerCallback(juce::ChangeBroadcaster*) override { markDirty(); }
    // ClipPool::Listener
    void clipAdded(ClipId, Clip*) override { markDirty(); }
    void clipRemoved(ClipId) override { markDirty(); }
    void clipChanged(ClipId, Clip*) override { markDirty(); }
    // TransportListener (mix state only; playback state is not project content)
    void transportTempoChanged(double) override { markDirty(); }
    void transportTimeSignatureChanged(int, int) override { markDirty(); }
    void transportLoopChanged(bool, double, double) override { markDirty(); }
    void transportMetronomeChanged(bool) override { markDirty(); }

    juce::ListenerList<Listener> listeners_;

    void notifyActiveChannelChanged(int newIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Project)
};

} // namespace vibedaw
