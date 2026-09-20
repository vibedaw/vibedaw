#include "Project.h"
#include "plugins/PluginHost.h"
#include "plugins/builtin/InternalPluginFormat.h"
#include "core/Constants.h"
#include "core/MidiRecorder.h"
#include "utils/Logger.h"

namespace vibedaw {

Project::Project() {
    recorder_ = std::make_unique<MidiRecorder>(*this);
    pluginLoader_ = [](const juce::String& path) -> std::unique_ptr<PluginHost> {
        auto host = std::make_unique<PluginHost>();
        return host->loadPlugin(path) ? std::move(host) : nullptr;
    };
    pluginRestorer_ = [](const juce::PluginDescription& description,
                         const juce::MemoryBlock& state) -> std::unique_ptr<PluginHost> {
        auto host = std::make_unique<PluginHost>();
        if (!host->createFromDescription(description)) return nullptr;
        if (state.getSize() > 0 && host->getPluginInstance() != nullptr)
            host->getPluginInstance()->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        return host;
    };
    channelList.addListener(this);
    trackList.addListener(this);
    clipPool.addListener(this);
    transport.addListener(this);
    LOG_INFO("Project: Created");
}

Project::~Project() {
    recorder_->endSession();
    recorder_.reset();
    transport.removeListener(this);
    clipPool.removeListener(this);
    trackList.removeListener(this);
    channelList.removeListener(this);
    shutdown();
    LOG_INFO("Project: Destroyed");
}

bool Project::initialise(AudioEngine& audioEngine, MidiManager& midiManager) {
    LOG_INFO("Project: Initialising");

    loadSettings();

    if (settings.midiInputDevice.isNotEmpty()) {
        if (!midiManager.connectToDevice(settings.midiInputDevice)) {
            LOG_WARN("Project: Failed to connect to MIDI device: " + settings.midiInputDevice);
        }
    }

    if (channelList.getNumChannels() == 0) {
        // First launch: give new users a playable instrument with no VSTs.
        seedDefaultInstrument();
    }

    if (channelList.getNumChannels() > 0) {
        setActiveChannel(0);
    }

    LOG_INFO("Project: Initialised");
    return true;
}

void Project::shutdown() {
    if (recorder_) recorder_->endSession();
    LOG_INFO("Project: Shutting down");
    saveSettings();
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

bool Project::loadPlugin(const juce::String& pluginPath, ChannelId target) {
    JUCE_ASSERT_MESSAGE_THREAD
    LOG_INFO("Project: Loading plugin: " + pluginPath);
    const bool creating = target == InvalidChannelId;
    if (!creating && recorder_ && recorder_->isSessionActive() && recorder_->getChannelId() == target) {
        recorder_->stop();
        if (recorder_->hasPendingContent()) return false;
    }
    if (pluginPath.trim().isEmpty() ||
        (creating ? channelList.getNumChannels() >= ChannelList::maxChannels
                  : channelList.getChannelById(target) == nullptr)) return false;

    AudioQuiescence::Edit edit;
    auto pluginHost = pluginLoader_(pluginPath);
    if (!pluginHost || !pluginHost->isLoaded() ||
        pluginHost->getProcessor()->getTotalNumInputChannels() > 2 ||
        pluginHost->getProcessor()->getTotalNumOutputChannels() > 2) {
        LOG_ERROR("Project: Failed to load plugin");
        return false;
    }

    // Re-resolve after loading: hosted code may have dispatched message-thread work.
    auto* channel = creating
        ? channelList.addChannel(pluginHost->getPluginName(), Channel::Type::Instrument)
        : channelList.getChannelById(target);
    if (!channel) return false;
    channel->setPlugin(std::move(pluginHost));

    if (creating) {
        // The built-in instrument is never persisted as a scan path.
        if (!InternalPluginFormat::claimsIdentifier(pluginPath))
            settings.pluginPath = pluginPath;
        setActiveChannel(channelList.indexOfChannel(channel));
    }

    LOG_INFO("Project: Plugin loaded successfully");
    return true;
}

void Project::setActiveChannel(int index) {
    auto* channel = channelList.getChannel(index);
    activeChannelId_ = channel ? channel->getId() : InvalidChannelId;
    notifyActiveChannelChanged(getActiveChannel());
    LOG_INFO("Project: Active channel set to " + juce::String(index));
}

void Project::channelListChanged() {
    if (!channelList.getChannelById(activeChannelId_)) activeChannelId_ = InvalidChannelId;
    notifyActiveChannelChanged(getActiveChannel());
}

void Project::trackAdded(Track* track) {
    if (track != nullptr) track->addChangeListener(this);
    markDirty();
}

void Project::trackListChanged() {
    // Instance edits broadcast through each Track's ChangeBroadcaster; keep the
    // registration current through structural rebuilds. addChangeListener is
    // idempotent, so re-registration is safe.
    for (const auto& track : trackList.getTracks()) track->addChangeListener(this);
    markDirty();
}

void Project::addListener(Listener* listener) {
    listeners_.add(listener);
}

void Project::removeListener(Listener* listener) {
    listeners_.remove(listener);
}

void Project::notifyActiveChannelChanged(int newIndex) {
    listeners_.call([newIndex](Listener& l) { l.activeChannelChanged(newIndex); });
}

void Project::notifyDocumentChanged() {
    listeners_.call([](Listener& l) { l.projectDocumentChanged(); });
}

juce::String Project::getProjectName() const {
    return projectFile_.getFullPathName().isEmpty()
        ? juce::String("Untitled")
        : projectFile_.getFileNameWithoutExtension();
}

void Project::markDirty() {
    if (restoring_ || dirty_) return;
    dirty_ = true;
    notifyDocumentChanged();
}

bool Project::isDirty() const {
    return dirty_ || (recorder_ && recorder_->hasPendingContent());
}

void Project::clearDirty() {
    if (!dirty_) return;
    dirty_ = false;
    notifyDocumentChanged();
}

void Project::newProject() {
    JUCE_ASSERT_MESSAGE_THREAD
    recorder_->endSession();
    if (recorder_->hasPendingContent()) return;
    applyStaged(ProjectDocument::Staged{});
    projectFile_ = juce::File();
    seedDefaultInstrument();
    dirty_ = false;
    notifyDocumentChanged();
    LOG_INFO("Project: New project created");
}

void Project::seedDefaultInstrument() {
    JUCE_ASSERT_MESSAGE_THREAD
    if (channelList.getNumChannels() > 0) return;
    if (!loadPlugin(InternalPluginFormat::identifier))
        LOG_WARN("Project: Built-in instrument unavailable; session starts silent");
}

bool Project::prepareLoad(const juce::File& file, juce::String& error) {
    JUCE_ASSERT_MESSAGE_THREAD
    staged_.reset();
    pendingProjectFile_ = juce::File();
    if (!file.existsAsFile()) {
        error = "Project file not found: " + file.getFullPathName();
        return false;
    }
    const auto json = file.loadFileAsString();
    auto staged = std::make_optional<ProjectDocument::Staged>();
    if (!ProjectDocument::stage(json, *staged, error)) {
        staged_.reset();
        return false;
    }
    staged_ = std::move(staged);
    pendingProjectFile_ = file;
    return true;
}

void Project::commitLoad() {
    JUCE_ASSERT_MESSAGE_THREAD
    if (!staged_) return;
    recorder_->endSession();
    if (recorder_->hasPendingContent()) return;
    applyStaged(*staged_);
    projectFile_ = pendingProjectFile_;
    staged_.reset();
    pendingProjectFile_ = juce::File();
    dirty_ = false;
    notifyDocumentChanged();
    LOG_INFO("Project: Loaded " + projectFile_.getFullPathName());
}

bool Project::saveProject(juce::String& error) {
    JUCE_ASSERT_MESSAGE_THREAD
    return saveProjectAs(projectFile_, error);
}

bool Project::saveProjectAs(const juce::File& file, juce::String& error) {
    JUCE_ASSERT_MESSAGE_THREAD
    recorder_->stop();
    if (recorder_->hasPendingContent()) {
        error = "Recording could not be finalized. Resolve the recorder error before saving.";
        return false;
    }
    if (!ProjectDocument::writeToFile(*this, file, error)) {
        LOG_ERROR("Project: Save failed: " + error);
        return false;
    }
    projectFile_ = file;
    dirty_ = false;
    notifyDocumentChanged();
    LOG_INFO("Project: Saved " + file.getFullPathName());
    return true;
}

void Project::applyStaged(const ProjectDocument::Staged& staged) {
    JUCE_ASSERT_MESSAGE_THREAD

    // Stop and flush the old render state first: stopGeneration advances and a
    // sticky panic forces full cleanup before anything plays again.
    transport.stop();

    {
        AudioQuiescence::Edit edit;
        restoring_ = true; // Restore-time notifications must not mark the document dirty.

        // Dependency order: placements reference clips/instruments, instruments
        // reference independent mixer destinations.
        trackList.clearTracks();
        clipPool.clearClips();
        channelList.clearChannels();
        channelList.clearMixerChannels();

        for (const auto& data : staged.mixerChannels) {
            auto* channel = channelList.restoreMixerChannel(data.id, data.name);
            if (channel == nullptr) {
                LOG_ERROR("Project: Failed to restore mixer channel ID " + juce::String(data.id));
                continue;
            }
            channel->setVolume(data.volume);
            channel->setPan(data.pan);
            channel->setMuted(data.muted);
            channel->setSolo(data.solo);
            channel->setColour(data.colour);
            // Consume restore-time notifications while dirty tracking is suspended.
            channel->dispatchPendingMessages();
        }

        for (const auto& data : staged.channels) {
            auto* channel = channelList.restoreChannel(data.id, data.name, data.type);
            if (channel == nullptr) {
                LOG_ERROR("Project: Failed to restore channel ID " + juce::String(data.id));
                continue;
            }
            channel->setVolume(data.volume);
            channel->setPan(data.pan);
            channel->setMuted(data.muted);
            channel->setSolo(data.solo);
            channel->setColour(data.colour);
            channelList.setChannelMixerDestination(data.id, data.mixerTrackId);
            if (data.type == Channel::Type::Sampler && !data.sampleFile.getFullPathName().isEmpty())
                channel->setSampleFile(data.sampleFile);
            if (data.plugin) {
                auto host = pluginRestorer_(data.plugin->description, data.plugin->state);
                if (host) {
                    channel->setPlugin(std::move(host));
                } else {
                    channel->setPlugin(nullptr);
                    Channel::MissingPlugin missing;
                    missing.description = data.plugin->description;
                    missing.state = data.plugin->state;
                    channel->setMissingPlugin(std::move(missing));
                    LOG_WARN("Project: Plugin unavailable, channel kept as unresolved: " +
                             data.plugin->description.name);
                }
            }
        }

        for (const auto& data : staged.clips) {
            std::unique_ptr<Clip> clip;
            switch (data.type) {
                case Clip::Type::Midi: {
                    auto midi = std::make_unique<MidiClip>(data.startBeats, data.durationBeats);
                    midi->setLoopEnabled(data.loopEnabled);
                    midi->replaceContent(data.notes, data.expressionEvents);
                    clip = std::move(midi);
                    break;
                }
                case Clip::Type::Audio: {
                    auto audio = std::make_unique<AudioClip>(data.startBeats, data.durationBeats);
                    audio->setAudioFile(data.audioFile);
                    clip = std::move(audio);
                    break;
                }
                case Clip::Type::Pattern: {
                    auto pattern = std::make_unique<PatternClip>(data.startBeats, data.durationBeats);
                    pattern->setPatternLength(data.patternLength);
                    pattern->setLoopCount(data.loopCount);
                    clip = std::move(pattern);
                    break;
                }
            }
            clip->setName(data.name);
            clip->setColour(data.colour);
            if (clipPool.restoreClip(data.id, std::move(clip)) == InvalidClipId)
                LOG_ERROR("Project: Failed to restore clip ID " + juce::String(data.id));
        }

        for (const auto& data : staged.tracks) {
            auto* track = trackList.restoreTrack(data.id, data.name);
            if (track == nullptr) {
                LOG_ERROR("Project: Failed to restore track " + data.id);
                continue;
            }
            track->setHeight(data.height);
            track->setColour(data.colour);
for (const auto& instanceData : data.instances) {
                auto instance = std::make_unique<ClipInstance>(instanceData.clipId, instanceData.channelId,
                    instanceData.startBeats, instanceData.durationBeats, instanceData.id);
                if (instanceData.muted) instance->setMuted(true);
                track->addClipInstance(std::move(instance));
            }
        }

        auto& master = getMasterBus();
        master.setGain(staged.masterGain);
        master.setMuted(staged.masterMuted);

        transport.setTempo(staged.transport.tempo);
        transport.setTimeSignature(staged.transport.numerator, staged.transport.denominator);
        if (staged.transport.loop.exists) {
            transport.setLoopRegion(staged.transport.loop.startBeats, staged.transport.loop.endBeats);
            transport.setLoopEnabled(staged.transport.loop.enabled);
        } else {
            transport.clearLoop();
        }
        transport.setMetronomeEnabled(staged.transport.metronome);

        if (channelList.getNumChannels() > 0) setActiveChannel(0);

        restoring_ = false;
    }

    clearDirty();
    notifyDocumentChanged();
}

} // namespace vibedaw
