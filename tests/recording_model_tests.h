#pragma once

#include "project/Project.h"
#include <limits>

// Include after the offline suite's CHECK definition; no fixture/test-access
// dependencies. The caller supplies the suite's message-thread environment.
inline void expressiveClipTests() {
    using namespace vibedaw;
    struct Listener : Clip::Listener {
        int changes = 0, invalidations = 0;
        void clipChanged() override { ++changes; }
        void notesInvalidated() override { ++invalidations; }
    } listener;
    MidiClip clip(8.0, 4.0);
    clip.addListener(&listener);
    const auto* borrowedNote = clip.addNote(Note(60, 0.0, 1.0));
    CHECK(borrowedNote != nullptr);
    listener.changes = listener.invalidations = 0;
    CHECK(clip.addExpressionEvent({2.0, 0xbf, 64, 0}));
    CHECK(clip.addExpressionEvent({0.0, 0xb0, 64, 127}));
    CHECK(clip.addExpressionEvent({0.0, 0xef, 127, 127}));
    CHECK(clip.addExpressionEvent({0.0, 0xe0, 0, 0}));
    CHECK(listener.changes == 4 && listener.invalidations == 0);
    CHECK(&clip.getNotes()[0] == borrowedNote);
    const auto& ordered = clip.getExpressionEvents();
    CHECK(ordered.size() == 4 && ordered[0].status == 0xb0 && ordered[1].status == 0xef);
    CHECK(ordered[2].status == 0xe0 && ordered[3].beat == 2.0);
    CHECK(ordered[0].getChannel() == 1 && ordered[1].getChannel() == 16);

    const MidiExpressionEvent invalidEvents[] = {
        {-1.0, 0xb0, 0, 0}, {std::numeric_limits<double>::infinity(), 0xb0, 0, 0},
        {std::numeric_limits<double>::quiet_NaN(), 0xb0, 0, 0},
        {TransportState::maxPositionBeats + 1.0, 0xb0, 0, 0},
        {0.0, -1, 0, 0}, {0.0, 0x1b0, 0, 0}, {0.0, 0x90, 60, 100},
        {0.0, 0xc0, 0, 0}, {0.0, 0xd0, 0, 0}, {0.0, 0xf0, 0, 0},
        {0.0, 0xb0, -1, 0}, {0.0, 0xb0, 128, 0},
        {0.0, 0xe0, 0, -1}, {0.0, 0xe0, 0, 128}
    };
    for (const auto& invalid : invalidEvents) {
        CHECK(!clip.addExpressionEvent(invalid));
        CHECK(!clip.setExpressionEvents({invalid}));
        CHECK(!clip.replaceContent({}, {invalid}));
    }
    CHECK(listener.changes == 4 && listener.invalidations == 0);
    CHECK(clip.getNumNotes() == 1 && clip.getExpressionEvents().size() == 4);
    CHECK(!clip.replaceContent({Note(60, -1.0, 1.0)}, {}));
    CHECK(!clip.replaceContent({Note(60, 0.0, 0.0)}, {}));
    CHECK(!clip.replaceContent({Note(60, std::numeric_limits<double>::infinity(), 1.0)}, {}));
    CHECK(listener.changes == 4 && listener.invalidations == 0);

    Note recorded(72, 1.0, 0.5, 117);
    recorded.setChannel(16);
    CHECK(clip.replaceContent({recorded}, {{3.0, 0xbf, 64, 0}, {0.0, 0xbf, 64, 127},
                                         {0.0, 0xef, 0, 64}, {0.0, 0xbf, 1, 19}}));
    CHECK(listener.changes == 5 && listener.invalidations == 1);
    CHECK(clip.getNotes()[0].getPitch() == 72 && clip.getNotes()[0].getChannel() == 16);
    CHECK(clip.getExpressionEvents()[1].status == 0xef && clip.getExpressionEvents()[2].data1 == 1);
    CHECK(clip.getStartTime() == 8.0 && clip.getDuration() == 4.0);
    auto cloned = clip.clone();
    auto* clonedMidi = dynamic_cast<MidiClip*>(cloned.get());
    CHECK(clonedMidi && clonedMidi->getNumNotes() == 1 && clonedMidi->getExpressionEvents().size() == 4);
    CHECK(clonedMidi->getExpressionEvents()[1].data2 == 64);
    clip.clearExpressionEvents();
    clip.clearExpressionEvents();
    CHECK(listener.changes == 6 && listener.invalidations == 1 && clip.getNumNotes() == 1);
    CHECK(clonedMidi->getExpressionEvents().size() == 4);
    CHECK(clip.setExpressionEvents({{2.0, 0xb0, 1, 2}, {0.0, 0xe0, 0, 64}, {0.0, 0xb0, 1, 3}}));
    CHECK(clip.getExpressionEvents()[0].status == 0xe0 && clip.getExpressionEvents()[1].data2 == 3);
    CHECK(clip.addExpressionEvent(clip.getExpressionEvents()[0]));
    CHECK(clip.getExpressionEvents()[2].status == 0xe0);
    clip.clearNotes();
    CHECK(clip.getExpressionEvents().size() == 4);
    CHECK(clip.replaceContent({}, {}) && clip.getExpressionEvents().empty() && clip.getNumNotes() == 0);
    clip.removeListener(&listener);

    // All channels and both raw bend extremes are legal; trimmed-out events are retained.
    for (int channel = 0; channel < 16; ++channel) {
        CHECK(clip.addExpressionEvent({0.0, 0xb0 + channel, 127, 127}));
        CHECK(clip.addExpressionEvent({0.0, 0xe0 + channel, 0, 0}));
        CHECK(clip.addExpressionEvent({TransportState::maxPositionBeats, 0xe0 + channel, 127, 127}));
    }
    std::vector<MidiExpressionEvent> fullEvents(MidiClip::maxExpressionEvents);
    CHECK(clip.setExpressionEvents(fullEvents));
    CHECK(!clip.addExpressionEvent({}));
    fullEvents.push_back({});
    CHECK(!clip.setExpressionEvents(fullEvents));
    CHECK(!clip.replaceContent({}, fullEvents));
    CHECK(clip.getExpressionEvents().size() == MidiClip::maxExpressionEvents);
    std::vector<Note> fullNotes(MidiClip::maxNotes);
    CHECK(clip.replaceContent(fullNotes, {}));
    CHECK(clip.addNote(Note()) == nullptr);
    fullNotes.push_back(Note());
    CHECK(!clip.replaceContent(fullNotes, {}));
    CHECK(clip.getNotes().size() == MidiClip::maxNotes);

    Project project;
    const auto id = project.getClipPool().addClip(std::move(cloned));
    auto* source = dynamic_cast<MidiClip*>(project.getClipPool().getClip(id));
    CHECK(source != nullptr);
    project.clearDirty();
    CHECK(!source->addExpressionEvent(invalidEvents[0]) && !project.isDirty());
    CHECK(source->addExpressionEvent({3.0, 0xef, 127, 127}) && project.isDirty());
    project.clearDirty();
    CHECK(source->setExpressionEvents(source->getExpressionEvents()) && project.isDirty());
    project.clearDirty();
    CHECK(source->replaceContent(source->getNotes(), source->getExpressionEvents()) && project.isDirty());

    const auto json = ProjectDocument::serialize(project);
    ProjectDocument::Staged staged;
    juce::String error;
    CHECK(ProjectDocument::stage(json, staged, error) && error.isEmpty());
    CHECK(staged.version == 3 && staged.clips.size() == 1);
    const auto& data = staged.clips[0];
    CHECK(data.id == id && data.notes.size() == 1 && data.notes[0].getChannel() == 16);
    CHECK(data.expressionEvents.size() == source->getExpressionEvents().size());
    for (size_t i = 0; i < data.expressionEvents.size(); ++i) {
        const auto& a = data.expressionEvents[i];
        const auto& b = source->getExpressionEvents()[i];
        CHECK(a.beat == b.beat && a.status == b.status && a.data1 == b.data1 && a.data2 == b.data2);
    }
    // Exercise staged-data application without depending on Project.cpp wiring.
    Project restored;
    auto restoredSource = std::make_unique<MidiClip>(data.startBeats, data.durationBeats);
    restoredSource->setName(data.name);
    restoredSource->setColour(data.colour);
    restoredSource->setLoopEnabled(data.loopEnabled);
    CHECK(restoredSource->replaceContent(data.notes, data.expressionEvents));
    CHECK(restored.getClipPool().restoreClip(id, std::move(restoredSource)) == id);
    CHECK(ProjectDocument::serialize(restored) == json);

    const auto rejected = [&](const juce::var& document) {
        ProjectDocument::Staged previous = staged;
        CHECK(!ProjectDocument::stage(juce::JSON::toString(document), previous, error));
        CHECK(error.isNotEmpty());
        CHECK(previous.version == staged.version && previous.clips.size() == 1);
        CHECK(previous.clips[0].id == id && previous.clips[0].notes[0].getPitch() == 72);
        CHECK(previous.clips[0].expressionEvents.size() == data.expressionEvents.size());
        CHECK(previous.clips[0].expressionEvents[1].data2 == 64);
    };
    const auto badEvent = [&](const char* field, const juce::var& value) {
        auto document = juce::JSON::parse(json);
        document["clips"][0]["expressionEvents"][0].getDynamicObject()->setProperty(field, value);
        rejected(document);
    };
    badEvent("beat", -1.0);
    badEvent("beat", TransportState::maxPositionBeats + 1.0);
    badEvent("beat", "not a beat");
    badEvent("status", -1);
    badEvent("status", 0x1b0);
    badEvent("status", 0x90);
    badEvent("status", 0xc0);
    badEvent("status", 0xf0);
    badEvent("status", 176.5);
    badEvent("status", static_cast<juce::int64>(0x1000000b0LL));
    badEvent("data1", -1);
    badEvent("data1", 128);
    badEvent("data2", -1);
    badEvent("data2", 128);
    badEvent("data2", 0.5);
    badEvent("data2", true);
    badEvent("beat", 1.0); // Descending relative to the next equal-beat event.
    for (const auto* field : {"beat", "status", "data1", "data2"}) {
        auto document = juce::JSON::parse(json);
        document["clips"][0]["expressionEvents"][0].getDynamicObject()->removeProperty(field);
        rejected(document);
    }
    for (const auto& invalid : {juce::var(), juce::var(42), juce::var("events")}) {
        auto document = juce::JSON::parse(json);
        document["clips"][0].getDynamicObject()->setProperty("expressionEvents", invalid);
        rejected(document);
    }
    {
        auto document = juce::JSON::parse(json);
        document["clips"][0]["expressionEvents"].getArray()->set(0, 42);
        rejected(document);
        document = juce::JSON::parse(json);
        document["clips"][0].getDynamicObject()->removeProperty("expressionEvents");
        rejected(document);
        document = juce::JSON::parse(json);
        document.getDynamicObject()->removeProperty("transport"); // Failure after clip staging.
        rejected(document);
    }
    for (const auto* field : {"notes", "expressionEvents"}) {
        auto document = juce::JSON::parse(json);
        auto* array = document["clips"][0][field].getArray();
        const auto first = (*array)[0];
        const auto limit = juce::String(field) == "notes" ? MidiClip::maxNotes : MidiClip::maxExpressionEvents;
        while (static_cast<size_t>(array->size()) <= limit) array->add(first);
        rejected(document);
    }

    for (int version : {1, 2}) {
        auto legacy = juce::JSON::parse(json);
        legacy.getDynamicObject()->setProperty("formatVersion", version);
        if (version == 1) legacy.getDynamicObject()->removeProperty("mixerChannels");
        auto* legacyClip = legacy["clips"][0].getDynamicObject();
        legacyClip->removeProperty("expressionEvents");
        ProjectDocument::Staged migrated;
        CHECK(ProjectDocument::stage(juce::JSON::toString(legacy), migrated, error));
        CHECK(migrated.version == version && migrated.clips[0].expressionEvents.empty());
        CHECK(migrated.clips[0].notes.size() == 1 && migrated.clips[0].notes[0].getChannel() == 16);
        legacyClip->setProperty("expressionEvents", "unknown legacy property");
        CHECK(ProjectDocument::stage(juce::JSON::toString(legacy), migrated, error));
        CHECK(migrated.clips[0].expressionEvents.empty());
        auto* note = legacy["clips"][0]["notes"][0].getDynamicObject();
        for (int invalidChannel : {0, 17}) {
            note->setProperty("channel", invalidChannel);
            rejected(legacy);
        }
        note->setProperty("channel", 16);
        note->setProperty("durationBeats", 0);
        rejected(legacy);
    }
    project.clearDirty();
    source->clearExpressionEvents();
    CHECK(project.isDirty() && source->getNumNotes() == 1);
    project.clearDirty();
    source->clearExpressionEvents();
    CHECK(!project.isDirty());
    CHECK(ProjectDocument::stage(ProjectDocument::serialize(project), staged, error));
    CHECK(staged.clips[0].expressionEvents.empty());
}
