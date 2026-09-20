#include "MidiRecorder.h"
#include "project/Project.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>

namespace vibedaw {

struct MidiRecorder::Impl : private ClipPool::Listener {
    static constexpr unsigned capacity = 65536;
    struct RecordedNote {
        double start = 0, end = 0;
        int key = 0, velocity = 0;
        unsigned pass = 0;
        bool alive = true, recorded = false, muted = false;
        unsigned onCycle = 0, offCycle = 0;
    };
    struct Expression {
        MidiExpressionEvent event;
        unsigned pass = 0;
        bool alive = true, recorded = false;
        unsigned cycle = 0;
        bool seed = false;
    };
    explicit Impl(Project& p) : project(p) { held.fill(-1); physicalExpression.fill(-1); project.getClipPool().addListener(this); }
    ~Impl() override { project.getClipPool().removeListener(this); }
    Project& project;
    TransportState clipTransport;
    Options options;
    ArrangementSnapshot backing;
    std::array<RecordedNote, capacity> notes{};
    std::array<unsigned, capacity> playbackNotes{};
    std::array<Expression, capacity> expression{};
    std::array<int, 2048> held{};
    struct ExpressionLane {
        bool active = false;
        int boundary = -1;
        MidiExpressionEvent restore;
    };
    std::array<ExpressionLane, 16 * 129> expressionLanes{};
    std::array<int, 16 * 129> physicalExpression{}; // Raw CC value or 14-bit bend; -1 means no live ownership.
    bool seedTake = false;
    std::array<RenderEvent, Channel::maxLiveEvents> output{};
    unsigned noteCount = 0, expressionCount = 0, outputCount = 0;
    unsigned pass = 1, selectedPass = 0, generation = 0, revision = 0, committed = 0, committedPass = 1;
    unsigned playbackCycle = 1;
    bool active = false, outputOverflow = false;
    std::atomic<bool> recording{false}, fault{false}, dirty{false};
    enum class Fault { None, Capacity, InputLoss, Interruption, Delivery, WorkLimit };
    static_assert(std::atomic<Fault>::is_always_lock_free);
    std::atomic<Fault> faultReason{Fault::None};
    bool seenBlock = false, recoveryCreated = false, manualTake = false, wroteContent = false, recoveryOnly = false;
    static constexpr size_t maxRenderWork = 1048576;
    size_t remainingWork = maxRenderWork;
    double lastBeat = 0, extent = 0, origin = 0, loopEnd = 0;
    double originalLength = 0;
    std::unique_ptr<Clip> observedSource;
    std::vector<std::pair<unsigned, ClipId>> takes;
    juce::String placementTrack, placementId, status;
    ClipId placementClip = InvalidClipId;
    struct PlacementState {
        juce::String track, instance;
        ClipId clip = InvalidClipId;
        ChannelId channel = InvalidChannelId;
        double start = 0, duration = 0;
        bool muted = false;
    };
    struct Transaction {
        struct Source { ClipId id; std::unique_ptr<Clip> before, after; };
        ClipId targetBefore = InvalidClipId;
        std::unique_ptr<Clip> baseline;
        std::vector<Source> sources;
        PlacementState before, after;
        std::vector<std::pair<unsigned, ClipId>> takesBefore;
        bool valid = true;
    };
    std::shared_ptr<Transaction> armedTransaction, undoTransaction;
    void clipAdded(ClipId, Clip*) override {}
    void clipChanged(ClipId, Clip*) override {}
    void clipRemoved(ClipId id) override {
        for (const auto& transaction : {armedTransaction, undoTransaction})
            if (transaction && (transaction->targetBefore == id ||
                std::any_of(transaction->sources.begin(), transaction->sources.end(),
                    [id](const auto& source) { return source.id == id; }))) transaction->valid = false;
    }

    ClipInstance* placement(const juce::String& trackId, const juce::String& instanceId) const {
        if (auto* track = project.getTrackList().getTrackById(trackId))
            for (const auto& instance : track->getClipInstances())
                if (instance->getId() == instanceId) return instance.get();
        return nullptr;
    }
    PlacementState placementState() const {
        if (auto* p = placement(placementTrack, placementId))
            return {placementTrack, placementId, p->getClipId(), p->getChannelId(),
                    p->getStartTime(), p->getDuration(), p->isMuted()};
        return {};
    }
    void beginRecording() {
        armedTransaction = std::make_shared<Transaction>();
        armedTransaction->targetBefore = options.clipId;
        if (auto* source = project.getClipPool().getClip(options.clipId)) armedTransaction->baseline = source->clone();
        armedTransaction->before = armedTransaction->after = placementState();
        armedTransaction->takesBefore = takes;
        recoveryCreated = false;
        expressionLanes.fill({});
        seedTake = true;
    }

    TransportState& transport() { return options.clipFocused ? clipTransport : project.getTransportState(); }
    void changed() noexcept { dirty = true; ++revision; }
    void fail(Fault reason = Fault::Capacity) noexcept {
        if (!fault.load()) faultReason.store(reason);
        fault = true; recording = false;
    }
    bool spend(size_t work) noexcept {
        if (work > remainingWork) { fail(Fault::WorkLimit); outputOverflow = true; return false; }
        remainingWork -= work; return true;
    }
    bool room(unsigned extra = 1) noexcept {
        if (noteCount + expressionCount + extra <= capacity) return true;
        fail(); return false;
    }
    void closeHeld(double beat) noexcept {
        for (auto& index : held) {
            if (index < 0) continue;
            auto& note = notes[static_cast<unsigned>(index)];
            note.end = std::max(note.start, beat);
            if (note.end <= note.start) note.alive = false;
            index = -1;
            changed();
        }
    }
    void emit(int sample, int size, unsigned token, int statusByte = 0, int d1 = 0, int d2 = 0,
              bool physical = false) noexcept {
        if (outputCount == output.size()) { outputOverflow = true; fail(); return; }
        output[outputCount++] = {sample, size, token,
            {static_cast<unsigned char>(statusByte), static_cast<unsigned char>(d1), static_cast<unsigned char>(d2)}, physical};
    }
    void loadSource(const MidiClip* source) {
        noteCount = expressionCount = 0;
        held.fill(-1);
        if (!source) return;
        for (const auto& note : source->getNotes()) {
            notes[noteCount++] = {note.getStartTime(), note.getEndTime(),
                (note.getChannel() - 1) * 128 + note.getPitch(), note.getVelocity(), 0, true, false, note.isMuted()};
        }
        for (const auto& event : source->getExpressionEvents())
            expression[expressionCount++] = {event, 0, true, false};
    }
    static bool sameContent(const MidiClip& source, const MidiClip& other) {
        const auto* previous = &other;
        if (source.getDuration() != previous->getDuration() || source.getNotes().size() != previous->getNotes().size() ||
            source.getExpressionEvents().size() != previous->getExpressionEvents().size()) return false;
        for (size_t i = 0; i < source.getNotes().size(); ++i) {
            const auto& a = source.getNotes()[i]; const auto& b = previous->getNotes()[i];
            if (a.getPitch() != b.getPitch() || a.getChannel() != b.getChannel() || a.getVelocity() != b.getVelocity() ||
                a.getStartTime() != b.getStartTime() || a.getDuration() != b.getDuration() || a.isMuted() != b.isMuted()) return false;
        }
        for (size_t i = 0; i < source.getExpressionEvents().size(); ++i) {
            const auto& a = source.getExpressionEvents()[i]; const auto& b = previous->getExpressionEvents()[i];
            if (a.beat != b.beat || a.status != b.status || a.data1 != b.data1 || a.data2 != b.data2) return false;
        }
        return true;
    }
    bool sourceChanged(const MidiClip& source) const {
        const auto* previous = dynamic_cast<const MidiClip*>(observedSource.get());
        return previous && !sameContent(source, *previous);
    }
    bool importSource(const MidiClip& source, std::unique_ptr<Clip>& prepared) {
        unsigned retained = 0;
        if (options.mode == Mode::Takes) {
            for (unsigned i = 0; i < noteCount; ++i) if (notes[i].pass != selectedPass) ++retained;
            for (unsigned i = 0; i < expressionCount; ++i) if (expression[i].pass != selectedPass) ++retained;
        }
        if (retained + source.getNotes().size() + source.getExpressionEvents().size() > capacity) return false;
        unsigned count = 0;
        if (options.mode == Mode::Takes)
            for (unsigned i = 0; i < noteCount; ++i)
                if (notes[i].pass != selectedPass) notes[count++] = notes[i];
        noteCount = count; count = 0;
        if (options.mode == Mode::Takes)
            for (unsigned i = 0; i < expressionCount; ++i)
                if (expression[i].pass != selectedPass) expression[count++] = expression[i];
        expressionCount = count;
        for (const auto& note : source.getNotes())
            notes[noteCount++] = {note.getStartTime(), note.getEndTime(),
                (note.getChannel() - 1) * 128 + note.getPitch(), note.getVelocity(), selectedPass,
                true, false, note.isMuted()};
        for (const auto& event : source.getExpressionEvents())
            expression[expressionCount++] = {event, selectedPass, true, false};
        observedSource.swap(prepared);
        originalLength = source.getDuration();
        committedPass = pass + 1;
        return true;
    }
    bool visible(unsigned p, bool recorded) const noexcept {
        if (options.mode == Mode::Takes)
            return p == selectedPass;
        return !recording || !recorded || p < pass;
    }
    // Erase only traversed time. Split notes crossing both edges, retaining the
    // untouched suffix. A repeated pitch closes its previous recorded attack.
    void erase(double start, double end) noexcept {
        if (!spend(noteCount)) return;
        const unsigned count = noteCount;
        if (options.mode == Mode::Replace)
            for (unsigned i = 0; i < count; ++i) {
                auto& n = notes[i];
                if (!n.alive || n.pass == pass || n.end <= start || n.start >= end) continue;
                if (n.start < start && n.end > end) {
                    if (!room()) return;
                    auto suffix = n;
                    suffix.start = end;
                    suffix.onCycle = suffix.offCycle = 0;
                    notes[noteCount++] = suffix;
                    n.end = start;
                } else if (n.start < start) n.end = start;
                else if (n.end > end) n.start = end;
                else n.alive = false;
                changed();
            }
    }
    static int expressionLane(const MidiExpressionEvent& e) noexcept {
        return (e.status & 15) * 129 + ((e.status & 0xf0) == 0xe0 ? 128 : e.data1);
    }
    void replaceLane(unsigned lane, double start, double end, bool incoming) noexcept {
        if (!spend(4 * static_cast<size_t>(expressionCount))) return;
        auto& state = expressionLanes[lane];
        if (!state.active) {
            // Reserve the recovery boundary before deleting any old lane data.
            if (!room(incoming ? 2 : 1)) return;
            const int cc = static_cast<int>(lane % 129);
            state.restore = {-1, static_cast<int>((cc == 128 ? 0xe0 : 0xb0) | (lane / 129)),
                cc == 128 ? 0 : cc, cc == 7 ? 100 : (cc == 10 || cc == 128 ? 64 : (cc == 11 ? 127 : 0))};
            for (unsigned i = 0; i < expressionCount; ++i) {
                const auto& e = expression[i];
                if (e.alive && e.pass != pass && expressionLane(e.event) == static_cast<int>(lane) &&
                    e.event.beat <= start && e.event.beat >= state.restore.beat) state.restore = e.event;
            }
            state.active = true;
            state.boundary = static_cast<int>(expressionCount++);
        }
        for (unsigned i = 0; i < expressionCount; ++i) {
            auto& e = expression[i];
            if (static_cast<int>(i) == state.boundary || !e.alive || e.pass == pass ||
                expressionLane(e.event) != static_cast<int>(lane) || e.event.beat < start || e.event.beat >= end) continue;
            if (e.event.beat >= state.restore.beat) state.restore = e.event;
            e.alive = false;
        }
        auto restored = state.restore;
        restored.beat = end;
        // An untouched event exactly at punch-out owns the boundary. Do not let
        // the synthetic restore overwrite it with the preceding controller value.
        for (unsigned i = 0; i < expressionCount; ++i) {
            const auto& e = expression[i];
            if (e.alive && e.pass != pass && e.event.beat == end &&
                expressionLane(e.event) == static_cast<int>(lane)) restored = e.event;
        }
        expression[static_cast<unsigned>(state.boundary)] = {restored, pass, true, true};
        changed();
    }
    void capture(const juce::MidiMessageMetadata& input, double beat) noexcept {
        if (input.numBytes != 3) return;
        if (input.data[1] > 127 || input.data[2] > 127) { closeHeld(beat); fail(); return; }
        const int statusByte = input.data[0], kind = statusByte & 0xf0;
        if ((kind == 0xb0 && input.data[1] < 120) || kind == 0xe0) {
            const int lane = (statusByte & 15) * 129 + (kind == 0xe0 ? 128 : input.data[1]);
            physicalExpression[lane] = kind == 0xe0 ? input.data[1] + 128 * input.data[2] : input.data[2];
        }
        if (!recording) return;
        const int key = (statusByte & 15) * 128 + (input.data[1] & 127);
        if (kind == 0x90 && input.data[2]) {
            if (held[key] >= 0) {
                auto& previous = notes[static_cast<unsigned>(held[key])];
                previous.end = beat;
                previous.alive = beat > previous.start;
                held[key] = -1;
                changed();
            }
            if (!room()) { closeHeld(beat); return; }
            held[key] = static_cast<int>(noteCount);
            notes[noteCount++] = {beat, beat, key, input.data[2], pass, true, true};
            changed();
        } else if (kind == 0x80 || (kind == 0x90 && !input.data[2])) {
            if (held[key] < 0) return;
            auto& note = notes[static_cast<unsigned>(held[key])];
            note.end = beat; note.alive = beat > note.start;
            held[key] = -1;
            changed();
        } else if ((kind == 0xb0 && input.data[1] < 120) || kind == 0xe0) {
            if (!room()) { closeHeld(beat); return; }
            expression[expressionCount++] = {{beat, statusByte, input.data[1], input.data[2]}, pass, true, true};
            changed();
        }
    }
    bool writeContent(MidiClip& clip, unsigned takePass, ClipId id, bool created) {
        std::vector<Note> result;
        std::vector<MidiExpressionEvent> events;
        for (unsigned i = 0; i < noteCount; ++i) {
            const auto& n = notes[i];
            if (!n.alive || n.end <= n.start || (options.mode == Mode::Takes && n.pass != takePass)) continue;
            Note note(n.key % 128, n.start, n.end - n.start, n.velocity);
            note.setChannel(n.key / 128 + 1);
            note.setMuted(n.muted);
            result.push_back(note);
        }
        for (unsigned i = 0; i < expressionCount; ++i) {
            const auto& e = expression[i];
            if (e.alive && (options.mode != Mode::Takes || e.pass == takePass)) events.push_back(e.event);
        }
        MidiClip candidate(0, std::max(TransportState::minLoopBeats,
            options.mode == Mode::Continuous ? std::max(originalLength, extent) :
                (options.mode == Mode::Takes || created ? loopEnd : std::max(originalLength, extent))));
        if (!candidate.replaceContent(std::move(result), std::move(events))) { fail(); return false; }
        if (!created && sameContent(clip, candidate)) return true;
        jassert(armedTransaction != nullptr);
        auto& transaction = *armedTransaction;
        if (options.mode == Mode::Replace && transaction.baseline && !recoveryCreated) {
            auto backup = transaction.baseline->clone();
            backup->setName(backup->getName() + " (before recording)");
            if (project.getClipPool().addClip(std::move(backup)) == InvalidClipId) { fail(); return false; }
            recoveryCreated = true;
        }
        auto found = std::find_if(transaction.sources.begin(), transaction.sources.end(),
            [id](const auto& source) { return source.id == id; });
        if (found == transaction.sources.end()) {
            std::unique_ptr<Clip> before;
            if (!created) before = id == transaction.targetBefore && transaction.baseline
                ? transaction.baseline->clone() : clip.clone();
            transaction.sources.push_back({id, std::move(before), nullptr});
            found = std::prev(transaction.sources.end());
        }
        if (!clip.replaceContent(candidate.getNotes(), candidate.getExpressionEvents())) { fail(); return false; }
        clip.setDuration(candidate.getDuration());
        if (created && options.mode != Mode::Takes) originalLength = clip.getDuration();
        found->after = clip.clone();
        undoTransaction = armedTransaction;
        wroteContent = true;
        return true;
    }
    bool nonempty(unsigned p) const noexcept {
        for (unsigned i = 0; i < noteCount; ++i)
            if (notes[i].alive && notes[i].recorded && notes[i].end > notes[i].start &&
                (options.mode != Mode::Takes || notes[i].pass == p)) return true;
        for (unsigned i = 0; i < expressionCount; ++i)
            if (expression[i].alive && expression[i].recorded && !expression[i].seed &&
                (options.mode != Mode::Takes || expression[i].pass == p)) return true;
        return false;
    }
    void place() {
        if (options.clipFocused || recoveryOnly || options.clipId == InvalidClipId) return;
        auto& tracks = project.getTrackList();
        if (!placementId.isEmpty()) {
            if (auto* track = tracks.getTrackById(placementTrack))
                for (const auto& instance : track->getClipInstances())
                    if (instance->getId() == placementId) {
                        const auto* expected = armedTransaction && armedTransaction->after.instance == placementId
                            ? &armedTransaction->after : nullptr;
                        if (instance->getClipId() != placementClip || instance->getChannelId() != options.channelId ||
                            instance->getStartTime() != origin || (expected &&
                            (instance->getDuration() != expected->duration || instance->isMuted() != expected->muted))) {
                            recording = false; transport().stop();
                            status = "Selected placement changed; captured source retained without changing its placement";
                            return;
                        }
                        instance->setClipId(options.clipId);
                        instance->setDuration(options.mode == Mode::Takes
                            ? project.getClipPool().getClip(options.clipId)->getDuration()
                            : std::max(instance->getDuration(), extent));
                        placementClip = options.clipId;
                        if (armedTransaction && undoTransaction == armedTransaction)
                            armedTransaction->after = placementState();
                        return;
                    }
            return; // A user-deleted placement is not resurrected.
        }
        auto candidate = std::make_unique<ClipInstance>(options.clipId, options.channelId,
            origin, project.getClipPool().getClip(options.clipId)->getDuration());
        placementId = candidate->getId();
        auto* track = tracks.addTrack("Recording");
        placementTrack = track->getId();
        track->addClipInstance(std::move(candidate));
        placementClip = options.clipId;
        if (armedTransaction && undoTransaction == armedTransaction) armedTransaction->after = placementState();
    }
    void separateEditedSource() {
        const auto edited = options.clipId;
        takes.erase(std::remove_if(takes.begin(), takes.end(),
            [edited](const auto& take) { return take.second == edited; }), takes.end());
        options.clipId = InvalidClipId;
        observedSource.reset(); placementId.clear(); placementTrack.clear();
        recoveryOnly = true;
        if (armedTransaction) {
            if (armedTransaction->sources.empty()) armedTransaction->before = armedTransaction->after = {};
        }
        status = "Source edited elsewhere; captured workspace saved separately";
    }
    void commit() {
        if (dirty)
            if (auto* source = dynamic_cast<MidiClip*>(project.getClipPool().getClip(options.clipId)))
                if (sourceChanged(*source)) {
                    closeHeld(lastBeat); recording = false; transport().stop();
                    separateEditedSource();
                }
        if (options.mode == Mode::Takes) {
            if (!recording && !manualTake && nonempty(pass)) selectedPass = pass;
            for (const auto& take : takes)
                if (take.first == selectedPass && options.clipId != take.second) {
                    options.clipId = take.second;
                    if (auto* source = project.getClipPool().getClip(options.clipId)) observedSource = source->clone();
                    place();
                }
        }
        if (committed == revision) return;
        wroteContent = false;
        auto& pool = project.getClipPool();
        if (options.mode == Mode::Takes) {
            for (unsigned p = committedPass; p <= pass; ++p) {
                if (!nonempty(p)) continue;
                auto found = std::find_if(takes.begin(), takes.end(), [p](const auto& t) { return t.first == p; });
                MidiClip* target = nullptr;
                ClipId targetId = found == takes.end() ? InvalidClipId : found->second;
                bool created = false;
                if (found != takes.end()) target = dynamic_cast<MidiClip*>(pool.getClip(found->second));
                if (!target && found != takes.end()) continue; // Explicitly deleted take.
                if (!target) {
                    auto clip = std::make_unique<MidiClip>(0, options.loopLength);
                    clip->setName("Recording Take " + juce::String(p));
                    const auto id = pool.addClip(std::move(clip));
                    if (id == InvalidClipId) { fail(); return; }
                    target = dynamic_cast<MidiClip*>(pool.getClip(id));
                    takes.emplace_back(p, id);
                    targetId = id; created = true;
                }
                if (!writeContent(*target, p, targetId, created)) return;
            }
            if (!recording && !manualTake && nonempty(pass)) selectedPass = pass;
            for (const auto& take : takes)
                if (take.first == selectedPass) options.clipId = take.second;
        } else {
            auto* target = dynamic_cast<MidiClip*>(pool.getClip(options.clipId));
            bool created = false;
            if (!target && nonempty(pass)) {
                auto clip = std::make_unique<MidiClip>();
                clip->setName("Recording");
                options.clipId = pool.addClip(std::move(clip));
                if (options.clipId == InvalidClipId) { fail(); return; }
                target = dynamic_cast<MidiClip*>(pool.getClip(options.clipId));
                created = true;
            }
            if (target) {
                if (!writeContent(*target, pass, options.clipId, created)) return;
            }
        }
        if (wroteContent) place();
        if (auto* source = pool.getClip(options.clipId)) observedSource = source->clone();
        committed = revision;
        committedPass = pass;
        dirty = false;
    }
};

MidiRecorder::MidiRecorder(Project& project) : impl(std::make_unique<Impl>(project)) { startTimerHz(30); }
MidiRecorder::~MidiRecorder() { stopTimer(); endSession(); }

bool MidiRecorder::start(const Options& options, bool record, juce::String& error) {
    error.clear();
    auto& r = *impl;
    auto* channel = r.project.getChannelList().getChannelById(options.channelId);
    auto* source = dynamic_cast<MidiClip*>(r.project.getClipPool().getClip(options.clipId));
    if (!channel || !channel->hasPlugin()) { error = "Choose an available instrument."; return false; }
    if (options.clipId != InvalidClipId && !source) { error = "The recording source no longer exists."; return false; }
    if (options.mode != Mode::Continuous && options.mode != Mode::Takes &&
        options.mode != Mode::Replace && options.mode != Mode::Overdub) {
        error = "Unknown recording mode."; return false;
    }
    if (!std::isfinite(options.startBeat) || options.startBeat < 0 ||
        !TransportState::validLoopRegion(0, options.loopLength) ||
        options.startBeat + options.loopLength > TransportState::maxPositionBeats) {
        error = "Invalid recording bounds."; return false;
    }
    if (source && source->getNotes().size() + source->getExpressionEvents().size() > Impl::capacity) {
        error = "Source exceeds the 65536-event recording workspace."; return false;
    }
    double sourceOrigin = options.clipFocused ? 0 : options.startBeat;
    if (options.trackId.isEmpty() != options.placementId.isEmpty() ||
        (options.clipFocused && (!options.trackId.isEmpty() || !options.placementId.isEmpty()))) {
        error = "Supply both song track and placement IDs, or neither for standalone clip playback."; return false;
    }
    if (!options.clipFocused && options.clipId != InvalidClipId) {
        auto* placement = r.placement(options.trackId, options.placementId);
        if (!placement || placement->getClipId() != options.clipId || placement->getChannelId() != options.channelId) {
            error = "Choose the exact song placement matching this source and instrument."; return false;
        }
        sourceOrigin = placement->getStartTime();
        if (options.startBeat < sourceOrigin) { error = "The record cursor precedes the selected placement."; return false; }
    } else if (!options.trackId.isEmpty()) {
        error = "A new song source cannot use an existing placement."; return false;
    }
    if (!options.clipFocused && options.mode == Mode::Continuous && r.project.getTransportState().isLoopEnabled()) {
        error = "Disable the song loop for continuous recording."; return false;
    }
    if (!options.clipFocused && options.mode != Mode::Continuous) {
        const auto loop = r.project.getTransportState().getLoopRegion();
        if (!loop.enabled || options.startBeat < loop.startBeats || options.startBeat >= loop.endBeats ||
            sourceOrigin > loop.startBeats) {
            error = "Loop recording needs the cursor inside the song loop and a source origin at or before its start.";
            return false;
        }
    }
    endSession();
    AudioQuiescence::Edit edit;
    if (r.dirty) { error = "Pending recording could not be committed; free clip-pool capacity before starting again."; return false; }
    if (!options.placementId.isEmpty()) {
        auto* placement = r.placement(options.trackId, options.placementId);
        if (!placement || placement->getClipId() != options.clipId || placement->getChannelId() != options.channelId ||
            placement->getStartTime() != sourceOrigin) {
            error = "Selected placement changed while the preceding session was finalized."; return false;
        }
    }
    r.options = options;
    if (!options.clipFocused && options.mode != Mode::Continuous) {
        const auto loop = r.project.getTransportState().getLoopRegion();
        r.options.loopLength = loop.endBeats - loop.startBeats;
    }
    r.observedSource = source ? source->clone() : nullptr;
    r.originalLength = source ? source->getDuration() : 0;
    r.loadSource(source);
    r.takes.clear(); r.placementId = options.placementId; r.placementTrack = options.trackId;
    r.placementClip = options.clipId;
    r.armedTransaction.reset();
    r.physicalExpression.fill(-1);
    r.pass = r.committedPass = 1; r.selectedPass = 0; r.revision = r.committed = 0;
    r.active = true; r.recording = record; r.fault = false; r.dirty = false;
    r.faultReason = Impl::Fault::None;
    r.recoveryCreated = false; r.recoveryOnly = false; r.manualTake = false; r.seenBlock = false; r.extent = 0; r.lastBeat = 0;
    r.origin = sourceOrigin;
    r.loopEnd = options.clipFocused ? options.loopLength :
        r.project.getTransportState().getLoopRegion().endBeats - sourceOrigin;
    r.lastBeat = options.clipFocused ? 0 : options.startBeat - sourceOrigin;
    if (record) r.beginRecording();
    r.status = record ? "Recording" : "Playing clip";
    ++r.generation;
    r.backing = options.clipFocused ? ArrangementSnapshot{} : compileArrangement(r.project.getTrackList(),
        r.project.getClipPool(), r.project.getChannelList(), r.generation, InvalidClipId,
        InvalidChannelId, -1, options.placementId);
    auto& song = r.project.getTransportState();
    if (options.clipFocused) {
        song.pollRenderPosition();
        song.setPlaying(false); // Keep the song cursor parked, never seek it to clip time.
        song.setPositionInBeats(song.getPositionInBeats());
        r.clipTransport.setTempo(song.getTempo());
        const auto meter = song.getTimeSignature();
        r.clipTransport.setTimeSignature(meter.numerator, meter.denominator);
        r.clipTransport.setMetronomeEnabled(song.isMetronomeEnabled());
        r.clipTransport.setLoopRegion(0, r.options.loopLength);
        r.clipTransport.setLoopEnabled(options.mode != Mode::Continuous);
    }
    r.transport().setPositionInBeats(options.clipFocused ? 0 : options.startBeat);
    r.transport().setRecording(record);
    r.transport().setPlaying(true);
    return true;
}

bool MidiRecorder::setRecording(bool record, juce::String& error) {
    error.clear();
    if (record) poll(); // Import disarmed editor changes before recording resumes.
    AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
    auto& r = *impl;
    if (!r.active) { error = "Start a recording session first."; return false; }
    if (record && !r.placementId.isEmpty()) {
        auto* placement = r.placement(r.placementTrack, r.placementId);
        if (!placement || placement->getClipId() != r.placementClip || placement->getChannelId() != r.options.channelId ||
            placement->getStartTime() != r.origin) {
            error = "Selected placement was removed, moved, or rerouted; start a new session."; return false;
        }
    }
    if (record && (r.fault || r.pass == Impl::capacity)) { error = "Recording capacity reached; stop and start a new session."; return false; }
    if (r.recording == record) return true;
    r.closeHeld(r.lastBeat);
    r.recording = record;
    if (record) { ++r.pass; r.committedPass = r.pass; r.manualTake = false; r.beginRecording(); }
    else r.commit();
    r.transport().setRecording(record);
    if (record) {
        ++r.generation;
    }
    r.status = record ? "Recording" : "Disarmed; playback continues";
    return true;
}

void MidiRecorder::stop() {
    AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
    auto& r = *impl;
    if (!r.active) return;
    r.closeHeld(r.lastBeat); r.recording = false;
    r.transport().pollRenderPosition();
    r.transport().stop();
    r.transport().setPositionInBeats(r.transport().getPositionInBeats());
    r.commit();
    r.status = r.fault ? "Recording stopped at the capacity/timing limit; captured prefix preserved" :
        (r.recoveryOnly ? "Stopped; source edited elsewhere and recording retained in a separate recovery source" : "Stopped");
}

void MidiRecorder::endSession() {
    AudioQuiescence::Edit edit;
    stop();
    if (impl->active && !impl->dirty) { impl->active = false; ++impl->generation; }
}

bool MidiRecorder::undoLastRecording(juce::String& error) {
    error.clear();
    AudioQuiescence::Edit edit;
    auto& r = *impl;
    const auto transaction = r.undoTransaction;
    if (r.recording || r.dirty) { error = "Disarm and commit the recording before undo."; return false; }
    if (!transaction || transaction->sources.empty()) { error = "No committed recording to undo."; return false; }
    if (!transaction->valid) { error = "A recording source was removed; recovery has been retained."; return false; }
    auto& pool = r.project.getClipPool();
    auto& tracks = r.project.getTrackList();
    // Validate the complete transaction before changing any source or placement.
    // Importing an editor's changes for playback must never update this guard.
    for (const auto& source : transaction->sources) {
        auto* actual = dynamic_cast<MidiClip*>(pool.getClip(source.id));
        auto* expected = dynamic_cast<MidiClip*>(source.after.get());
        if (!actual || !expected || !Impl::sameContent(*actual, *expected) ||
            actual->getName() != expected->getName() || actual->getColour() != expected->getColour() ||
            actual->getStartTime() != expected->getStartTime() || actual->isMuted() != expected->isMuted() ||
            actual->isLoopEnabled() != expected->isLoopEnabled()) {
            error = "Recording source changed since its last commit; undo would overwrite later edits."; return false;
        }
        if (!source.before)
            for (const auto& track : tracks.getTracks())
                for (const auto& instance : track->getClipInstances())
                    if (instance->getClipId() == source.id && instance->getId() != transaction->after.instance) {
                        error = "A recorded take is used by another placement; undo would remove it."; return false;
                    }
    }
    const auto& after = transaction->after;
    auto* placed = r.placement(after.track, after.instance);
    if (!after.instance.isEmpty() && (!placed || placed->getClipId() != after.clip ||
        placed->getChannelId() != after.channel || placed->getStartTime() != after.start ||
        placed->getDuration() != after.duration || placed->isMuted() != after.muted)) {
        error = "Recording placement changed since its last commit; later edits were preserved."; return false;
    }
    if (r.active) { r.transport().stop(); r.active = false; ++r.generation; }
    for (const auto& source : transaction->sources)
        if (auto* before = dynamic_cast<MidiClip*>(source.before.get())) {
            auto* actual = dynamic_cast<MidiClip*>(pool.getClip(source.id));
            if (!actual->replaceContent(before->getNotes(), before->getExpressionEvents())) {
                error = "Could not restore recording content; recovery has been retained."; return false;
            }
            actual->setDuration(before->getDuration());
        }
    if (placed) {
        if (!transaction->before.instance.isEmpty()) {
            placed->setClipId(transaction->before.clip);
            placed->setDuration(transaction->before.duration);
        } else if (auto* track = tracks.getTrackById(after.track)) {
            for (int i = 0; i < track->getNumClipInstances(); ++i)
                if (track->getClipInstance(i)->getId() == after.instance) { track->removeClipInstance(i); break; }
            if (track->getNumClipInstances() == 0) tracks.removeTrack(tracks.indexOfTrack(track));
        }
    }
    for (const auto& source : transaction->sources) if (!source.before) pool.removeClip(source.id);
    r.options.clipId = transaction->targetBefore;
    r.takes = transaction->takesBefore;
    r.undoTransaction.reset();
    if (r.armedTransaction == transaction) r.armedTransaction.reset();
    r.status = "Last recording undone; recovery backups retained";
    return true;
}

bool MidiRecorder::canUndoLastRecording() const {
    return !impl->recording && !impl->dirty && impl->undoTransaction &&
        impl->undoTransaction->valid && !impl->undoTransaction->sources.empty();
}

bool MidiRecorder::selectTake(unsigned index, juce::String& error) {
    error.clear();
    poll();
    auto& r = *impl;
    if (!r.active) { error = "Start a session before choosing a take."; return false; }
    if (r.recording) { error = "Disarm before choosing a take."; return false; }
    if (index >= r.takes.size() || !r.project.getClipPool().getClip(r.takes[index].second)) {
        error = "Take no longer exists."; return false;
    }
    auto* source = dynamic_cast<MidiClip*>(r.project.getClipPool().getClip(r.takes[index].second));
    if (!source) { error = "Take no longer exists."; return false; }
    auto prepared = source->clone();
    AudioQuiescence::Edit edit;
    const auto previousPass = r.selectedPass;
    r.selectedPass = r.takes[index].first;
    if (!r.importSource(*source, prepared)) {
        r.selectedPass = previousPass;
        error = "Take exceeds the remaining recording workspace."; return false;
    }
    r.manualTake = true;
    r.options.clipId = r.takes[index].second;
    ++r.generation;
    r.place();
    return true;
}

bool MidiRecorder::isSessionActive() const { return impl->active; }
bool MidiRecorder::isRecording() const { return impl->recording.load(); }
bool MidiRecorder::isClipFocused() const { return impl->options.clipFocused; }
ClipId MidiRecorder::getTargetClipId() const { return impl->options.clipId; }
ChannelId MidiRecorder::getChannelId() const { return impl->options.channelId; }
MidiRecorder::Mode MidiRecorder::getMode() const { return impl->options.mode; }
unsigned MidiRecorder::getTakeCount() const { return static_cast<unsigned>(impl->takes.size()); }
bool MidiRecorder::hasPendingContent() const { return impl->dirty.load(); }
juce::String MidiRecorder::getStatus() const {
    juce::String result = impl->recording ? "Recording; note preview and takes are pending until disarm" : impl->status;
    switch (impl->faultReason.load()) {
        case Impl::Fault::None: break;
        case Impl::Fault::InputLoss: result = "Input-loss fault; recording disarmed, valid prefix retained"; break;
        case Impl::Fault::Interruption: result = "Audio interruption fault; recording disarmed, valid prefix retained"; break;
        case Impl::Fault::Delivery: result = "Instrument delivery/reset fault; recording disarmed, valid prefix retained"; break;
        case Impl::Fault::WorkLimit: result = "Render work-budget fault; recording disarmed, valid prefix retained"; break;
        case Impl::Fault::Capacity: result = "Recording capacity/timing fault; captured prefix retained"; break;
    }
    if (impl->active && !impl->options.clipFocused) result += "; backing edits apply after End Session";
    return result;
}
TransportState& MidiRecorder::getPlaybackTransport() { return impl->transport(); }

void MidiRecorder::poll() {
    auto& r = *impl;
    if (!r.active) return;
    if (r.options.clipFocused) {
        auto& song = r.project.getTransportState();
        if (song.isPlaying()) song.setPlaying(false);
        r.clipTransport.setTempo(song.getTempo());
        const auto meter = song.getTimeSignature();
        r.clipTransport.setTimeSignature(meter.numerator, meter.denominator);
        r.clipTransport.setMetronomeEnabled(song.isMetronomeEnabled());
    }
    r.transport().pollRenderPosition();
    const bool missingClip = r.options.clipId != InvalidClipId && !r.project.getClipPool().getClip(r.options.clipId);
    if (missingClip || !r.project.getChannelList().getChannelById(r.options.channelId)) {
        AudioQuiescence::Edit edit;
        if (missingClip) r.options.clipId = InvalidClipId;
        endSession();
        r.status = "Target removed; session ended and captured content preserved";
        return;
    }
    // No render gate, source scans, sorting, cloning, or model notifications while
    // recording. The bounded audio workspace is committed only on disarm/stop.
    if (r.recording && r.transport().isPlaying()) return;
    if (r.dirty || r.recording || (r.fault && r.transport().isPlaying())) { stop(); return; }
    if (auto* source = dynamic_cast<MidiClip*>(r.project.getClipPool().getClip(r.options.clipId))) {
        if (r.sourceChanged(*source)) {
            if (r.recording || r.dirty) {
                AudioQuiescence::Edit edit;
                // Never overwrite an independent editor's changes. Commit the
                // captured workspace to a separate recovery source instead.
                r.separateEditedSource();
                endSession();
                r.status = "Source edited elsewhere; recording saved separately and session ended";
                return;
            }
            auto prepared = source->clone();
            bool imported = false;
            {
                AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
                imported = r.importSource(*source, prepared);
                if (imported) ++r.generation;
            }
            if (!imported) {
                endSession(); r.status = "Edited source exceeds recording workspace; session ended"; return;
            }
        }
    }
    if (!r.transport().isPlaying() || r.fault) r.transport().setRecording(false);
}

bool MidiRecorder::renderActive() const noexcept { return impl->active; }
bool MidiRecorder::renderHasStarted() const noexcept { return impl->seenBlock; }
unsigned MidiRecorder::renderGeneration() const noexcept { return impl->generation; }
ChannelId MidiRecorder::renderChannel() const noexcept { return impl->options.channelId; }
TransportState& MidiRecorder::renderTransport() noexcept { return impl->transport(); }
const ArrangementSnapshot& MidiRecorder::renderArrangement() const noexcept { return impl->backing; }
const MidiRecorder::RenderEvent* MidiRecorder::renderEvents() const noexcept { return impl->output.data(); }
unsigned MidiRecorder::renderEventCount() const noexcept { return impl->outputCount; }
bool MidiRecorder::renderOverflowed() const noexcept { return impl->outputOverflow; }
void MidiRecorder::renderDeliveryFailed() noexcept {
    impl->closeHeld(impl->lastBeat); impl->fail(Impl::Fault::Delivery);
}
void MidiRecorder::renderInterrupted() noexcept {
    if (impl->active && impl->seenBlock && impl->recording) {
        impl->closeHeld(impl->lastBeat); impl->fail(Impl::Fault::Interruption);
    }
}
void MidiRecorder::primeLiveExpression(const std::array<int, 16 * 129>& values) noexcept {
    impl->physicalExpression = values;
}

void MidiRecorder::render(const TransportClock::Block& block, const juce::MidiBuffer& input, bool inputValid,
                          bool syntheticCleanup, bool interrupted) noexcept {
    auto& r = *impl;
    r.outputCount = 0; r.outputOverflow = false;
    r.remainingWork = Impl::maxRenderWork;
    if (!r.active) return;
    if (!inputValid || (interrupted && r.seenBlock && r.recording)) {
        r.closeHeld(r.lastBeat); r.fail(!inputValid ? Impl::Fault::InputLoss : Impl::Fault::Interruption); return;
    }
    if (block.spanOverflow) { r.closeHeld(r.lastBeat); r.fail(); return; }
    if (r.recording && r.seenBlock && r.options.mode == Mode::Continuous &&
        block.startBeats - r.origin + 1.0e-9 < r.lastBeat) {
        r.closeHeld(r.lastBeat); r.fail(); return;
    }
    if (block.discontinuity && r.seenBlock) {
        r.closeHeld(r.lastBeat);
        r.expressionLanes.fill({});
    }
    if (block.discontinuity) ++r.playbackCycle;
    r.seenBlock = true;
    if (!block.playing) { r.closeHeld(r.lastBeat); r.recording = false; return; }
    const double step = block.tempo / 60.0 / block.sampleRate;
    const double epsilon = 1.0e-7 + 8 * std::numeric_limits<double>::epsilon() * std::abs(block.startBeats) / step;
    auto inputIterator = input.cbegin();
    if (syntheticCleanup) inputIterator = input.cend();
    for (unsigned s = 0; s < block.spanCount; ++s) {
        if (!r.spend(16384 + 4 * static_cast<size_t>(r.expressionCount) + r.noteCount)) {
            r.closeHeld(r.lastBeat); return;
        }
        const auto& span = block.spans[s];
        const double start = span.start - r.origin;
        const double end = span.end - r.origin;
        if (r.recording && (end <= start || (r.options.mode == Mode::Continuous && span.wrap) ||
            (r.options.mode != Mode::Continuous && (start < 0 || end > r.loopEnd + 1.0e-9)))) {
            r.closeHeld(r.lastBeat); r.fail(); return;
        }
        const auto sampleAt = [&](double beat) {
            return static_cast<int>(std::floor(span.sampleOffset + (beat + r.origin - span.start) / step + epsilon));
        };
        const int firstSample = std::max(0, sampleAt(start));
        const int endSample = s + 1 == block.spanCount ? block.numSamples : std::max(0, sampleAt(end));
        std::array<int, 16 * 129> physicalFrom;
        for (unsigned lane = 0; lane < physicalFrom.size(); ++lane)
            physicalFrom[lane] = r.recording && r.physicalExpression[lane] >= 0 ? firstSample : block.numSamples;
        if (r.recording)
            for (auto iterator = inputIterator; iterator != input.cend(); ++iterator) {
                const auto message = *iterator;
                const int sample = juce::jlimit(0, block.numSamples - 1, message.samplePosition);
                if (sample >= endSample) break;
                if (sample < firstSample || message.numBytes != 3 || message.data[1] > 127 || message.data[2] > 127) continue;
                const int kind = message.data[0] & 0xf0;
                if (kind != 0xe0 && !(kind == 0xb0 && message.data[1] < 120)) continue;
                const int lane = (message.data[0] & 15) * 129 + (kind == 0xe0 ? 128 : message.data[1]);
                physicalFrom[lane] = std::min(physicalFrom[lane], sample);
            }
        if (span.wrap) {
            r.expressionLanes.fill({});
            ++r.playbackCycle;
            std::array<int, 2048> velocities{};
            for (unsigned key = 0; key < r.held.size(); ++key)
                if (r.held[key] >= 0) velocities[key] = r.notes[static_cast<unsigned>(r.held[key])].velocity;
            r.closeHeld(r.lastBeat);
            if (r.options.mode == Mode::Takes && r.recording && !r.manualTake && r.nonempty(r.pass)) r.selectedPass = r.pass;
            if (r.recording) {
                if (r.pass == Impl::capacity) r.fail();
                else ++r.pass;
            }
            r.emit(firstSample, -1, 0);
            if (r.recording)
                for (unsigned lane = 0; lane < r.physicalExpression.size() && r.recording; ++lane) {
                    const int value = r.physicalExpression[lane];
                    if (value < 0) continue;
                    const int cc = static_cast<int>(lane % 129);
                    const int status = (cc == 128 ? 0xe0 : 0xb0) | static_cast<int>(lane / 129);
                    const int data1 = cc == 128 ? value & 127 : cc;
                    const int data2 = cc == 128 ? value >> 7 : value;
                    r.emit(firstSample, 3, 0, status, data1, data2, true);
                    if (r.options.mode == Mode::Takes && r.room()) {
                        r.expression[r.expressionCount++] = {{start, status, data1, data2}, r.pass, true, true};
                        r.expression[r.expressionCount - 1].seed = true;
                        r.changed();
                    }
                }
            if (r.recording)
                for (unsigned key = 0; key < velocities.size(); ++key) {
                    if (!velocities[key] || !r.room()) continue;
                    r.held[key] = static_cast<int>(r.noteCount);
                    r.notes[r.noteCount++] = {start, start, static_cast<int>(key), velocities[key], r.pass, true, true};
                    r.changed();
                }
        }
        if (!span.wrap && s == 0 && block.discontinuity && r.recording)
            for (unsigned lane = 0; lane < r.physicalExpression.size() && r.recording; ++lane) {
                const int value = r.physicalExpression[lane];
                if (value < 0) continue;
                const int cc = static_cast<int>(lane % 129);
                const int status = (cc == 128 ? 0xe0 : 0xb0) | static_cast<int>(lane / 129);
                const int data1 = cc == 128 ? value & 127 : cc, data2 = cc == 128 ? value >> 7 : value;
                r.emit(firstSample, 3, 0, status, data1, data2, true);
                if (r.seedTake && r.options.mode == Mode::Takes && r.room()) {
                    r.expression[r.expressionCount++] = {{start, status, data1, data2}, r.pass, true, true};
                    r.expression[r.expressionCount - 1].seed = true;
                    r.changed();
                }
            }
        r.seedTake = false;
        if (r.recording && start >= 0 && r.options.mode != Mode::Overdub) r.erase(start, end);
        if (r.recording && (r.options.mode == Mode::Overdub || r.options.mode == Mode::Replace)) {
            std::array<double, 16 * 129> touches;
            std::array<bool, 16 * 129> incoming{};
            touches.fill(std::numeric_limits<double>::infinity());
            for (auto iterator = inputIterator; iterator != input.cend(); ++iterator) {
                const auto message = *iterator;
                const int sample = juce::jlimit(0, block.numSamples - 1, message.samplePosition);
                if (sample >= endSample) break;
                if (sample < firstSample || message.numBytes != 3 || message.data[1] > 127 || message.data[2] > 127) continue;
                const int kind = message.data[0] & 0xf0;
                if (kind != 0xe0 && !(kind == 0xb0 && message.data[1] < 120)) continue;
                const int lane = (message.data[0] & 15) * 129 + (kind == 0xe0 ? 128 : message.data[1]);
                touches[lane] = std::min(touches[lane], std::min(end, std::max(start, start + (sample - span.sampleOffset) * step)));
                incoming[lane] = true;
            }
            for (unsigned lane = 0; lane < touches.size() && r.recording; ++lane) {
                if (r.expressionLanes[lane].active) r.replaceLane(lane, start, end, incoming[lane]);
                else if (std::isfinite(touches[lane])) r.replaceLane(lane, touches[lane], end, incoming[lane]);
            }
        }
        if ((block.discontinuity && s == 0) || span.wrap) {
            // Restore only expression lanes present in this source, bounded by
            // the same destination budget as ordinary playback.
            std::array<int, 16 * 129> latest;
            std::array<bool, 16 * 129> lanes{};
            latest.fill(-1);
            for (unsigned i = 0; i < r.expressionCount; ++i) {
                const auto& e = r.expression[i];
                if (!r.visible(e.pass, e.recorded)) continue;
                if ((e.event.status & 0xf0) == 0xb0 && e.event.data1 >= 120) continue;
                const int lane = (e.event.status & 15) * 129 + ((e.event.status & 0xf0) == 0xe0 ? 128 : e.event.data1);
                lanes[lane] = true;
                if (!e.alive || e.event.beat >= start) continue;
                if (latest[lane] < 0 || r.expression[static_cast<unsigned>(latest[lane])].event.beat <= e.event.beat)
                    latest[lane] = static_cast<int>(i);
            }
            for (unsigned lane = 0; lane < lanes.size(); ++lane) {
                if (!lanes[lane] || latest[lane] >= 0 || physicalFrom[lane] <= firstSample) continue;
                const int cc = static_cast<int>(lane % 129);
                const int value = cc == 7 ? 100 : (cc == 10 || cc == 128 ? 64 : (cc == 11 ? 127 : 0));
                r.emit(firstSample, 3, 0, (cc == 128 ? 0xe0 : 0xb0) | (lane / 129), cc == 128 ? 0 : cc, value);
            }
            for (unsigned i = 0; i < r.expressionCount; ++i) {
                const auto& e = r.expression[i].event;
                const int lane = (e.status & 15) * 129 + ((e.status & 0xf0) == 0xe0 ? 128 : e.data1);
                if (latest[lane] == static_cast<int>(i) && physicalFrom[lane] > firstSample)
                    r.emit(firstSample, 3, 0, e.status, e.data1, e.data2);
            }
        }
        unsigned visibleNotes = 0;
        for (unsigned i = 0; i < r.noteCount; ++i) {
            const auto& n = r.notes[i];
            if (n.alive && !n.muted && n.velocity > 0 && r.visible(n.pass, n.recorded) && n.end > n.start)
                r.playbackNotes[visibleNotes++] = i;
        }
        unsigned levels = 1;
        for (unsigned n = visibleNotes; n > 1; n >>= 1) ++levels;
        if (!r.spend(static_cast<size_t>(visibleNotes) * (levels * 2 + 2))) { r.closeHeld(r.lastBeat); return; }
        std::sort(r.playbackNotes.begin(), r.playbackNotes.begin() + visibleNotes, [&](unsigned a, unsigned b) {
            return std::tie(r.notes[a].key, r.notes[a].start, a) < std::tie(r.notes[b].key, r.notes[b].start, b);
        });
        for (unsigned i = 0; i < visibleNotes;) {
            auto& n = r.notes[r.playbackNotes[i++]];
            double unionEnd = n.end;
            while (i < visibleNotes) {
                const auto& next = r.notes[r.playbackNotes[i]];
                if (next.key != n.key || next.start >= unionEnd) break;
                unionEnd = std::max(unionEnd, next.end);
                ++i;
            }
            // Strict overlap is one lifetime, exactly like compileArrangement.
            // The key token remains stable if a newly completed overdub extends it.
            const auto token = 0x100000u + static_cast<unsigned>(n.key);
            // As in the arrangement cursor, a boundary event rounded to the next
            // callback belongs to its sample zero, even if its beat is just behind
            // that callback's floating-point start. Cycle stamps prevent replay.
            const double earliest = !block.discontinuity && !span.wrap && s == 0 ? start - step : start;
            if (n.onCycle != r.playbackCycle && n.start >= earliest && n.start < end) {
                const int sample = std::max(0, sampleAt(n.start));
                if (sample < block.numSamples && sampleAt(std::min(unionEnd, end)) > sample) {
                    r.emit(sample, 3, token, 0x90 | (n.key / 128), n.key % 128, n.velocity);
                    n.onCycle = r.playbackCycle;
                }
            }
            if (n.offCycle != r.playbackCycle && unionEnd >= earliest && unionEnd < end) {
                const int sample = std::max(0, sampleAt(unionEnd));
                if (sample < block.numSamples) {
                    r.emit(sample, 3, token, 0x80 | (n.key / 128), n.key % 128, 0);
                    n.offCycle = r.playbackCycle;
                }
            }
        }
        for (unsigned i = 0; i < r.expressionCount; ++i) {
            auto& e = r.expression[i];
            const double earliest = !block.discontinuity && !span.wrap && s == 0 ? start - step : start;
            if (!e.alive || e.cycle == r.playbackCycle || !r.visible(e.pass, e.recorded) ||
                e.event.beat < earliest || e.event.beat >= end) continue;
            const int sample = std::max(0, sampleAt(e.event.beat));
            if (physicalFrom[Impl::expressionLane(e.event)] <= sample) continue;
            if (sample < block.numSamples) {
                r.emit(sample, 3, 0, e.event.status, e.event.data1, e.event.data2);
                e.cycle = r.playbackCycle;
            }
        }
        while (inputIterator != input.cend()) {
            const auto message = *inputIterator;
            const int sample = juce::jlimit(0, block.numSamples - 1, message.samplePosition);
            if (sample >= endSample) break;
            ++inputIterator;
            if (sample < firstSample) continue;
            const double beat = std::min(end, std::max(start, start + (sample - span.sampleOffset) * step));
            if (beat >= 0) r.capture(message, beat);
        }
        if (r.recording) {
            for (const auto index : r.held)
                if (index >= 0) { r.notes[static_cast<unsigned>(index)].end = end; r.changed(); }
            if (end > r.extent) {
                r.extent = end;
                if (r.options.mode == Mode::Continuous && r.nonempty(r.pass)) r.changed();
            }
        }
        r.lastBeat = end;
        if (r.fault) r.closeHeld(r.lastBeat);
    }
}

} // namespace vibedaw
