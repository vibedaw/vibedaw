#pragma once

#include "ui/components/RecordingControls.h"

// Include after CHECK, Probe, and OfflineInstrument in offline_tests.cpp.
// No peers, popovers, audio devices, or native prompts are created here.
namespace recording_ui_test {

template <typename T>
T& control(RecordingControls& view, const char* id) {
    auto* result = dynamic_cast<T*>(view.findChildWithID(id));
    CHECK(result);
    return *result;
}

inline void click(RecordingControls& view, const char* id) {
    auto& button = control<juce::Button>(view, id);
    CHECK(button.isEnabled() && button.isVisible());
    const auto action = button.onClick;
    CHECK(action);
    action(); // The action may synchronously destroy the view and button.
}

struct Fixture {
    Probe probe;
    Project project;
    MidiRecorder& recorder = project.getMidiRecorder();
    ChannelMixer mixer{project.getChannelList(), project.getTrackList(), project.getClipPool(),
                       project.getTransportState(), &recorder};
    Channel* channel = nullptr;
    juce::AudioBuffer<float> audio{2, 240};
    juce::MidiBuffer input;

    Fixture() {
        channel = addInstrument("First instrument");
        project.setActiveChannel(project.getChannelList().indexOfChannel(channel));
        mixer.prepareToPlay(480, 240); // 240 samples per quarter-note beat.
        mixer.setActiveChannel(project.getChannelList().indexOfChannel(channel));
        input.ensureSize(32768);
    }
    ~Fixture() { recorder.endSession(); mixer.releaseResources(); }
    Channel* addInstrument(const juce::String& name) {
        auto* result = project.getChannelList().addChannel(name);
        result->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
        return result;
    }
    ClipId addSource(const juce::String& name = "Source") {
        auto clip = std::make_unique<MidiClip>(0, 4);
        clip->setName(name);
        return project.getClipPool().addClip(std::move(clip));
    }
    void renderNote(int pitch = 64) {
        input.clear();
        input.addEvent(juce::MidiMessage::noteOn(1, pitch, static_cast<juce::uint8>(99)), 30);
        input.addEvent(juce::MidiMessage::noteOff(1, pitch), 90);
        CHECK(AudioQuiescence::instance().enter());
        mixer.processBlock(audio, input);
        AudioQuiescence::instance().leave();
    }
};

struct PoolHook : ClipPool::Listener {
    explicit PoolHook(ClipPool& value) : pool(value) { pool.addListener(this); }
    ~PoolHook() override { pool.removeListener(this); }
    ClipPool& pool;
    std::function<void()> added, removing;
    void clipAdded(ClipId, Clip*) override { const auto call = added; if (call) call(); }
    void clipWillBeRemoved(ClipId) override { const auto call = removing; if (call) call(); }
    void clipRemoved(ClipId) override {}
    void clipChanged(ClipId, Clip*) override {}
};

struct TransportHook : TransportListener {
    explicit TransportHook(TransportState& value) : transport(value) { transport.addListener(this); }
    ~TransportHook() override { transport.removeListener(this); }
    TransportState& transport;
    std::function<void()> position;
    void transportPositionChanged(double) override { const auto call = position; if (call) call(); }
};

} // namespace recording_ui_test

inline void recordingUiTests() {
    using namespace recording_ui_test;
    {
        Fixture f;
        RecordingControls setup(f.project, false, InvalidClipId, true);
        CHECK(control<juce::Label>(setup, "recordingTitle").getText() == "Record MIDI");
        CHECK(!setup.findChildWithID("recordingMode")); // No mode dropdown.
        for (const auto* id : {"recordingPlay", "recordingStop", "undoRecording"})
            CHECK(!control<juce::Button>(setup, id).isVisible());
        CHECK(!control<juce::Label>(setup, "recordingStatus").isVisible());
        CHECK(!setup.findChildWithID("recordingTargetInfo") && !setup.findChildWithID("recordingSharedWarning"));
        const char* modes[] = {"Continuous", "Takes", "Replace", "Overdub"};
        for (const auto* mode : modes) {
            const auto id = "recordingMode" + juce::String(mode);
            auto& button = control<juce::Button>(setup, id.toRawUTF8());
            CHECK(button.getName() == mode && button.getTooltip() == mode);
            click(setup, id.toRawUTF8());
            for (const auto* other : modes)
                CHECK(control<juce::Button>(setup, ("recordingMode" + juce::String(other)).toRawUTF8())
                    .getToggleState() == (juce::String(mode) == other));
            CHECK(control<juce::Label>(setup, "recordingModeHint").getText().startsWith(juce::String(mode) + ":"));
            CHECK(control<juce::TextEditor>(setup, "recordingLoopLength").isEnabled() == (juce::String(mode) != "Continuous"));
        }
        for (const auto icon : {IconId::continuous, IconId::takes, IconId::overdub}) CHECK(!Icons::path(icon).isEmpty());
        auto& record = control<juce::TextButton>(setup, "recordingRecord");
        CHECK(record.findColour(juce::TextButton::buttonColourId) == theme::danger);
        CHECK(record.getX() == 16 && record.getRight() == setup.getWidth() - 16);
        CHECK(record.getBottom() == setup.getHeight() - 16 && record.getHeight() == 42);
        CHECK(control<juce::ComboBox>(setup, "recordingTake").getBottom() < record.getY());
        CHECK(control<juce::Button>(setup, "endRecordingSession").getBottom() < record.getY());
        CHECK(control<juce::Label>(setup, "recordingModeHint").getBottom()
            <= control<juce::TextEditor>(setup, "recordingLoopLength").getY());
        juce::Image image(juce::Image::ARGB, setup.getWidth(), setup.getHeight(), true);
        juce::Graphics graphics(image);
        setup.paintEntireComponent(graphics, true);
        click(setup, "recordingRecord");
        CHECK(f.recorder.getMode() == MidiRecorder::Mode::Overdub);
        CHECK(record.getButtonText() == "Disarm recording");
        for (const auto* mode : modes)
            CHECK(!control<juce::Button>(setup, ("recordingMode" + juce::String(mode)).toRawUTF8()).isEnabled());
        click(setup, "recordingRecord");
        CHECK(record.getButtonText() == "Record" && f.recorder.getPlaybackTransport().isPlaying());
    }
    {
        Fixture f;
        const auto source = f.addSource("Shared source");
        auto* second = f.addInstrument("Placement instrument");
        auto* firstTrack = f.project.getTrackList().addTrack("Other track");
        firstTrack->addClipInstance(std::make_unique<ClipInstance>(source, f.channel->getId(), 8, 4));
        auto* chosenTrack = f.project.getTrackList().addTrack("Chosen track");
        chosenTrack->addClipInstance(std::make_unique<ClipInstance>(source, second->getId(), 8, 4));
        f.addSource("Unplaced source must not appear in song setup");

        RecordingControls toolbar(f.project, false);
        auto setup = toolbar.createSetup();
        auto& targets = control<juce::ComboBox>(*setup, "recordingSource");
        CHECK(targets.getSelectedId() == 1 && targets.getNumItems() == 3);
        CHECK(targets.getItemText(0).contains("new track"));
        CHECK(control<juce::Button>(*setup, "recordingModeContinuous").getToggleState());
        targets.setSelectedId(3, juce::sendNotificationSync);
        f.project.getTrackList().moveTrack(1, 0);
        f.project.getChannelList().moveChannel(1, 0);
        f.project.setActiveChannel(f.project.getChannelList().indexOfChannel(f.channel));
        f.project.getTransportState().setPositionInBeats(9);
        setup->refresh();
        const auto detail = control<juce::ComboBox>(*setup, "recordingSource").getTooltip();
        CHECK(detail.contains("Chosen track") && detail.contains("Shared source"));
        CHECK(detail.contains("Placement instrument") && detail.contains("8.000") && detail.contains("9.000"));
        CHECK(!control<juce::ComboBox>(*setup, "recordingInstrument").isEnabled());
        CHECK(control<juce::ComboBox>(*setup, "recordingSourcePolicy").getNumItems() == 1);
        click(*setup, "recordingRecord");
        CHECK(f.recorder.isRecording() && f.recorder.getTargetClipId() == source);
        CHECK(f.recorder.getChannelId() == second->getId());
        f.renderNote();
        toolbar.refresh();
        click(toolbar, "recordingStop");
        auto* clip = dynamic_cast<MidiClip*>(f.project.getClipPool().getClip(source));
        CHECK(clip && clip->getNumNotes() == 1);
        CHECK(std::abs(clip->getNotes()[0].getStartTime() - 1.125) < 1.0e-9);
        CHECK(f.project.getTrackList().getNumTracks() == 2);
        CHECK(firstTrack->getClipInstance(0)->getDuration() == 4);
    }
    {
        Fixture f;
        const auto source = f.addSource();
        auto* track = f.project.getTrackList().addTrack("Removed placement");
        track->addClipInstance(std::make_unique<ClipInstance>(source, f.channel->getId(), 0, 4));
        RecordingControls setup(f.project, false, InvalidClipId, true);
        control<juce::ComboBox>(setup, "recordingSource").setSelectedId(2, juce::sendNotificationSync);
        track->removeClipInstance(0);
        click(setup, "recordingRecord");
        CHECK(!f.recorder.isSessionActive());
        CHECK(control<juce::Label>(setup, "recordingStatus").getText().contains("placement was removed"));
        CHECK(f.project.getClipPool().getNumClips() == 1);
    }
    {
        Fixture f;
        RecordingControls toolbar(f.project, true);
        auto setup = toolbar.createSetup();
        CHECK(control<juce::Button>(*setup, "recordingModeOverdub").getToggleState());
        CHECK(!control<juce::TextButton>(*setup, "undoRecording").isEnabled());
        click(*setup, "recordingRecord");
        f.renderNote();
        click(*setup, "recordingRecord");
        CHECK(!f.recorder.isRecording() && f.recorder.getPlaybackTransport().isPlaying());
        CHECK(f.recorder.canUndoLastRecording());
        click(*setup, "endRecordingSession");
        CHECK(!f.recorder.isSessionActive());
        toolbar.refresh();
        CHECK(control<juce::TextButton>(toolbar, "undoRecording").isEnabled());
        click(toolbar, "undoRecording");
        CHECK(!f.recorder.canUndoLastRecording() && f.project.getClipPool().getNumClips() == 0);
    }
    {
        Fixture f;
        const auto id = f.addSource();
        RecordingControls toolbar(f.project, true, id);
        auto setup = toolbar.createSetup();
        click(*setup, "recordingRecord");
        f.renderNote();
        click(*setup, "recordingRecord");
        click(*setup, "endRecordingSession");
        auto* source = dynamic_cast<MidiClip*>(f.project.getClipPool().getClip(id));
        CHECK(source && source->addNote(Note(80, 2, 0.5)));
        toolbar.refresh();
        if (control<juce::TextButton>(toolbar, "undoRecording").isEnabled()) {
            click(toolbar, "undoRecording");
            CHECK(control<juce::Label>(toolbar, "recordingStatus").getText().contains("changed"));
        }
        CHECK(source->getNumNotes() == 2); // Undo never overwrites a subsequent edit.
    }
    for (const bool endFirst : {false, true}) {
        Fixture f;
        auto view = std::make_unique<RecordingControls>(f.project, true);
        auto setup = view->createSetup();
        click(*setup, "recordingRecord");
        f.renderNote();
        view->refresh();
        click(*view, "recordingStop");
        setup->refresh();
        if (endFirst) click(*setup, "endRecordingSession");
        PoolHook hook(f.project.getClipPool());
        hook.removing = [&] { view->closeOwnedSession(); view.reset(); };
        // Mirrors MainContent closing an editor synchronously in clipWillBeRemoved.
        const juce::Component::SafePointer<RecordingControls> safe(view.get());
        click(*view, "undoRecording");
        CHECK(!safe && !view);
        CHECK(f.project.getClipPool().getNumClips() == 0);
    }
    {
        Fixture f;
        const auto id = f.addSource();
        auto view = std::make_unique<RecordingControls>(f.project, true, id, true);
        control<juce::ComboBox>(*view, "recordingSourcePolicy").setSelectedId(2, juce::sendNotificationSync);
        PoolHook hook(f.project.getClipPool());
        hook.added = [&] { view->closeOwnedSession(); view.reset(); };
        click(*view, "recordingRecord");
        CHECK(!view && !f.recorder.isSessionActive());
        CHECK(f.project.getClipPool().getNumClips() == 2); // Explicit clone remains recoverable.
    }
    {
        Fixture f;
        auto view = std::make_unique<RecordingControls>(f.project, true, InvalidClipId, true);
        click(*view, "recordingRecord");
        f.renderNote();
        PoolHook hook(f.project.getClipPool());
        hook.added = [&] { view->closeOwnedSession(); view.reset(); };
        click(*view, "recordingRecord");
        CHECK(!view && !f.recorder.isSessionActive());
        CHECK(f.project.getClipPool().getNumClips() == 1);
    }
    {
        Fixture f;
        auto view = std::make_unique<RecordingControls>(f.project, true, InvalidClipId, true);
        TransportHook hook(f.project.getTransportState());
        hook.position = [&] {
            if (view) { view->closeOwnedSession(); view.reset(); }
        };
        click(*view, "recordingRecord"); // start() parks the song cursor and notifies.
        CHECK(!view && !f.recorder.isSessionActive());
    }
    {
        Fixture f;
        auto owner = std::make_unique<RecordingControls>(f.project, true, InvalidClipId, true);
        click(*owner, "recordingRecord");
        f.renderNote();
        PoolHook hook(f.project.getClipPool());
        hook.added = [&] { owner->closeOwnedSession(); owner.reset(); };
        {
            // The main transport can issue a command that closes a different view.
            RecordingControls::ScopedRecorderCommand command(f.recorder);
            f.recorder.stop();
        }
        CHECK(!owner && !f.recorder.isSessionActive());
        CHECK(f.project.getClipPool().getNumClips() == 1);
    }
    {
        Fixture f;
        auto view = std::make_unique<RecordingControls>(f.project, true, InvalidClipId, true);
        click(*view, "recordingModeTakes");
        control<juce::TextEditor>(*view, "recordingLoopLength").setText("1", false);
        click(*view, "recordingRecord");
        f.renderNote(60);
        f.renderNote(62);
        click(*view, "recordingRecord");
        CHECK(f.recorder.getTakeCount() == 2);
        TransportHook hook(f.recorder.getPlaybackTransport());
        hook.position = [&] {
            if (view) { view->closeOwnedSession(); view.reset(); }
        };
        auto& takes = control<juce::ComboBox>(*view, "recordingTake");
        CHECK(takes.isEnabled());
        const auto position = f.recorder.getPlaybackTransport().getPositionInBeats();
        takes.setSelectedId(1, juce::dontSendNotification);
        const auto select = takes.onChange;
        select();
        // Selecting a take changes the playback source, not the transport cursor.
        CHECK(view && f.recorder.isSessionActive());
        CHECK(f.recorder.getPlaybackTransport().getPositionInBeats() == position);
        CHECK(f.project.getClipPool().getNumClips() == 2);
    }
}
