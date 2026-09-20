#include "core/AudioBoundary.h"
#include "core/ChannelMixer.h"
#include "project/Project.h"
#include "plugins/PluginHost.h"
#include "plugins/PluginWindow.h"
#include "plugins/builtin/InternalPluginFormat.h"
#include "plugins/builtin/VibeSynthProcessor.h"
#include "ui/DragPayload.h"
#include "ui/panels/MixerPanel.h"
#include "ui/TransportComponent.h"
#include "ui/DawLookAndFeel.h"
#include "ui/DawWindow.h"
#include "ui/PianoComponent.h"
#include "ui/panels/TimelinePanel.h"
#include "ui/timeline/TimelineGeometry.h"
#include "ui/editor/NoteGridComponent.h"
#include "ui/editor/PianoRollGeometry.h"
#include "ui/editor/PianoRollKeyboard.h"
#include "ui/editor/TimeRulerComponent.h"
#include "ui/editor/PianoRollEditor.h"
#include "ui/editor/ClipEditorWindow.h"
#include "ui/components/PluginButton.h"
#include "ui/components/TextPrompt.h"
#include "ui/sidebar/channel/ChannelRackSidebar.h"
#include "ui/sidebar/clips/ClipsSidebar.h"
#include "ui/sidebar/browser/PluginSection.h"
#include "ui/sidebar/browser/BrowserSidebar.h"
#include "ui/sidebar/SidebarContainer.h"
#include "ui/sidebar/SidebarTab.h"
#include "core/Constants.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>
#include <cstdlib>
#include <new>

thread_local bool rendering = false;
thread_local unsigned renderAllocations = 0, renderDeletions = 0;
void* operator new(std::size_t n) {
    if (rendering) ++renderAllocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if (rendering && p) ++renderDeletions; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

using namespace vibedaw;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #x); } while (false)

namespace vibedaw {
struct SidebarContainerTestAccess {
    static double started(const SidebarContainer& container, const Sidebar& sidebar) {
        const auto* transition = container.transitionFor(&sidebar);
        CHECK(transition != nullptr);
        return transition->started;
    }
    static void advance(SidebarContainer& container, double now) { container.advanceAnimation(now); }
    static bool running(const SidebarContainer& container) { return !container.transitions_.empty(); }
    static constexpr double fade = SidebarContainer::fadeDurationMs;
    static constexpr double reflow = SidebarContainer::reflowDurationMs;
};
struct SidebarTabTestAccess {
    static void advance(SidebarTab& tab, double elapsed) { tab.advanceFlash(tab.flashStartTime_ + elapsed); }
    static float amount(const SidebarTab& tab) { return tab.flashAmount_; }
};
struct DawWindowTestAccess {
    static void poll(DawWindow& window) { window.pollNativeState(juce::Time::getMillisecondCounterHiRes()); }
    static void expire(DawWindow& window) { window.pollNativeState(window.requestStarted_ + 1600.0); }
};
struct PluginWindowTestAccess {
    static std::unique_ptr<PluginWindow> create(PluginHost* host) {
        return std::unique_ptr<PluginWindow>(new PluginWindow(host, "Offline plugin", false));
    }
};
struct ProjectTestAccess {
    static void loader(Project& project, std::function<std::unique_ptr<PluginHost>(const juce::String&)> load) {
        project.pluginLoader_ = std::move(load);
    }
    static void restorer(Project& project,
        std::function<std::unique_ptr<PluginHost>(const juce::PluginDescription&, const juce::MemoryBlock&)> restore) {
        project.pluginRestorer_ = std::move(restore);
    }
};
struct TimelineContentTestAccess {
    static void tick(TimelineContent& content) { content.timerCallback(); }
    static juce::PopupMenu placementMenu(TimelineContent& content, Track& track, ClipInstance& instance) {
        return content.createPlacementMenu(track, instance);
    }
    static juce::PopupMenu emptySpaceMenu(TimelineContent& content, const juce::String& targetId,
                                          bool newTrack, double beat) {
        return content.createEmptySpaceMenu(targetId, newTrack, beat);
    }
};
struct ClipRowTestAccess {
    static void starter(ClipRow& row, std::function<void(const juce::var&, bool)> start) {
        row.dragStarter_ = std::move(start);
    }
};
struct ClipsContentTestAccess {
    static void clickDeleteSource(ClipsContent& clips) { clips.deleteClipButton_.triggerClick(); }
};
struct AudioEngineTestAccess {
    inline static double nowMs = 1000.0;
    static double clock() { return nowMs; }
    static void resetClock() { nowMs = 1000.0; }
    static void attachClock(AudioEngine& engine) { engine.midiClock = &clock; }
    static void prepare(AudioEngine& engine, double rate, int samples) {
        if (engine.midiClock != &clock) {
            resetClock();
            attachClock(engine);
        }
        engine.prepare(rate, samples);
    }
    static void stop(AudioEngine& engine) { engine.audioDeviceStopped(); }
    static void render(AudioEngine& engine, juce::AudioBuffer<float>& buffer) {
        renderSamples(engine, buffer, buffer.getNumSamples());
    }
    static void renderSamples(AudioEngine& engine, juce::AudioBuffer<float>& buffer, int samples) {
        // Existing routing tests deliver at block start. Round up elapsed time so
        // their input reliably lands at zero, independent of wall-clock/runtime jitter.
        if (samples > 0 && engine.sampleRate > 0)
            nowMs += std::ceil(samples * 1000.0 / engine.sampleRate);
        renderAt(engine, buffer, samples, nowMs);
    }
    static void renderAt(AudioEngine& engine, juce::AudioBuffer<float>& buffer, int samples, double timeMs) {
        nowMs = timeMs;
        engine.audioDeviceIOCallbackWithContext(nullptr, 0, buffer.getArrayOfWritePointers(),
            buffer.getNumChannels(), samples, {});
    }
};
struct ChannelTestAccess { static void resetVoices(Channel& channel) { channel.timerCallback(); } };
struct MidiManagerTestAccess { static void drain(MidiManager& manager) { manager.timerCallback(); } };
struct LevelMeterTestAccess {
    static float tick(LevelMeter& meter, double seconds) {
        meter.lastTime = juce::Time::getMillisecondCounterHiRes() - seconds * 1000;
        meter.timerCallback();
        return meter.leftDisplayLevel;
    }
};
}

static void boundaryTests() {
    BoundedQueue<unsigned, 4> queue;
    for (unsigned i = 0; i < 4; ++i) CHECK(queue.push(i));
    CHECK(!queue.push(99));
    unsigned value = 0;
    for (unsigned i = 0; i < 4; ++i) { CHECK(queue.pop(value)); CHECK(value == i); }
    CHECK(!queue.pop(value));
    for (unsigned i = 0; i < 10000; ++i) { CHECK(queue.push(i)); CHECK(queue.pop(value)); CHECK(value == i); }

    struct State { unsigned a = 0, b = 0; std::vector<unsigned> data; };
    LatestState<State> mailbox;
    mailbox.publish({1, 1, {1}});
    const auto& held = mailbox.acquire();
    for (unsigned i = 2; i < 1000; ++i) mailbox.publish({i, i, {i}});
    CHECK(held.a == 1 && held.data[0] == 1);
    CHECK(mailbox.acquire().a == 999);
    std::atomic<bool> finished{false};
    std::thread producer([&] {
        for (unsigned i = 1000; i < 100000; ++i) mailbox.publish({i, i, {i}});
        finished.store(true);
    });
    do {
        const auto& state = mailbox.acquire();
        CHECK(state.a == state.b && state.data[0] == state.a);
    } while (!finished.load());
    producer.join();
    CHECK(mailbox.acquire().a == 99999);
    rendering = true;
    mailbox.acquire();
    rendering = false;
    CHECK(renderAllocations == 0 && renderDeletions == 0);

    auto& gate = AudioQuiescence::instance();
    CHECK(gate.enter());
    std::atomic<bool> started{false}, edited{false};
    std::thread writer([&] { started.store(true); AudioQuiescence::Edit edit; edited.store(true); });
    while (!started.load()) std::this_thread::yield();
    CHECK(!edited.load());
    gate.leave();
    writer.join();
    CHECK(edited.load());
    { AudioQuiescence::Edit edit; AudioQuiescence::Edit nested; CHECK(!gate.enter()); }
    CHECK(gate.enter()); gate.leave();
}

struct Changes : juce::ChangeListener {
    int count = 0;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { ++count; }
};
struct ClipChanges : Clip::Listener {
    int changes = 0, invalidations = 0;
    void clipChanged() override { ++changes; }
    void notesInvalidated() override { ++invalidations; }
};
struct PoolChanges : ClipPool::Listener {
    explicit PoolChanges(ClipPool& p) : pool(p) { pool.addListener(this); }
    ~PoolChanges() override { pool.removeListener(this); }
    ClipPool& pool;
    int before = 0, after = 0;
    void clipAdded(ClipId, Clip*) override {}
    void clipChanged(ClipId, Clip*) override {}
    void clipWillBeRemoved(ClipId id) override { CHECK(pool.getClip(id)); ++before; }
    void clipRemoved(ClipId id) override { CHECK(!pool.getClip(id)); ++after; }
};
struct ChannelChanges : ChannelList::Listener {
    int changed = 0;
    ChannelId last = -1;
    void channelAdded(Channel*) override {}
    void channelRemoved(int) override {}
    void channelListChanged() override {}
    void channelChanged(Channel* c) override {
        CHECK(juce::MessageManager::getInstance()->isThisTheMessageThread());
        ++changed; last = c->getId();
    }
};

struct Probe {
    struct Event {
        long long time;
        int offset, channel, pitch, velocity;
        bool on;
    };
    Probe() { events.reserve(20000); }
    std::vector<Event> events;
    long long samples = 0;
    int blocks = 0, notes = 0, noteOffs = 0, sounding = 0, destroyed = 0, resets = 0;
    int lastInputCount = 0, lastOffSample = -1, editorCalls = 0, editorQueries = 0;
    bool destroyedQuiescent = false;
    bool invalidInputOffset = false;
    bool destroyedOnAudio = false, resetOnAudio = false, resetQuiescent = false, sustain = false;
    bool prepared = false, editorAvailable = true;
    bool constantOutput = false;
    float leftOutput = 0.25f, rightOutput = 0.25f;
    int resetsWhileUnprepared = 0;
    std::function<void()> duringReset;
    std::array<unsigned, 2048> held{};
};
class OfflineInstrument : public juce::AudioPluginInstance {
public:
    explicit OfflineInstrument(Probe& p) : probe(p) {}
    ~OfflineInstrument() override {
        ++probe.destroyed; probe.destroyedOnAudio = rendering;
        const bool admitted = AudioQuiescence::instance().enter();
        probe.destroyedQuiescent = !admitted;
        if (admitted) AudioQuiescence::instance().leave();
    }
    const juce::String getName() const override { return "Offline instrument"; }
    void fillInPluginDescription(juce::PluginDescription&) const override {}
    void prepareToPlay(double, int) override { probe.prepared = true; }
    void releaseResources() override { probe.prepared = false; }
    void reset() override {
        ++probe.resets; probe.resetOnAudio = rendering;
        if (!probe.prepared) ++probe.resetsWhileUnprepared;
        const bool admitted = AudioQuiescence::instance().enter();
        probe.resetQuiescent = !admitted;
        if (admitted) AudioQuiescence::instance().leave();
        if (probe.duringReset) probe.duringReset();
        probe.sounding = 0; probe.held.fill(0);
        // Pedal parameter deliberately survives reset, like some mapped controls.
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        ++probe.blocks;
        probe.lastInputCount = midi.getNumEvents();
        unsigned converted = 0;
        for (const auto metadata : midi) {
            probe.invalidInputOffset = probe.invalidInputOffset || metadata.samplePosition < 0 ||
                metadata.samplePosition >= buffer.getNumSamples();
            if (++converted > 2048) break; // JUCE counts ignored CCs too.
            if (metadata.numBytes != 3) continue;
            const auto status = metadata.data[0] & 0xf0;
            const auto index = (metadata.data[0] & 15) * 128 + (metadata.data[1] & 127);
            if ((status == 0x80 || status == 0x90) && probe.events.size() < probe.events.capacity())
                probe.events.push_back({probe.samples + metadata.samplePosition, metadata.samplePosition,
                    (metadata.data[0] & 15) + 1, metadata.data[1] & 127, metadata.data[2],
                    status == 0x90 && metadata.data[2] != 0});
            if (status == 0x90 && metadata.data[2] != 0) {
                ++probe.notes; ++probe.sounding; ++probe.held[index];
            } else if (status == 0x80 || status == 0x90) {
                ++probe.noteOffs; probe.lastOffSample = metadata.samplePosition;
                if (probe.held[index]) {
                    --probe.held[index];
                    if (!probe.sustain) --probe.sounding;
                }
            } else if (status == 0xb0 && metadata.data[1] == 64 && metadata.data[2] >= 64) {
                probe.sustain = true;
            }
            // Intentionally ignore ALL cleanup CCs, including sustain-off.
        }
        buffer.clear();
        probe.samples += buffer.getNumSamples();
        if (probe.sounding != 0 || probe.constantOutput)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                juce::FloatVectorOperations::fill(buffer.getWritePointer(ch),
                    ch == 0 ? probe.leftOutput : probe.rightOutput, buffer.getNumSamples());
    }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { ++probe.editorCalls; return nullptr; }
    bool hasEditor() const override { ++probe.editorQueries; return probe.editorAvailable; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    Probe& probe;
};

class EditorInstrument : public OfflineInstrument {
public:
    explicit EditorInstrument(Probe& p) : OfflineInstrument(p) {}
    bool editorDestroyedSafely = false;
    juce::AudioProcessorEditor* createEditor() override {
        CHECK(!AudioQuiescence::instance().enter());
        struct Editor : juce::AudioProcessorEditor {
            explicit Editor(EditorInstrument& p) : AudioProcessorEditor(p), owner(p) { setSize(100, 100); }
            ~Editor() override { owner.editorDestroyedSafely = !rendering && !AudioQuiescence::instance().enter(); }
            EditorInstrument& owner;
        };
        return new Editor(*this);
    }
};

// T07: fake plugin with real state capture so project round-trips exercise
// opaque plugin blobs without any third-party plugin.
class StatefulInstrument : public OfflineInstrument {
public:
    explicit StatefulInstrument(Probe& p) : OfflineInstrument(p) {}
    juce::MemoryBlock received;
    void fillInPluginDescription(juce::PluginDescription& description) const override {
        description.name = "Stateful instrument";
        description.descriptiveName = "Stateful instrument (offline)";
        description.pluginFormatName = "Offline";
        description.fileOrIdentifier = "/offline/stateful.vst3";
        description.manufacturerName = "vibedaw tests";
        description.version = "1.2.3";
        description.uniqueId = 0x7ea77;
        description.isInstrument = true;
        description.numInputChannels = 0;
        description.numOutputChannels = 2;
    }
    void getStateInformation(juce::MemoryBlock& block) override { block.append("STATE-BYTES", 11); }
    void setStateInformation(const void* data, int size) override { received.append(data, static_cast<std::size_t>(juce::jmax(0, size))); }
};

static void pluginEditorCreationTests() {
    Probe probe;
    Project project;
    auto* channel = project.getChannelList().addChannel("Editor");
    auto instance = std::make_unique<EditorInstrument>(probe);
    auto* instrument = instance.get();
    channel->setPlugin(std::make_unique<PluginHost>(std::move(instance)));
    project.setActiveChannel(0);
    PluginButton button(project);
    CHECK(button.isEnabled());
    ChannelRow row(channel, 0);
    auto menu = row.createContextMenu();
    juce::PopupMenu::MenuItemIterator items(menu);
    CHECK(items.next() && items.getItem().isEnabled);
    CHECK(channel->getPlugin()->hasEditor());
    {
        AudioQuiescence::Edit edit;
        auto editor = channel->getPlugin()->createEditor();
        CHECK(editor != nullptr);
        CHECK(!channel->getPlugin()->createEditor()); // No second owner of active editor.
        instrument->editorBeingDeleted(editor.get());
    }
    CHECK(instrument->editorDestroyedSafely);
    channel->setPlugin(nullptr);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    CHECK(probe.destroyed == 1 && probe.destroyedQuiescent);
}

static void rackDropAndClipCreationTests() {
    Probe first, replacement;
    Project project;
    ChannelRackContent rack(project);
    rack.setSize(250, 240);
    rack.setVisible(true); // Component hit testing requires visibility, but no native peer.
    int loads = 0;
    ProjectTestAccess::loader(project, [&](const juce::String& path) -> std::unique_ptr<PluginHost> {
        ++loads;
        CHECK(!AudioQuiescence::instance().enter());
        if (path == "fail") return nullptr;
        if (path == "unloaded") return std::make_unique<PluginHost>();
        auto instrument = std::make_unique<OfflineInstrument>(loads == 1 ? first : replacement);
        if (path == "surround") instrument->setPlayConfigDetails(0, 4, 44100.0, 512);
        return std::make_unique<PluginHost>(std::move(instrument));
    });
    PluginTreeItem plugin("Test", "/offline/test-plugin", true);
    auto payload = plugin.getDragSourceDescription();
    CHECK(DragDropInfo::fromDragDescription(payload).type == DragSourceType::Plugin);
    CHECK(DragDropInfo::fromDragDescription(payload).path == "/offline/test-plugin");
    using Details = juce::DragAndDropTarget::SourceDetails;
    Details empty(payload, nullptr, {20, ChannelRow::rowHeight * 2 + 8});
    juce::Component browserSource;
    const Details sourceDetails(payload, &browserSource, {900, 700});
    juce::DragAndDropTarget* currentTarget = nullptr;
    // Match JUCE findTarget/updateLocation: discovery sees source coordinates,
    // then events (including the previous target's exit check) see new-target coordinates.
    auto dispatchMove = [&](juce::Point<int> rackPosition) {
        auto details = sourceDetails;
        juce::DragAndDropTarget* target = nullptr;
        juce::Point<int> localPosition;
        for (auto* hit = rack.getComponentAt(rackPosition); hit; hit = hit->getParentComponent()) {
            auto* candidate = dynamic_cast<juce::DragAndDropTarget*>(hit);
            if (candidate && candidate->isInterestedInDragSource(sourceDetails)) {
                target = candidate;
                localPosition = hit->getLocalPoint(&rack, rackPosition);
                break;
            }
        }
        details.localPosition = localPosition;
        if (target != currentTarget) {
            if (currentTarget && currentTarget->isInterestedInDragSource(details))
                currentTarget->itemDragExit(details);
            currentTarget = target;
            if (target && target->isInterestedInDragSource(details)) target->itemDragEnter(details);
        }
        if (target && target->isInterestedInDragSource(details)) target->itemDragMove(details);
        return target;
    };
    auto* clipPayload = new juce::DynamicObject();
    clipPayload->setProperty("type", "vibedaw.clip");
    clipPayload->setProperty("clipId", 1);
    for (auto invalid : {juce::var(), juce::var(42), juce::var("42"), juce::var("/tmp/plugin.vst3"),
                         juce::var("preset:///tmp/preset"), juce::var(clipPayload), DragDropInfo::plugin(""),
                         DragDropInfo::plugin("relative-path")}) {
        Details details(invalid, nullptr, {20, 80});
        CHECK(!rack.isInterestedInDragSource(details));
        rack.itemDragEnter(details);
        rack.itemDropped(details);
    }
    CHECK(loads == 0 && project.getActiveChannelId() == InvalidChannelId);
    CHECK(rack.isInterestedInDragSource(empty));
    CHECK(dispatchMove(empty.localPosition) == &rack);
    juce::Image rackPreview(juce::Image::RGB, 250, 240, true);
    juce::Graphics rackGraphics(rackPreview);
    rack.paint(rackGraphics);
    CHECK(rackPreview.getPixelAt(3, 100) == theme::dropFill);
    CHECK(dispatchMove({300, 80}) == nullptr); // Cancellation never loads.
    rack.paint(rackGraphics);
    CHECK(rackPreview.getPixelAt(3, 100) == theme::raised);
    CHECK(loads == 0 && project.getChannelList().getNumChannels() == 0);
    CHECK(dispatchMove(empty.localPosition) == &rack);
    auto dropDetails = sourceDetails;
    dropDetails.localPosition = empty.localPosition;
    currentTarget = nullptr; // JUCE clears the current target before itemDropped.
    rack.itemDropped(dropDetails);
    CHECK(loads == 1 && project.getChannelList().getNumChannels() == 1);
    auto* original = project.getChannelList().getChannel(0);
    const auto originalId = original->getId();
    CHECK(original->hasPlugin() && original->getName() == "Offline instrument");
    CHECK(original->getType() == Channel::Type::Instrument);
    CHECK(project.getActiveChannelId() == originalId);
    Details rowDrop(payload, nullptr, {20, 27});
    CHECK(rack.isInterestedInDragSource(rowDrop)); // Interest must not interpret these coordinates.
    rack.itemDropped(rowDrop); // Even direct parent dispatch cannot also create.
    CHECK(loads == 1);
    CHECK(rack.isInterestedInDragSource(Details(payload, nullptr, {20, ChannelRow::rowHeight})));
    CHECK(rack.isInterestedInDragSource(Details(payload, nullptr, {20, 225})));
    CHECK(rack.isInterestedInDragSource(Details(payload, nullptr, {250, 80})));
    rack.itemDropped(Details(payload, nullptr, {250, 80})); // Commit still excludes outside geometry.
    CHECK(loads == 1);
    auto* other = project.getChannelList().addChannel("Other");
    project.setActiveChannel(1);
    auto* oldHost = original->getPlugin();
    CHECK(!project.loadPlugin("fail", originalId));
    CHECK(original->getPlugin() == oldHost && project.getActiveChannelId() == other->getId());
    CHECK(!project.loadPlugin("fail"));
    CHECK(project.getChannelList().getNumChannels() == 2 && project.getActiveChannelId() == other->getId());
    CHECK(!project.loadPlugin("unloaded"));
    CHECK(!project.loadPlugin("surround"));
    CHECK(!project.loadPlugin("surround", originalId));
    CHECK(original->getPlugin() == oldHost && project.getChannelList().getNumChannels() == 2);
    auto* row = dynamic_cast<ChannelRow*>(rack.getChildComponent(1));
    CHECK(row && row->getChannel() == original);
    CHECK(row->isInterestedInDragSource(rowDrop));
    CHECK(!row->isInterestedInDragSource(Details("preset:///tmp/no", nullptr, {})));
    juce::Image preview(juce::Image::RGB, row->getWidth(), row->getHeight(), true);
    juce::Graphics previewGraphics(preview);
    row->paint(previewGraphics);
    // Inside the card, away from its rounded outline, labels and controls.
    const auto rowInterior = juce::Point<int>(row->getWidth() - 8, row->getHeight() / 2);
    const auto idleRowColour = preview.getPixelAt(rowInterior.x, rowInterior.y);
    auto* addButton = dynamic_cast<juce::TextButton*>(rack.getChildComponent(0));
    CHECK(addButton);
    CHECK(dispatchMove(empty.localPosition) == &rack);
    CHECK(addButton->getButtonText() == "Create instrument channel");
    CHECK(dispatchMove({20, 27}) == row); // Accepting child wins over interested parent.
    rack.paint(rackGraphics);
    CHECK(rackPreview.getPixelAt(3, 100) == theme::raised);
    CHECK(addButton->getButtonText() == "+ Add Channel");
    row->paint(previewGraphics);
    const auto dropRowColour = preview.getPixelAt(rowInterior.x, rowInterior.y);
    CHECK(dropRowColour != idleRowColour);
    CHECK(dispatchMove({300, 80}) == nullptr);
    row->paint(previewGraphics);
    CHECK(preview.getPixelAt(rowInterior.x, rowInterior.y) == idleRowColour);
    CHECK(dispatchMove(empty.localPosition) == &rack);
    CHECK(dispatchMove({300, 80}) == nullptr); // Exit recheck uses zero, which overlaps row 0.
    rack.paint(rackGraphics);
    CHECK(rackPreview.getPixelAt(3, 100) == theme::raised);
    CHECK(addButton->getButtonText() == "+ Add Channel");
    rack.itemDragEnter(empty);
    rack.itemDragMove(rowDrop); // Defensive geometry check even if sent to the parent directly.
    CHECK(addButton->getButtonText() == "+ Add Channel");
    rack.itemDragMove(Details(payload, nullptr, {250, 80}));
    CHECK(addButton->getButtonText() == "+ Add Channel");
    row->itemDragEnter(rowDrop);
    row->paint(previewGraphics);
    CHECK(preview.getPixelAt(rowInterior.x, rowInterior.y) == dropRowColour);
    row->itemDragExit(rowDrop);
    row->itemDragEnter(Details("preset:///tmp/no", nullptr, {}));
    row->paint(previewGraphics);
    CHECK(preview.getPixelAt(rowInterior.x, rowInterior.y) == idleRowColour);
    row->itemDropped(rowDrop);
    CHECK(original->getPlugin() != oldHost && first.destroyed == 1 && first.destroyedQuiescent);
    CHECK(project.getActiveChannelId() == other->getId());
    const auto sampleFile = juce::File::getCurrentWorkingDirectory().getChildFile("CMakeCache.txt");
    CHECK(sampleFile.existsAsFile());
    Details sample("sample://" + sampleFile.getFullPathName(), nullptr, {});
    CHECK(row->isInterestedInDragSource(sample));
    CHECK(!rack.isInterestedInDragSource(sample));
    row->itemDropped(sample);
    CHECK(original->getSampleFile() == sampleFile && original->getPlugin() != nullptr);
    rack.itemDropped(empty); // Populated unused space creates once, not replacement.
    CHECK(project.getChannelList().getNumChannels() == 3);
    CHECK(project.getActiveChannelId() != other->getId());
    rack.setSize(250, 60); // Rows cannot steal the add-button region in a short rack.
    rack.itemDragEnter(Details(payload, nullptr, {20, 45}));
    CHECK(addButton->getButtonText() == "Create instrument channel");
    rack.itemDragMove(Details(payload, nullptr, {20, 27}));
    CHECK(addButton->getButtonText() == "+ Add Channel");
    rack.setSize(250, 240);
    const auto selected = project.getActiveChannelId();
    while (project.getChannelList().getNumChannels() < ChannelList::maxChannels)
        CHECK(project.getChannelList().addChannel());
    const int before = loads;
    CHECK(!project.loadPlugin("test-plugin"));
    CHECK(loads == before && project.getActiveChannelId() == selected);
    CHECK(!rack.isInterestedInDragSource(empty));
    rack.itemDropped(empty);
    CHECK(loads == before);
    CHECK(project.loadPlugin("test-plugin", originalId)); // Replacement allowed at capacity.
    project.getChannelList().removeChannel(0);
    const int beforeMissing = loads;
    CHECK(!project.loadPlugin("test-plugin", originalId) && loads == beforeMissing);

    struct ClipActions : ClipsContent::Listener {
        int created = 0, opened = 0;
        ClipId selected = InvalidClipId;
        void clipCreated(ClipId id, Clip*) override { ++created; CHECK(selected == id); }
        void clipSelected(ClipId id, Clip*) override { selected = id; }
        void clipOpened(ClipId id, Clip*) override { ++opened; CHECK(selected == id); }
    } actions;
    auto clips = std::make_unique<ClipsContent>(project);
    clips->setClipsListener(&actions);
    auto* button = dynamic_cast<juce::TextButton*>(clips->getChildComponent(0));
    CHECK(button && button->getButtonText() == "+ New Clip");
    button->onClick();
    CHECK(actions.created == 1 && actions.opened == 0 && actions.selected != InvalidClipId);
    const auto sourceId = actions.selected;
    CHECK(project.getClipPool().getClip(sourceId)->getDuration() == 4.0);
    CHECK(project.getTrackList().getNumTracks() == 0);
    CHECK(project.getActiveChannelId() == selected);
    auto* clipRow = dynamic_cast<ClipRow*>(clips->getChildComponent(2));
    CHECK(clipRow && clipRow->isSelected());
    auto edit = clipRow->editSource;
    edit();
    CHECK(actions.opened == 1);
    const juce::MouseEvent doubleClick(juce::Desktop::getInstance().getMainMouseSource(), {5, 5},
        juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        clipRow, clipRow, juce::Time::getCurrentTime(), {5, 5}, juce::Time::getCurrentTime(), 2, false);
    clipRow->mouseDoubleClick(doubleClick);
    CHECK(actions.opened == 2);
    auto menu = clipRow->createContextMenu();
    CHECK(menu.getNumItems() == 3); // Edit, Rename, Delete Source.
    auto renameAction = clipRow->renameRequested;
    auto deleteAction = clipRow->deleteSourceRequested;
    PoolChanges removal(project.getClipPool());
    project.getClipPool().removeClip(sourceId);
    CHECK(removal.before == 1 && removal.after == 1);
    edit(); // Delayed context action resolves ID, not a deleted row/source.
    CHECK(actions.opened == 2 && actions.selected == InvalidClipId);
    renameAction(); // Deleted source: harmless no-ops, no stale-pointer access.
    deleteAction();
    CHECK(actions.opened == 2);
    clips.reset();
    edit();
    renameAction();
    deleteAction();
    CHECK(actions.opened == 2);
}

static void timelineDragTests() {
    using G = TimelineGeometry;
    CHECK(G::beatAt(25, 175, 100) == 2 && G::xAt(2, 175, 100) == 25);
    CHECK(G::startAt(38, 100, 100, 0.25, false) == 1.25);
    CHECK(std::abs(G::startAt(38, 100, 100, 0.25, true) - 1.13) < 1e-12);
    CHECK(G::startAt(-100, 0, 50, 1, false) == 0);
    CHECK(G::startAt(std::numeric_limits<double>::infinity(), 0, 50, 0, false) == 0);
    CHECK(G::laneAt(0, 24, 500, 300, 24, 0, 64, 0) == 0);
    CHECK(G::laneAt(0, 87, 500, 300, 24, 0, 64, 2) == 0);
    CHECK(G::laneAt(0, 88, 500, 300, 24, 0, 64, 2) == 1);
    CHECK(G::laneAt(0, 152, 500, 300, 24, 0, 64, 2) == 2);
    CHECK(G::laneAt(0, 24, 500, 300, 24, 64, 64, 2) == 1);
    for (auto p : {juce::Point<int>(-1, 30), {500, 30}, {10, 23}, {10, 300}})
        CHECK(G::laneAt(p.x, p.y, 500, 300, 24, 0, 64, 2) == -1);
    CHECK(G::edgeDelta(0, 0, 500) == -12 && G::edgeDelta(499, 0, 500) == 12);
    CHECK(G::edgeDelta(250, 0, 500) == 0 && G::edgeDelta(500, 0, 500) == 0);

    Project project;
    auto& tracks = project.getTrackList();
    auto& pool = project.getClipPool();
    auto& channels = project.getChannelList();
    auto source = std::make_unique<MidiClip>(0, 4);
    source->addNote(Note(60, 0, 1));
    const auto clipId = pool.addClip(std::move(source));
    ClipRow row(clipId, pool.getClip(clipId), 0);
    const auto payload = row.getDragDescription();
    CHECK(DragDropInfo::fromDragDescription(payload).clipId == clipId);
    CHECK(DragDropInfo::fromDragDescription(payload).type == DragSourceType::Clip);
    TimelinePanel panel(project); panel.setSize(800, 420); panel.setVisible(true);
    TimelineContent* content = nullptr;
    for (auto* child : panel.getChildren()) if (auto* c = dynamic_cast<TimelineContent*>(child)) content = c;
    CHECK(content);
    using Details = juce::DragAndDropTarget::SourceDetails;
    const Details discovery(payload, &row, {900, 700});
    CHECK(content->isInterestedInDragSource(discovery));
    for (const auto invalid : {juce::var(clipId), juce::var("0"), DragDropInfo::plugin("/offline/plugin"), DragDropInfo::clip(-1)})
        CHECK(!content->isInterestedInDragSource(Details(invalid, &row, {})));
    auto enter = [&](juce::Point<int> point) {
        // Actual child-first component discovery, with SOURCE coordinates for interest.
        juce::DragAndDropTarget* target = nullptr;
        for (auto* hit = content->getComponentAt(point); hit; hit = hit->getParentComponent()) {
            if (auto* ddt = dynamic_cast<juce::DragAndDropTarget*>(hit))
                if (ddt->isInterestedInDragSource(discovery)) { target = ddt; break; }
        }
        CHECK(target == content);
        target->itemDragEnter(Details(payload, &row, point));
    };
    auto drop = [&](juce::Point<int> point) { content->itemDropped(Details(payload, &row, point)); };
    enter({100, 40});
    CHECK(content->getPreview().visible && !content->getPreview().valid);
    CHECK(content->getPreview().label.contains("Select an instrument"));
    drop({100, 40}); CHECK(tracks.getNumTracks() == 0);
    auto* instrument = channels.addChannel("Alpha"); const auto destination = instrument->getId();
    project.setActiveChannel(0);
    ArrangementPublisher publisher(tracks, pool, channels);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
    const auto revision = publisher.acquire().revision;
    enter({113, 40});
    CHECK(content->getPreview().newTrack && content->getPreview().valid);
    CHECK(content->getPreview().beat == 2.25 && content->getPreview().duration == 4);
    CHECK(content->getPreview().label.contains("Alpha") && content->getPreview().label.contains("no plugin"));
    juce::Image ghost(juce::Image::RGB, content->getWidth(), content->getHeight(), true);
    juce::Graphics ghostGraphics(ghost);
    content->paintOverChildren(ghostGraphics);
    CHECK(ghost.getPixelAt(8, 24) == theme::dragValid); // Outside the playhead glow.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    CHECK(publisher.acquire().revision == revision && tracks.getNumTracks() == 0);
    CHECK(content->isInterestedInDragSource(Details(payload, &row, {})));
    content->itemDragExit(Details(payload, &row, {}));
    CHECK(!content->getPreview().visible && tracks.getNumTracks() == 0);
    enter({113, 40}); drop({113, 40});
    CHECK(tracks.getNumTracks() == 1 && tracks.getTrack(0)->getNumClipInstances() == 1);
    auto* placement = tracks.getTrack(0)->getClipInstance(0);
    const auto identity = placement->getId();
    CHECK(placement->getStartTime() == 2.25 && placement->getChannelId() == destination);
    CHECK(placement->isSelected());
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    CHECK(publisher.acquire().notes.size() == 1 && publisher.acquire().notes[0].start == 2.25);
    enter({10, 24}); CHECK(!content->getPreview().newTrack);
    drop({10, 23}); CHECK(tracks.getNumTracks() == 1 && tracks.getTrack(0)->getNumClipInstances() == 1);
    enter({20, 88}); CHECK(content->getPreview().newTrack);
    drop({20, 88}); CHECK(tracks.getNumTracks() == 2);
    auto* first = tracks.getTrack(0); auto* second = tracks.getTrack(1);
    const auto firstId = first->getId(), secondId = second->getId();
    auto event = [&](juce::Point<float> point, juce::Point<float> down, int modifiers = juce::ModifierKeys::leftButtonModifier) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point, modifiers,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, content, content, juce::Time::getCurrentTime(),
            down, juce::Time::getCurrentTime(), 1, point != down);
    };
    auto* lane = dynamic_cast<TimelineLane*>(content->getComponentAt(juce::Point<int>(125, 40)));
    CHECK(lane);
    auto downOnLane = event({125, 40}, {125, 40}).getEventRelativeTo(lane);
    lane->mouseDown(downOnLane); // JUCE calls the component, then its registered parent listener.
    content->mouseDown(downOnLane);
    content->mouseDrag(event({150, 40}, {125, 40}).getEventRelativeTo(lane));
    CHECK(content->getPreview().beat == 2.75 && placement->getStartTime() == 2.25);
    content->mouseUp(event({150, 40}, {125, 40}, 0).getEventRelativeTo(lane));
    CHECK(first->getClipInstance(0) == placement && placement->getStartTime() == 2.75);
    content->mouseDown(event({150, 40}, {150, 40}));
    content->mouseDrag(event({125, 40}, {150, 40}));
    content->mouseUp(event({125, 40}, {150, 40}, 0));
    CHECK(placement->getStartTime() == 2.25);
    placement->setDuration(3.5); placement->setMuted(true);
    channels.addChannel("Beta"); project.setActiveChannel(1);
    content->mouseDown(event({125, 40}, {125, 40})); // grab = .25 beats
    content->mouseDrag(event({128, 40}, {125, 40}));
    CHECK(!content->getPreview().visible && placement->getStartTime() == 2.25);
    content->mouseDrag(event({175, 110}, {125, 40}));
    CHECK(content->getPreview().beat == 3.25 && content->getPreview().trackId == secondId);
    CHECK(content->getPreview().destination == destination && placement->getStartTime() == 2.25);
    content->mouseUp(event({175, 110}, {125, 40}, 0));
    CHECK(first->getNumClipInstances() == 0 && second->getNumClipInstances() == 2);
    CHECK(second->getClipInstance(1) == placement && placement->getId() == identity);
    CHECK(placement->getClipId() == clipId && placement->getDuration() == 3.5 && placement->isMuted());
    CHECK(placement->getChannelId() == destination && placement->getStartTime() == 3.25 && placement->isSelected());
    int opened = 0;
    panel.onEditSource = [&](ClipId id) { CHECK(id == clipId); ++opened; };
    content->mouseDoubleClick(event({175, 110}, {175, 110})); CHECK(opened == 1);
    content->mouseDown(event({175, 110}, {175, 110}));
    content->mouseDrag(event({200, 180}, {175, 110}));
    CHECK(content->getPreview().newTrack);
    content->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    content->mouseUp(event({200, 180}, {175, 110}, 0));
    CHECK(tracks.getNumTracks() == 2 && placement->getStartTime() == 3.25);
    content->mouseDown(event({175, 110}, {175, 110}));
    content->mouseDrag(event({200, 180}, {175, 110}));
    content->mouseUp(event({200, 180}, {175, 110}, 0));
    CHECK(tracks.getNumTracks() == 3 && tracks.getTrack(2)->getClipInstance(0) == placement);
    CHECK(placement->getId() == identity && placement->getStartTime() == 3.75);
    content->setPixelsPerBeat(100); content->setScrollOffset(64, 300);
    content->mouseDown(event({100, 110}, {100, 110})); // third lane, grab .25
    content->mouseDrag(event({138, 40}, {100, 110}, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier));
    CHECK(content->getPreview().trackId == secondId && std::abs(content->getPreview().beat - 4.13) < 1e-12);
    content->mouseUp(event({138, 40}, {100, 110}, juce::ModifierKeys::altModifier));
    CHECK(second->getClipInstance(1) == placement && std::abs(placement->getStartTime() - 4.13) < 1e-12);
    content->setScrollOffset(0, 0); content->setPixelsPerBeat(50);
    const auto before = placement->getStartTime();
    content->mouseDown(event({220, 110}, {220, 110}));
    content->mouseDrag(event({250, 40}, {220, 110}));
    content->mouseUp(event({-1, 40}, {220, 110}, 0));
    CHECK(placement->getStartTime() == before && second->getClipInstance(1) == placement);
    content->mouseDown(event({220, 110}, {220, 110}));
    content->mouseDrag(event({250, 40}, {220, 110}));
    pool.removeClip(clipId); // Resolved source vanishes mid-move: reject.
    content->mouseUp(event({250, 40}, {220, 110}, 0));
    CHECK(placement->getStartTime() == before && second->getClipInstance(1) == placement);
    content->mouseDoubleClick(event({220, 110}, {220, 110})); CHECK(opened == 1);
    content->mouseDown(event({220, 110}, {220, 110})); // Already unresolved can move safely.
    content->mouseDrag(event({250, 40}, {220, 110}));
    content->mouseUp(event({250, 40}, {220, 110}, 0));
    CHECK(first->getClipInstance(0) == placement && placement->getChannelId() == destination);
    enter({100, 240}); CHECK(!content->getPreview().valid); drop({100, 240});
    CHECK(tracks.getNumTracks() == 3);
    CHECK(!tracks.commitPlacement("vanished", 0, firstId, identity));
    CHECK(!tracks.commitPlacement({}, 0, firstId, "vanished"));
    CHECK(!tracks.commitPlacement({}, -1, firstId, identity));
    CHECK(!tracks.commitPlacement({}, std::numeric_limits<double>::infinity(), firstId, identity));
    CHECK(tracks.getNumTracks() == 3 && first->getClipInstance(0) == placement);
    const auto finalId = placement->getId();
    first->removeClipInstance(0);
    CHECK(!tracks.commitPlacement({}, 0, firstId, finalId) && tracks.getNumTracks() == 3);
    tracks.removeTrack(0);
    CHECK(!tracks.getTrackById(firstId));
    CHECK(!tracks.commitPlacement(firstId, 0, {}, {}, std::make_unique<ClipInstance>(clipId, destination, 0, 4)));
    CHECK(tracks.getNumTracks() == 2);
}

static void timelineInvalidationTests() {
    Project project;
    auto& tracks = project.getTrackList(); auto& clips = project.getClipPool(); auto& channels = project.getChannelList();
    const auto id = clips.addClip(std::make_unique<MidiClip>(0, 4));
    channels.addChannel("Sampler", Channel::Type::Sampler);
    project.setActiveChannel(0);
    TimelinePanel panel(project); panel.setSize(800, 420); panel.setVisible(true);
    TimelineContent* content = nullptr;
    juce::ScrollBar *horizontal = nullptr, *vertical = nullptr;
    for (auto* child : panel.getChildren()) {
        if (auto* c = dynamic_cast<TimelineContent*>(child)) content = c;
        if (auto* bar = dynamic_cast<juce::ScrollBar*>(child)) (bar->isVertical() ? vertical : horizontal) = bar;
    }
    CHECK(content && horizontal && vertical);
    using Details = juce::DragAndDropTarget::SourceDetails;
    Details drop(DragDropInfo::clip(id), nullptr, {100, 40});
    content->itemDragEnter(drop); CHECK(!content->getPreview().valid);
    content->itemDropped(drop); CHECK(tracks.getNumTracks() == 0);
    channels.addChannel("Instrument"); project.setActiveChannel(1);
    content->itemDragEnter(drop); CHECK(content->getPreview().valid);
    channels.removeChannel(1);
    content->itemDropped(drop); CHECK(tracks.getNumTracks() == 0);
    channels.addChannel("Replacement"); project.setActiveChannel(1);
    for (int i = 0; i < 8; ++i) tracks.addTrack();
    drop.description = DragDropInfo::clip(id); // A new press after structural edits.
    const auto staleTrack = tracks.getTrack(0)->getId();
    content->itemDragEnter(drop); CHECK(content->getPreview().trackId == staleTrack);
    tracks.removeTrack(0); // Rebuild cancels; do not reinterpret the old y as the next track.
    CHECK(!content->getPreview().visible);
    content->itemDropped(drop);
    for (const auto& track : tracks.getTracks()) CHECK(track->getNumClipInstances() == 0);
    drop.description = DragDropInfo::clip(id); // The deleted-target gesture stays cancelled.
    auto edge = drop; edge.localPosition = {content->getWidth() - 1, content->getHeight() - 1};
    content->itemDragEnter(edge);
    TimelineContentTestAccess::tick(*content);
    CHECK(horizontal->getCurrentRangeStart() == 12 && vertical->getCurrentRangeStart() == 12);
    CHECK(content->getPreview().beat == TimelineGeometry::startAt(edge.localPosition.x, 12, 50, 0, false));
    for (int i = 0; i < 400; ++i) TimelineContentTestAccess::tick(*content);
    CHECK(horizontal->getCurrentRangeStart() == horizontal->getMaximumRangeLimit() - horizontal->getCurrentRangeSize());
    CHECK(vertical->getCurrentRangeStart() == vertical->getMaximumRangeLimit() - vertical->getCurrentRangeSize());
    CHECK(content->getPreview().newTrack); // The extra scroll row makes below-lanes reachable.
    const double scroll = horizontal->getCurrentRangeStart();
    auto outside = edge; outside.localPosition.x = -1;
    content->itemDragMove(outside); TimelineContentTestAccess::tick(*content);
    CHECK(!content->getPreview().visible && horizontal->getCurrentRangeStart() == scroll);
    content->itemDragExit(Details(drop.description, nullptr, {}));
    TimelineContentTestAccess::tick(*content);
    CHECK(horizontal->getCurrentRangeStart() == scroll);
    horizontal->setCurrentRangeStart(0, juce::sendNotificationSync);
    vertical->setCurrentRangeStart(0, juce::sendNotificationSync);
    content->itemDragEnter(drop);
    clips.removeClip(id);
    content->itemDropped(drop);
    for (const auto& track : tracks.getTracks()) CHECK(track->getNumClipInstances() == 0);
    auto vanishedRow = std::make_unique<juce::Component>();
    content->itemDragEnter(Details(drop.description, vanishedRow.get(), {100, 40}));
    CHECK(content->getPreview().visible);
    vanishedRow.reset();
    TimelineContentTestAccess::tick(*content);
    CHECK(!content->getPreview().visible);
}

static void timelineGestureReviewTests() {
    Project project;
    auto& tracks = project.getTrackList(); auto& clips = project.getClipPool();
    const auto id = clips.addClip(std::make_unique<MidiClip>(0, 4));
    project.getChannelList().addChannel("Alpha");
    project.getChannelList().addChannel("Beta");
    project.setActiveChannel(0);
    ClipRow row(id, clips.getClip(id), 0);
    TimelineContent docked(tracks, clips, project.getChannelList(), project.getTransportState());
    TimelineContent detached(tracks, clips, project.getChannelList(), project.getTransportState());
    for (auto* target : {&docked, &detached}) {
        target->setSize(500, 300);
        target->activeDestination = [&] { return project.getActiveChannelId(); };
    }
    auto event = [&](float x, int mods = juce::ModifierKeys::leftButtonModifier) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {x, 4}, mods,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &row, &row, juce::Time::getCurrentTime(),
            {4, 4}, juce::Time::getCurrentTime(), 1, x != 4);
    };
    int starts = 0;
    juce::var payload;
    ClipRowTestAccess::starter(row, [&](const juce::var& description, bool acrossWindows) {
        CHECK(acrossWindows); // Production forwards true to JUCE startDragging.
        ++starts; payload = description;
    });
    using Details = juce::DragAndDropTarget::SourceDetails;
    auto details = [&](int y = 40) { return Details(payload, &row, {100, y}); };
    auto begin = [&] { row.mouseDown(event(4)); row.mouseDrag(event(20)); };
    auto count = [&] { int total = 0; for (const auto& track : tracks.getTracks()) total += track->getNumClipInstances(); return total; };
    begin(); CHECK(starts == 1);
    row.mouseDrag(event(30)); CHECK(starts == 1);
    // Even if JUCE Escape destroyed its drag image, continued mouseDrag cannot restart it.
    row.mouseDrag(event(40)); CHECK(starts == 1);
    docked.itemDragEnter(details());
    docked.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    row.mouseDrag(event(45)); CHECK(starts == 1);
    docked.itemDragExit(details());
    docked.itemDragEnter(details()); docked.itemDragMove(details());
    CHECK(!docked.getPreview().visible);
    docked.itemDropped(details());
    detached.itemDragEnter(details()); detached.itemDropped(details()); // Same gesture, different target.
    CHECK(tracks.getNumTracks() == 0 && count() == 0);
    row.mouseUp(event(45, 0)); row.mouseDrag(event(50)); CHECK(starts == 1);
    begin(); CHECK(starts == 2);
    row.mouseUp(event(20, 0)); // Component mouseUp precedes JUCE's listener/drop.
    detached.itemDropped(details()); // Release-only discovery, empty timeline, no enter/move.
    CHECK(tracks.getNumTracks() == 1 && count() == 1);
    CHECK(tracks.getTrack(0)->getClipInstance(0)->getStartTime() == 2);
    begin(); docked.itemDropped(details()); // Release-only existing lane.
    CHECK(tracks.getNumTracks() == 1 && count() == 2);
    begin(); docked.itemDragEnter(details(2));
    CHECK(!docked.getPreview().visible);
    docked.itemDropped(details()); // Ruler directly to valid lane on release.
    CHECK(tracks.getNumTracks() == 1 && count() == 3);
    begin(); docked.itemDragEnter(details(2)); docked.itemDropped(details(88));
    CHECK(tracks.getNumTracks() == 2 && count() == 4); // Ruler -> new lane.
    begin(); docked.itemDragEnter(details()); CHECK(docked.getPreview().valid);
    project.setActiveChannel(1); docked.itemDropped(details());
    CHECK(count() == 4); // A genuinely valid preview must not silently change route.
    begin(); docked.itemDragEnter(details(88));
    tracks.removeTrack(1); CHECK(count() == 3);
    docked.itemDragEnter(details(88)); docked.itemDropped(details(88));
    CHECK(tracks.getNumTracks() == 1 && count() == 3); // Stale-structure cancellation cannot revive.
    begin(); docked.itemDragEnter(details()); docked.itemDragExit(details(2));
    docked.itemDropped(details()); // Ordinary exit is not explicit gesture cancellation.
    CHECK(count() == 4);
    begin(); docked.itemDragEnter(details());
    docked.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    docked.itemDropped(details(88)); // Cancelled release-only new lane is still rejected.
    CHECK(tracks.getNumTracks() == 1 && count() == 4);
}

static void timelineCommitTests() {
    TrackList tracks; ClipPool clips; ChannelList channels;
    auto* channel = channels.addChannel("Route");
    auto clip = std::make_unique<MidiClip>(0, 4); clip->addNote(Note(60, 0, 1));
    const auto id = clips.addClip(std::move(clip));
    auto* source = tracks.addTrack();
    source->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 1, 3));
    auto* instance = source->getClipInstance(0);
    const auto trackId = source->getId(), instanceId = instance->getId();
    ArrangementPublisher publisher(tracks, clips, channels);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
    const auto& old = publisher.acquire();
    CHECK(old.notes.size() == 1 && old.notes[0].start == 1);
    struct CommitObserver : TrackList::Listener {
        TrackList& tracks; ClipPool& clips; ChannelList& channels;
        int additions = 0;
        CommitObserver(TrackList& t, ClipPool& p, ChannelList& c) : tracks(t), clips(p), channels(c) { tracks.addListener(this); }
        ~CommitObserver() override { tracks.removeListener(this); }
        void trackAdded(Track* track) override {
            ++additions;
            CHECK(track->getNumClipInstances() == 1);
            const auto snapshot = compileArrangement(tracks, clips, channels, 0);
            CHECK(snapshot.notes.size() == 1 && snapshot.notes[0].start == 5);
        }
        void trackRemoved(int) override {}
        void trackChanged(Track*) override {}
        void trackListChanged() override {}
    } observer(tracks, clips, channels);
    CHECK(!tracks.commitPlacement({}, 5, trackId, "stale"));
    CHECK(!tracks.commitPlacement({}, 5, {}, instanceId));
    CHECK(tracks.getNumTracks() == 1 && source->getClipInstance(0) == instance);
    CHECK(instance->getStartTime() == 1 && observer.additions == 0);
    CHECK(tracks.commitPlacement({}, 5, trackId, instanceId) == instance);
    CHECK(observer.additions == 1 && tracks.getNumTracks() == 2);
    CHECK(source->getNumClipInstances() == 0 && instance->getId() == instanceId);
    CHECK(old.notes[0].start == 1); // Held audio snapshot never sees partial transfer.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
    CHECK(publisher.acquire().notes.size() == 1 && publisher.acquire().notes[0].start == 5);
    CHECK(publisher.acquire().notes[0].destination == channel->getId());
}

static void sidebarRailTests() {
    { // Left rail geometry and stable tab bindings.
        SidebarContainer left(Sidebar::Side::Left);
        auto browser = std::make_unique<Sidebar>("Browser", Sidebar::Side::Left);
        auto rack = std::make_unique<Sidebar>("Channel Rack", Sidebar::Side::Left);
        browser->setSidebarWidth(220);
        rack->setSidebarWidth(250);
        left.addSidebar(browser.get());
        left.addSidebar(rack.get());
        left.setBounds(0, 0, 470, 600);
        left.constrainTo(470);

        CHECK(left.getTotalWidth() == 470 && left.getDisplayedWidth() == 470);
        CHECK(browser->getX() == 0 && browser->getWidth() == 220);
        CHECK(rack->getX() == 220 && rack->getWidth() == 250);

        browser->setExpanded(false);
        CHECK(left.getTotalWidth() == 278); // One 28px rail, not 28px per collapsed sidebar.
        CHECK(left.getWidth() == 278);
        CHECK(!browser->isVisible() && rack->isVisible());
        CHECK(rack->getX() == 28);
        CHECK(left.getTabCount() == 1);
        auto* tab = left.getTab(0);
        CHECK(tab && &tab->sidebar() == browser.get());
        CHECK(tab->getBounds() == juce::Rectangle<int>(0, 0, 28, 28));
        CHECK(left.getLocalBounds().contains(tab->getBounds()));
        CHECK(tab->getTooltip() == "Restore Browser");

        rack->setExpanded(false);
        CHECK(left.getTotalWidth() == 28 && left.getWidth() == 28); // No blank reserved column.
        CHECK(left.getTabCount() == 2);
        auto* browserTab = left.getTab(0);
        auto* rackTab = left.getTab(1);
        CHECK(&browserTab->sidebar() == browser.get() && &rackTab->sidebar() == rack.get());
        CHECK(browserTab->getBounds() == juce::Rectangle<int>(0, 0, 28, 28));
        CHECK(rackTab->getBounds() == juce::Rectangle<int>(0, 28, 28, 28));
        CHECK(left.getLocalBounds().contains(rackTab->getBounds()));
        CHECK(rackTab->getTooltip() == "Restore Channel Rack");

        // Equal-count membership changes must rebind tabs, not leave stale bindings.
        browser->setExpanded(true);
        CHECK(left.getTabCount() == 1 && &left.getTab(0)->sidebar() == rack.get());
        rack->setExpanded(true);
        CHECK(left.getTabCount() == 0 && left.getWidth() == 470);
        browser->setExpanded(false);
        rack->setExpanded(false);
        CHECK(left.getTabCount() == 2);
        browser->setExpanded(true);
        rack->setExpanded(false); // One expanded, one collapsed: same tab count, different owner.
        auto* reboundTab = left.getTab(0);
        CHECK(&reboundTab->sidebar() == rack.get());
        reboundTab->activate();
        CHECK(browser->isExpanded() && rack->isExpanded() && left.getTabCount() == 0);

        // Each tab operates exactly its own panel.
        browser->setExpanded(false);
        rack->setExpanded(false);
        CHECK(left.getTabCount() == 2);
        left.getTab(1)->activate();
        CHECK(rack->isExpanded() && !browser->isExpanded());
        left.getTab(0)->activate();
        CHECK(browser->isExpanded() && rack->isExpanded());
    }

    { // Right rail: collapsed tab stays inside the container (expanded-width offset regression).
        SidebarContainer right(Sidebar::Side::Right);
        auto clips = std::make_unique<Sidebar>("Clips", Sidebar::Side::Right);
        clips->setSidebarWidth(250);
        right.addSidebar(clips.get());
        right.setBounds(0, 0, 250, 600);
        clips->setExpanded(false);
        CHECK(right.getWidth() == 28);
        auto* tab = right.getTab(0);
        CHECK(tab && &tab->sidebar() == clips.get());
        CHECK(tab->getBounds().getX() == 0); // Previously placed at 250 - 28, outside the 28px container.
        CHECK(right.getLocalBounds().contains(tab->getBounds()));
        right.getTab(0)->activate();
        CHECK(clips->isExpanded() && right.getWidth() == 250);

        clips->setExpanded(false);
        auto extra = std::make_unique<Sidebar>("Extra", Sidebar::Side::Right);
        extra->setMinWidth(100);
        extra->setSidebarWidth(120);
        right.addSidebar(extra.get());
        CHECK(right.getTotalWidth() == 148 && right.getWidth() == 148);
        CHECK(extra->getX() == 0 && extra->getWidth() == 120);
        auto* clipsTab = right.getTab(0);
        CHECK(clipsTab->getBounds().getX() == 120); // Rail hugs the outer (right) edge.
        CHECK(right.getLocalBounds().contains(clipsTab->getBounds()));
    }

    { // Narrow-window arbitration, width memory, notifications, and removal.
        SidebarContainer left(Sidebar::Side::Left);
        struct NotifyCounter : SidebarContainerListener {
            int changes = 0;
            void sidebarContainerChanged(SidebarContainer*) override { ++changes; }
        };
        NotifyCounter counter;
        left.setContainerListener(&counter);
        auto browser = std::make_unique<Sidebar>("Browser", Sidebar::Side::Left);
        auto rack = std::make_unique<Sidebar>("Channel Rack", Sidebar::Side::Left);
        browser->setSidebarWidth(220);
        rack->setSidebarWidth(250);
        left.addSidebar(browser.get());
        left.addSidebar(rack.get());
        left.setBounds(0, 0, 470, 600);

        left.constrainTo(300); // Proportional clamp; the center keeps any remainder.
        CHECK(left.getDisplayedWidth() == 299);
        CHECK(browser->getWidth() == 140 && rack->getWidth() == 159);
        CHECK(browser->getSidebarWidth() == 220 && rack->getSidebarWidth() == 250);
        left.constrainTo(1000);
        CHECK(left.getDisplayedWidth() == 470 && browser->getWidth() == 220);

        // Reopen controls stay reachable even in an extremely narrow window.
        browser->setExpanded(false);
        rack->setExpanded(false);
        left.constrainTo(10);
        CHECK(left.getDisplayedWidth() == 28 && left.getWidth() == 28);
        CHECK(left.getTabCount() == 2);
        CHECK(left.getLocalBounds().contains(left.getTab(1)->getBounds()));

        // Mixed collapsed/expanded under constraint keeps the rail intact.
        rack->setExpanded(true);
        left.constrainTo(200);
        CHECK(rack->getWidth() == 172);
        CHECK(left.getDisplayedWidth() == 200);
        CHECK(rack->getX() == 28);
        auto* browserTab = left.getTab(0);
        CHECK(&browserTab->sidebar() == browser.get());
        CHECK(left.getLocalBounds().contains(browserTab->getBounds()));
        left.constrainTo(1000);

        // Remembered widths survive repeated toggle cycles.
        browser->setExpanded(true);
        browser->setSidebarWidth(300);
        for (int i = 0; i < 3; ++i) {
            browser->setExpanded(false);
            CHECK(browser->getSidebarWidth() == 300);
            browser->setExpanded(true);
            CHECK(browser->getSidebarWidth() == 300);
        }
        CHECK(left.getTotalWidth() == 550);

        // Resize notifications drive coherent relayout.
        int before = counter.changes;
        rack->setSidebarWidth(260);
        CHECK(counter.changes == before + 1);
        CHECK(left.getWidth() == 560);

        // Removing a collapsed sidebar drops exactly its tab.
        rack->setExpanded(false);
        CHECK(left.getTabCount() == 1 && &left.getTab(0)->sidebar() == rack.get());
        left.removeSidebar(rack.get());
        CHECK(left.getTabCount() == 0 && left.getTotalWidth() == 300);
        CHECK(browser->isVisible());
    }

    { // Factories wire semantic vector icons; tooltips name the restore action.
        Project project;
        auto clips = std::unique_ptr<Sidebar>(createClipsSidebar(project));
        CHECK(clips->getName() == "Clips" && clips->getIcon() == IconId::clips);
        auto rack = std::unique_ptr<Sidebar>(createChannelRackSidebar(project));
        CHECK(rack->getName() == "Channel Rack" && rack->getIcon() == IconId::channelRack);
        PluginScanner scanner;
        auto browser = std::unique_ptr<Sidebar>(createBrowserSidebar(scanner));
        CHECK(browser->getName() == "Browser" && browser->getIcon() == IconId::browser);
        SidebarTab tab(*clips);
        CHECK(tab.getTooltip() == "Restore Clips");
        CHECK(&tab.sidebar() == clips.get());
    }
}

static void sidebarAnimationTests() {
    using Animation = SidebarContainerTestAccess;
    using Pulse = SidebarTabTestAccess;
    constexpr double fade = Animation::fade, reflow = Animation::reflow, duration = fade + reflow;
    for (auto side : {Sidebar::Side::Left, Sidebar::Side::Right}) {
        Sidebar closing("Closing", side), remaining("Remaining", side);
        SidebarContainer container(side);
        struct Workspace : SidebarContainerListener {
            juce::Component center;
            int layouts = 0;
            void sidebarContainerChanged(SidebarContainer* sidebars) override {
                ++layouts;
                center.setBounds(sidebars->getDisplayedWidth(), 0,
                                 1000 - sidebars->getDisplayedWidth(), 600);
            }
        } workspace;
        closing.setSidebarWidth(220);
        remaining.setSidebarWidth(250);
        container.addSidebar(&closing);
        container.addSidebar(&remaining);
        container.setSize(470, 600);
        container.setContainerListener(&workspace);
        workspace.sidebarContainerChanged(&container);
        container.setAnimationsEnabled(true);
        const auto centerBefore = workspace.center.getBounds();
        const auto remainingBefore = remaining.getBounds();

        closing.setExpanded(false);
        auto start = Animation::started(container, closing);
        CHECK(closing.isVisible() && closing.isCollapsed() && container.getTabCount() == 0);
        CHECK(Animation::running(container));
        workspace.layouts = 0;
        Animation::advance(container, start + fade / 2);
        CHECK(closing.getAlpha() > 0.45f && closing.getAlpha() < 0.55f);
        CHECK(workspace.center.getBounds() == centerBefore && remaining.getBounds() == remainingBefore);
        CHECK(container.getDisplayedWidth() == 470 && closing.getSidebarWidth() == 220);
        Animation::advance(container, start + fade);
        CHECK(workspace.layouts == 0); // Opacity and tab membership do not relayout the workspace.
        CHECK(!closing.isVisible() && container.getDisplayedWidth() == 470);
        CHECK(container.getTabCount() == 1 && !container.getTab(0)->isFlashing());
        CHECK(!container.getTab(0)->isVisible());
        Animation::advance(container, start + fade + reflow / 2);
        CHECK(workspace.layouts == 1);
        CHECK(container.getDisplayedWidth() == 374); // 110px closing + 250px remaining + 14px rail.
        CHECK(workspace.center.getWidth() > centerBefore.getWidth());
        CHECK(workspace.center.getWidth() < 1000 - 278);
        CHECK(remaining.isVisible() && remaining.getAlpha() == 1.0f);
        CHECK(!container.getTab(0)->isVisible());
        Animation::advance(container, start + duration);
        CHECK(!Animation::running(container) && container.getDisplayedWidth() == 278);
        CHECK(!closing.isVisible() && closing.getAlpha() == 1.0f);
        CHECK(workspace.center.getWidth() == 722);
        auto* tab = container.getTab(0);
        CHECK(tab && &tab->sidebar() == &closing && tab->isFlashing() && tab->isVisible());
        CHECK(container.getLocalBounds().contains(tab->getBounds()));

        // One identity-bound pulse survives relayout and stops after 300 ms.
        juce::Image highlighted(juce::Image::ARGB, 28, 28, true);
        { juce::Graphics g(highlighted); tab->paintButton(g, false, false); }
        Pulse::advance(*tab, 150);
        CHECK(std::abs(Pulse::amount(*tab) - 0.5f) < 0.001f);
        container.constrainTo(300);
        CHECK(container.getTab(0) == tab && std::abs(Pulse::amount(*tab) - 0.5f) < 0.001f);
        Pulse::advance(*tab, 300);
        CHECK(!tab->isFlashing());
        juce::Image idle(juce::Image::ARGB, 28, 28, true);
        { juce::Graphics g(idle); tab->paintButton(g, false, false); }
        CHECK(highlighted.getPixelAt(14, 14) != idle.getPixelAt(14, 14));
        Animation::advance(container, start + 500);
        CHECK(!tab->isFlashing());

        // Restore makes room first, then fades in. The tab may destroy itself on activation.
        container.constrainTo(1000);
        tab->activate();
        start = Animation::started(container, closing);
        CHECK(closing.isExpanded() && !closing.isVisible() && container.getTabCount() == 0);
        Animation::advance(container, start + reflow / 2);
        CHECK(container.getDisplayedWidth() == 374 && !closing.isVisible());
        Animation::advance(container, start + reflow);
        CHECK(container.getDisplayedWidth() == 470 && !closing.isVisible());
        workspace.layouts = 0;
        Animation::advance(container, start + reflow + fade / 2);
        CHECK(closing.isVisible() && closing.getAlpha() > 0.45f && closing.getAlpha() < 0.55f);
        Animation::advance(container, start + duration);
        CHECK(workspace.layouts == 0); // Restoring opacity also needs no workspace layout.
        CHECK(closing.getAlpha() == 1.0f && !Animation::running(container));
        CHECK(workspace.center.getBounds() == centerBefore && remaining.getBounds() == remainingBefore);

        // Reverse during both phases without snapping alpha/width or leaving a stale tab.
        for (double elapsed : {fade / 2, fade + reflow / 2}) {
            closing.setExpanded(false);
            Animation::advance(container, Animation::started(container, closing) + elapsed);
            const auto width = container.getDisplayedWidth();
            const auto alpha = closing.getAlpha();
            closing.setExpanded(true);
            CHECK(container.getDisplayedWidth() == width && closing.getAlpha() == alpha);
            Animation::advance(container, Animation::started(container, closing) + reflow + fade / 2);
            const auto reverseWidth = container.getDisplayedWidth();
            const auto reverseAlpha = closing.getAlpha();
            closing.setExpanded(false);
            CHECK(container.getDisplayedWidth() == reverseWidth && closing.getAlpha() == reverseAlpha);
            Animation::advance(container, Animation::started(container, closing) + duration);
            CHECK(container.getTabCount() == 1 && container.getTab(0)->isFlashing());
            closing.setExpanded(true);
            Animation::advance(container, Animation::started(container, closing) + duration);
            CHECK(closing.isVisible() && closing.getAlpha() == 1.0f && container.getTabCount() == 0);
        }

        // High-refresh callbacks are not throttled to 60 Hz, and fade frames do no layout.
        closing.setExpanded(false);
        start = Animation::started(container, closing);
        workspace.layouts = 0;
        for (double elapsed = 8; elapsed <= fade; elapsed += 8)
            Animation::advance(container, start + elapsed);
        CHECK(workspace.layouts == 0);
        int distinctWidths = 0, lastWidth = container.getWidth();
        for (double elapsed = fade + 8; elapsed < duration; elapsed += 8) {
            Animation::advance(container, start + elapsed);
            if (lastWidth != container.getWidth()) ++distinctWidths;
            lastWidth = container.getWidth();
        }
        CHECK(distinctWidths >= 10);
        Animation::advance(container, start + duration);
        closing.setExpanded(true);
        Animation::advance(container, Animation::started(container, closing) + duration);

        // Both transitions can run together; resizing never overwrites preferred widths.
        closing.setExpanded(false);
        remaining.setExpanded(false);
        start = Animation::started(container, remaining);
        Animation::advance(container, start + fade + reflow / 2);
        container.constrainTo(100);
        container.setSize(container.getWidth(), 400);
        CHECK(container.getDisplayedWidth() <= 100);
        CHECK(closing.getSidebarWidth() == 220 && remaining.getSidebarWidth() == 250);
        Animation::advance(container, start + duration);
        CHECK(container.getWidth() == 28 && container.getTabCount() == 2);
        CHECK(container.getTab(0)->isFlashing() && container.getTab(1)->isFlashing());
        Pulse::advance(*container.getTab(0), 300);
        remaining.setExpanded(true);
        Animation::advance(container, Animation::started(container, remaining) + duration);
        CHECK(container.getTabCount() == 1 && !container.getTab(0)->isFlashing());
        container.constrainTo(1000);
        CHECK(remaining.getWidth() == 250);

        // Removal/clear/disable during a transition restore opacity and stop animation work.
        closing.setExpanded(true);
        Animation::advance(container, Animation::started(container, closing) + reflow + fade / 2);
        container.removeSidebar(&closing);
        CHECK(closing.getAlpha() == 1.0f && !Animation::running(container));
        CHECK(container.getWidth() == 250 && container.getTabCount() == 0);
        remaining.setExpanded(false);
        container.setAnimationsEnabled(false);
        CHECK(container.getWidth() == 28 && !remaining.isVisible() && remaining.getAlpha() == 1.0f);
        CHECK(!Animation::running(container));
        container.setAnimationsEnabled(true);
        remaining.setExpanded(true);
        container.clearSidebars();
        CHECK(container.getWidth() == 0 && container.getTabCount() == 0 && !Animation::running(container));
        CHECK(remaining.getAlpha() == 1.0f);
    }
}

static void pluginEditorBindingTests() {
    Probe first, replacement, last;
    {
        Project project;
        auto& channels = project.getChannelList();
        auto button = std::make_unique<PluginButton>(project);
        CHECK(button->getButtonText() == "Plugin" && !button->isEnabled());
        CHECK(button->getTooltip().contains("Select an instrument"));
        auto* a = channels.addChannel("Alpha");
        auto* b = channels.addChannel("Beta");
        const auto id = a->getId();
        project.setActiveChannel(0);
        CHECK(button->getTooltip().contains("Alpha: No plugin"));
        a->setPlugin(std::make_unique<PluginHost>());
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(button->getTooltip().contains("Alpha: No plugin"));
        CHECK(!a->getPlugin()->hasEditor() && !a->getPlugin()->createEditor());
        a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(button->getTooltip().contains("Alpha: Offline instrument"));
        CHECK(button->isEnabled());
        first.editorAvailable = false;
        a->setName("No editor");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(!button->isEnabled() && button->getTooltip().contains("No supported editor"));
        CHECK(!a->getPlugin()->createEditor() && first.editorCalls == 0);
        first.editorAvailable = true;
        a->setName("Alpha");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(button->isEnabled());
        channels.moveChannel(0, 1);
        CHECK(project.getActiveChannelId() == id);
        CHECK(button->getTooltip().contains("Alpha:"));
        project.setActiveChannel(0);
        CHECK(button->getTooltip().contains("Beta: No plugin"));
        // A context target is independent of the selected instrument.
        CHECK(PluginButton::unavailableReason(a).isEmpty());
        CHECK(PluginButton::unavailableReason(b).contains("Beta: No plugin"));
        auto row = std::make_unique<ChannelRow>(a, 1);
        auto menu = row->createContextMenu(); // Construct menu data only, no native popup.
        CHECK(project.getActiveChannelId() == b->getId());
        juce::PopupMenu::MenuItemIterator items(menu);
        CHECK(items.next() && items.getItem().isEnabled && !items.getItem().action); // Targets row a.
        CHECK(items.next() && items.getItem().isSeparator);
        CHECK(items.next() && items.getItem().text == "Select for Live Audition & New Placements");
        CHECK(items.next() && items.getItem().text == "Audio Output");
        CHECK(items.next() && items.getItem().text == "Rename Channel…");
        CHECK(items.next() && items.getItem().text == "Remove Channel…");
        CHECK(!items.next());
        row.reset();
        project.setActiveChannel(1);
        project.getTransportState().setPlaying(true);
        a->prepareToPlay(48000, 64);
        for (int i = 0; i < 3; ++i) {
            CHECK(a->getPlugin()->hasEditor());
        }
        CHECK(button->isEnabled() && first.editorQueries > 0);
        // A failed candidate load must not change the installed host or its binding.
        auto* original = a->getPlugin();
        auto candidate = std::make_unique<PluginHost>();
        CHECK(!candidate->loadPlugin(juce::File(VIBEDAW_TEST_HOME).getChildFile("missing.vst3").getFullPathName()));
        CHECK(a->getPlugin() == original && button->getTooltip().contains("Alpha: Offline instrument"));
        a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(replacement)));
        CHECK(first.destroyed == 1 && first.destroyedQuiescent && !first.destroyedOnAudio);
        a->setName("Renamed");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(button->getTooltip().contains("Renamed: Offline instrument"));
        CHECK(a->getPlugin()->hasEditor());
        channels.removeChannel(channels.indexOfChannel(a));
        juce::PopupMenu::MenuItemIterator staleItems(menu);
        CHECK(staleItems.next() && staleItems.getItem().isEnabled && !staleItems.getItem().action);
        CHECK(staleItems.next() && staleItems.getItem().isSeparator);
        CHECK(staleItems.next() && staleItems.getItem().text == "Select for Live Audition & New Placements");
        CHECK(staleItems.next() && staleItems.getItem().text == "Audio Output");
        CHECK(staleItems.next() && staleItems.getItem().text == "Rename Channel…");
        CHECK(staleItems.next() && staleItems.getItem().text == "Remove Channel…");
        CHECK(!staleItems.next());
        CHECK(replacement.destroyed == 1 && replacement.destroyedQuiescent && !replacement.destroyedOnAudio);
        CHECK(button->getTooltip().contains("Select an instrument"));
        b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(last)));
        project.setActiveChannel(0);
        CHECK(button->getTooltip().contains("Beta: Offline instrument"));
        button.reset();
        b->setName("After button teardown");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        // Project destruction tears down the last host with no retained UI binding.
    }
    CHECK(last.destroyed == 1 && last.destroyedQuiescent && !last.destroyedOnAudio);
    CHECK(first.editorQueries > 0 && replacement.editorQueries > 0 && last.editorQueries > 0);
    CHECK(first.editorCalls == 0 && replacement.editorCalls == 0 && last.editorCalls == 0);
}

static void renderTests() {
    Probe first, second;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState transport;
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    ChannelMixer mixer(channels, tracks, clips, transport);
    mixer.prepareToPlay(48000, 64);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    midi.ensureSize(32768);
    auto render = [&] {
        CHECK(AudioQuiescence::instance().enter());
        rendering = true;
        mixer.processBlock(buffer, midi);
        rendering = false;
        AudioQuiescence::instance().leave();
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    };
    mixer.setActiveChannel(0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    render();
    CHECK(first.notes == 1 && second.notes == 0);
    const auto priorOffs = first.noteOffs;
    mixer.setActiveChannel(1);
    midi.addEvent(juce::MidiMessage::noteOn(1, 62, 0.5f), 0);
    render();
    CHECK(first.noteOffs == priorOffs + 1 && first.sounding == 0 && second.notes == 1);
    CHECK(first.blocks == 2 && second.blocks == 2);
    b->setMuted(true);
    midi.addEvent(juce::MidiMessage::noteOff(1, 62), 0);
    render(); CHECK(second.blocks == 3);
    CHECK(second.sounding == 0);
    b->setMuted(false); // T04 drops attacks while suppressed; resume on a fresh attack.
    channels.moveChannel(1, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.5f), 0);
    render(); CHECK(second.notes == 2 && first.notes == 1);
    const auto stopOffs = second.noteOffs;
    transport.stop(); transport.setPlaying(true);
    render(); CHECK(second.noteOffs == stopOffs + 1 && second.sounding == 0);
    for (int i = 0; i < 2049; ++i) midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    render(); CHECK(second.notes == 2 && mixer.getOverflowCount() == 1);
    channels.removeChannel(0);
    CHECK(second.destroyed == 1 && !second.destroyedOnAudio);
    render();
    a->setPlugin(nullptr);
    CHECK(first.destroyed == 1 && !first.destroyedOnAudio);
    channels.clearChannels(); render();
    mixer.releaseResources();

    AudioEngine engine; // Never initialise: no audio device or application launch.
    AudioEngineTestAccess::prepare(engine, 48000, 64);
    engine.setProcessor(&mixer);
    auto deviceRender = [&] {
        rendering = true;
        AudioEngineTestAccess::render(engine, buffer);
        rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    };
    deviceRender();
    for (int i = 0; i < 2049; ++i)
        engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 60, 0.5f));
    CHECK(engine.getMidiOverflowCount() == 1);
    deviceRender();
    { AudioQuiescence::Edit edit; deviceRender(); }
    CHECK(engine.getSkippedBlockCount() == 1);
    deviceRender();
    AudioEngineTestAccess::prepare(engine, 96000, 32);
    deviceRender(); CHECK(engine.getSkippedBlockCount() == 2);
    CHECK(buffer.getMagnitude(0, 64) == 0);
    engine.clearProcessor();

    auto* destination = channels.addChannel();
    auto dense = std::make_unique<MidiClip>(0, 4);
    for (size_t i = 0; i < ArrangementSnapshot::maxNotes; ++i) dense->addNote(Note(60, 0, 1));
    auto id = clips.addClip(std::move(dense));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, destination->getId(), 0, 4));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, destination->getId(), 4, 4));
    const auto saturated = compileArrangement(tracks, clips, channels, 9);
    CHECK(saturated.overflow && saturated.notes.empty() && saturated.revision == 9);
}

static void reviewRegressionTests() {
    Probe probe;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState transport;
    auto* channel = channels.addChannel();
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    CHECK(channel->getPlugin()->hasEditor());
    ChannelMixer mixer(channels, tracks, clips, transport);
    mixer.setActiveChannel(0);
    AudioEngine engine;
    AudioEngineTestAccess::prepare(engine, 48000, 64);
    engine.setProcessor(&mixer);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    midi.ensureSize(32768);
    auto render = [&] {
        CHECK(AudioQuiescence::instance().enter());
        rendering = true;
        mixer.processBlock(buffer, midi);
        rendering = false;
        AudioQuiescence::instance().leave();
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(probe.lastInputCount <= 2048);
    };
    auto deviceRender = [&] {
        rendering = true;
        AudioEngineTestAccess::render(engine, buffer);
        rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    };
    deviceRender(); // Consume preparation cleanup before delivering notes.
    auto holdMaximum = [&] {
        for (unsigned i = 0; i < Channel::maxDeliveredNotes; ++i) {
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
            if ((i + 1) % Channel::maxLiveEvents == 0) render();
        }
        if (!midi.isEmpty()) render();
        CHECK(probe.sounding == static_cast<int>(Channel::maxDeliveredNotes));
    };
    holdMaximum();
    transport.stop();
    for (unsigned i = 0; i < Channel::maxLiveEvents - 1; ++i)
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 63);
    render();
    CHECK(probe.lastInputCount == 2048 && probe.lastOffSample == 63);
    CHECK(probe.noteOffs == 1025 && probe.sounding == 0);

    // A full 2048-event live batch cannot consume the cleanup reserve. Reject the
    // entire batch, not a prefix ending before its late note-off.
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    auto notesBefore = probe.notes;
    auto overflowBefore = mixer.getOverflowCount();
    for (int i = 0; i < 2047; ++i) midi.addEvent(juce::MidiMessage::noteOn(1, 61, 0.5f), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 63);
    render();
    CHECK(probe.notes == notesBefore && probe.sounding == 0);
    CHECK(mixer.getOverflowCount() == overflowBefore + 1);
    holdMaximum();
    notesBefore = probe.notes;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    CHECK(probe.notes == notesBefore && probe.sounding == 0); // Outstanding-note cap, including repeats.

    // Unmapped cleanup CCs cannot release a sustained voice. Silence immediately,
    // then reset via the message-thread polling seam under actual quiescence.
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0); render();
    CHECK(probe.sounding == 1);
    transport.stop(); render();
    CHECK(channel->isVoiceResetPending() && probe.sounding == 1);
    CHECK(buffer.getMagnitude(0, 64) == 0);
    const auto blocksBefore = probe.blocks;
    render(); CHECK(probe.blocks == blocksBefore && buffer.getMagnitude(0, 64) == 0);
    const auto skipsBeforeReset = engine.getSkippedBlockCount();
    probe.duringReset = [&] { deviceRender(); };
    ChannelTestAccess::resetVoices(*channel);
    probe.duringReset = {};
    CHECK(!channel->isVoiceResetPending() && probe.sounding == 0);
    CHECK(probe.resets == 1 && !probe.resetOnAudio && probe.resetQuiescent);
    CHECK(engine.getSkippedBlockCount() == skipsBeforeReset + 1);
    const auto blocksAfterReset = probe.blocks;
    for (int i = 0; i < 3; ++i) {
        deviceRender(); // Sticky panic from the skipped callback, with no fresh MIDI.
        CHECK(!channel->isVoiceResetPending());
        ChannelTestAccess::resetVoices(*channel);
    }
    CHECK(probe.resets == 1 && probe.blocks == blocksAfterReset + 3);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    transport.stop(); render();
    CHECK(channel->isVoiceResetPending()); // Pedal mapping can persist across reset.
    const auto overflowDuringReset = engine.getMidiOverflowCount();
    const auto notesBeforeReset = probe.notes;
    probe.duringReset = [&] {
        deviceRender();
        const unsigned char payload[]{1, 2};
        engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::createSysExMessage(payload, 2));
        engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 62, 0.5f));
    };
    ChannelTestAccess::resetVoices(*channel);
    probe.duringReset = {};
    CHECK(probe.resets == 2 && probe.sounding == 0);
    CHECK(engine.getMidiOverflowCount() == overflowDuringReset + 1);
    deviceRender();
    CHECK(probe.notes == notesBeforeReset && !channel->isVoiceResetPending());
    ChannelTestAccess::resetVoices(*channel);
    CHECK(probe.resets == 2); // Independent ingress panic was not cleared to break the loop.

    // A reset pending at device release must not reactivate an unprepared plugin.
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    transport.stop(); render();
    CHECK(channel->isVoiceResetPending());
    mixer.releaseResources();
    CHECK(!channel->isPrepared() && !probe.prepared);
    ChannelTestAccess::resetVoices(*channel);
    ChannelTestAccess::resetVoices(*channel);
    CHECK(probe.resets == 2 && probe.resetsWhileUnprepared == 0);
    CHECK(channel->isVoiceResetPending() && !probe.prepared);
    probe.duringReset = [&] { deviceRender(); };
    mixer.prepareToPlay(48000, 64);
    probe.duringReset = {};
    CHECK(probe.resets == 3 && probe.resetsWhileUnprepared == 0);
    CHECK(channel->isPrepared() && probe.prepared && !channel->isVoiceResetPending());
    CHECK(probe.sounding == 0 && probe.resetQuiescent && !probe.resetOnAudio);
    deviceRender(); ChannelTestAccess::resetVoices(*channel);
    CHECK(probe.resets == 3 && !channel->isVoiceResetPending());

    // New instance clears the conservative pedal latch.
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    probe.sustain = false;
    juce::MidiKeyboardState keyboard;
    keyboard.addListener(&engine);
    MidiManager manager;
    manager.setAudioDestination(engine, keyboard);
    keyboard.noteOn(1, 65, 0.5f); deviceRender();
    CHECK(probe.sounding == 1 && keyboard.isNoteOn(1, 65));
    const auto ingressOverflow = engine.getMidiOverflowCount();
    for (int i = 0; i < 2049; ++i) {
        manager.sendMidiMessage(juce::MidiMessage::controllerEvent(1, 7, 100));
        if (i % 500 == 499) deviceRender();
    }
    deviceRender(); // Audio ingress never saturates, only the feedback queue does.
    CHECK(engine.getMidiOverflowCount() == ingressOverflow && probe.sounding == 1);
    MidiManagerTestAccess::drain(manager);
    CHECK(!keyboard.isNoteOn(1, 65));
    const auto offsBeforeFeedbackPanic = probe.noteOffs;
    deviceRender();
    CHECK(probe.sounding == 0 && probe.noteOffs == offsBeforeFeedbackPanic + 1);

    keyboard.noteOn(1, 66, 0.5f); deviceRender();
    const auto skipsBefore = engine.getSkippedBlockCount();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i) buffer.setSample(ch, i, 0.375f);
    rendering = true;
    AudioEngineTestAccess::renderSamples(engine, buffer, -1);
    AudioEngineTestAccess::renderSamples(engine, buffer, 0);
    rendering = false;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i) CHECK(buffer.getSample(ch, i) == 0.375f);
    CHECK(engine.getSkippedBlockCount() == skipsBefore + 2);
    deviceRender(); CHECK(probe.sounding == 0); // Nonpositive callbacks still request cleanup.
    keyboard.removeListener(&engine);
    engine.clearProcessor();
}

static void modelTests() {
    Project project;
    auto& channels = project.getChannelList();
    auto& pool = project.getClipPool();
    auto& tracks = project.getTrackList();
    auto& transport = project.getTransportState();
    PoolChanges deletion(pool);
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    const auto aId = a->getId(), bId = b->getId();
    project.setActiveChannel(0);
    auto source = std::make_unique<MidiClip>(5, 4);
    auto* midi = source.get();
    midi->setLoopEnabled(true);
    ClipChanges clipChanges;
    midi->addListener(&clipChanges);
    midi->addNote(Note(62, 3.5, 1.5));
    midi->addNote(Note(60, 0, 1));
    midi->addNote(Note(64, 4, 1));
    CHECK(clipChanges.invalidations == 3);
    auto* borrowed = midi->findNoteAt(0, 60);
    midi->updateNote(borrowed, Note(61, 0, 1));
    CHECK(midi->findNoteAt(0, 61) == borrowed);
    CHECK(clipChanges.invalidations == 3 && clipChanges.changes == 4);
    const auto sourceId = pool.addClip(std::move(source));
    auto* x = tracks.addTrack("X");
    auto* y = tracks.addTrack("Y");
    Changes trackChanges;
    x->addChangeListener(&trackChanges);
    x->addClipInstance(std::make_unique<ClipInstance>(sourceId, aId, 8, 6));
    x->addClipInstance(std::make_unique<ClipInstance>(sourceId, bId, 0, 2));
    y->addClipInstance(std::make_unique<ClipInstance>(sourceId, aId, 0, 4));
    x->sendSynchronousChangeMessage();
    CHECK(trackChanges.count > 0);
    channels.moveChannel(0, 1);
    CHECK(channels.getChannel(0) == b && channels.getChannelById(aId) == a);
    CHECK(project.getActiveChannelId() == aId && project.getActiveChannel() == 1);
    CHECK(x->getClipInstance(0)->getChannelId() == aId);
    CHECK(x->getClipInstance(1)->getChannelId() == bId);
    CHECK(y->getClipInstance(0)->getChannelId() == aId);
    auto snapshot = compileArrangement(tracks, pool, channels, 1);
    CHECK(snapshot.notes.size() == 5);
    CHECK(snapshot.notes.back().start == 11.5 && snapshot.notes.back().end == 12);
    CHECK(snapshot.notes.front().pitch == 61);
    ArrangementPublisher publisher(tracks, pool, channels);
    const auto& old = publisher.acquire();
    const auto revision = old.revision;
    midi->clearNotes();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    CHECK(old.revision == revision && old.notes.size() == 5);
    CHECK(publisher.acquire().notes.empty());
    midi->addNote(Note(60, 0, 1));
    const auto previousRevision = publisher.acquire().revision;
    x->getClipInstance(0)->setMuted(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    CHECK(publisher.acquire().revision != previousRevision);
    x->getClipInstance(0)->setMuted(false);
    x->removeClipInstance(1);
    CHECK(pool.getClip(sourceId) == midi && y->getNumClipInstances() == 1);

    for (double tempo : {120.0, 60.0, 90.0}) {
        transport.setTempo(tempo); transport.setPositionInBeats(4);
        CHECK(std::abs(transport.getPosition() - 240.0 / tempo) < 1e-10);
        CHECK(std::abs(transport.getPositionInBeats() - 4) < 1e-10);
        CHECK(midi->getDuration() == 4 && x->getClipInstance(0)->getStartTime() == 8);
    }
    transport.stop();
    transport.setPlaying(true);
    for (int i = 0; i < 10000; ++i) transport.setPosition(i);
    CHECK(transport.acquireControl().stopGeneration != 0);
    CHECK(transport.acquireControl().playing);
    transport.setLoopRegion(2, 6); transport.setLoopEnabled(true);
    CHECK(transport.acquireControl().loop.endBeats == 6);
    transport.publishRenderPosition({2.5, 120, 48000, {}, false, 7, 0});
    CHECK(transport.acquireRenderPosition().beats == 2.5);
    CHECK(transport.acquireRenderPosition().revision == 7);
    ClipInstance interval(sourceId, aId, 8, 4);
    CHECK(interval.containsTime(8) && !interval.containsTime(12) && !interval.overlapsRange(12, 16));
    CHECK(!Note(60, 0, 1).containsTime(1));
    for (double bad : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        interval.setStartTime(bad); CHECK(interval.getStartTime() == 8);
        interval.setDuration(bad); CHECK(interval.getDuration() == 4);
        CHECK(!midi->addNote(Note(60, bad, 1)));
        x->addClipInstance(std::make_unique<ClipInstance>(sourceId, aId, bad, 1));
    }
    x->addClipInstance(nullptr);
    x->addClipInstance(std::make_unique<ClipInstance>(-1, aId, 0, 1));
    CHECK(x->getNumClipInstances() == 1);
    CHECK(pool.addClip(nullptr) == InvalidClipId);
    AudioClip audio(0, 2); CHECK(audio.getDuration() == 2);

    ChannelChanges changes;
    channels.addListener(&changes);
    a->setName("renamed"); a->setMuted(true); a->setPlugin(nullptr);
    a->setSampleFile({}); a->setColour(juce::Colours::red); a->setVolume(0.5f);
    a->setPan(0.25f); a->setMixerTrackId(8);
    a->sendSynchronousChangeMessage();
    CHECK(changes.changed > 0 && changes.last == aId);
    const auto count = changes.changed;
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer events;
    a->prepareToPlay(48000, 64); buffer.clear(); a->processBlock(buffer, events); a->releaseResources();
    CHECK(changes.changed == count);
    a->setMuted(false); channels.moveChannel(1, 0); a->sendSynchronousChangeMessage();
    CHECK(changes.last == aId);
    a->setName("pending deletion");
    channels.removeChannel(1);
    CHECK(project.getActiveChannelId() == aId && project.getActiveChannel() == 0);
    channels.removeChannel(0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    CHECK(project.getActiveChannelId() == -1 && project.getActiveChannel() == -1);
    CHECK(compileArrangement(tracks, pool, channels, 8).notes.empty());
    project.setActiveChannel(999); CHECK(project.getActiveChannel() == -1);
    auto* c = channels.addChannel(); CHECK(c->getId() != aId && c->getId() != bId);
    CHECK(!channels.getChannelById(aId) && !channels.getChannelById(-1));
    channels.clearChannels(); channels.addChannel();
    CHECK(!channels.getChannelById(aId));
    channels.removeListener(&changes);
    midi->removeListener(&clipChanges);
    pool.removeClip(sourceId);
    CHECK(x->getClipInstance(0)->getClipId() == sourceId);
    auto next = pool.addClip(std::make_unique<MidiClip>()); CHECK(next != sourceId);
    pool.clearClips();
    CHECK(pool.addClip(std::make_unique<MidiClip>()) != next);
    pool.removeClip(sourceId);
    CHECK(deletion.before == deletion.after && deletion.before == 2);
    x->removeChangeListener(&trackChanges);

    ChannelMixer mixer(channels, tracks, pool, transport);
    mixer.prepareToPlay(48000, 64);
    for (int i = 0; i < 2100; ++i) events.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    mixer.processBlock(buffer, events);
    CHECK(mixer.getOverflowCount() == 1 && events.isEmpty());
    juce::AudioBuffer<float> oversized(2, 65);
    mixer.processBlock(oversized, events); CHECK(mixer.getOverflowCount() == 2);
    channels.clearChannels(); mixer.processBlock(buffer, events);
    CHECK(buffer.getMagnitude(0, 64) == 0);
    mixer.releaseResources(); mixer.prepareToPlay(96000, 32); mixer.releaseResources();
}

static void transportClockTests() {
    constexpr double tolerance = 1e-10; // Quarter-note beats for all sample-count comparisons.
    for (int size : {1, 7, 64, 127, 512, 4096, 96000}) {
        TransportState state;
        TransportClock clock;
        state.setPlaying(true);
        double end = 0;
        for (int rendered = 0; rendered < 96000;) {
            const int count = std::min(size, 96000 - rendered);
            rendering = true;
            const auto block = clock.beginBlock(state, count, 48000);
            clock.endBlock(state, block);
            rendering = false;
            CHECK(block.startBeats == end && block.numSamples == count);
            end = block.endBeats;
            rendered += count;
        }
        CHECK(std::abs(end - 4.0) < tolerance);
        CHECK(state.getPositionInBeats() == 0); // No polling, no UI clock, no listener side effects.
        state.pollRenderPosition();
        CHECK(std::abs(state.getPositionInBeats() - 4.0) < tolerance);
    }

    {
        TransportState longRun;
        TransportClock clock;
        longRun.setTempo(137);
        longRun.setPlaying(true);
        const int total = 44100 * 3600;
        for (int samples = 0; samples < total;) {
            int count = std::min(127, total - samples);
            auto block = clock.beginBlock(longRun, count, 44100);
            clock.endBlock(longRun, block);
            samples += count;
        }
        longRun.pollRenderPosition();
        CHECK(std::abs(longRun.getPositionInBeats() - 137.0 * 60) < tolerance);
    }

    struct Listener : TransportListener {
        int positions = 0;
        void transportPositionChanged(double) override {
            CHECK(!rendering && juce::MessageManager::getInstance()->isThisTheMessageThread());
            ++positions;
        }
    } listener;
    TransportState state;
    TransportClock clock;
    state.addListener(&listener);
    auto render = [&](int count = 480, double rate = 48000) {
        rendering = true;
        auto block = clock.beginBlock(state, count, rate);
        clock.endBlock(state, block);
        rendering = false;
        return block;
    };
    CHECK(render().endBeats == 0);
    state.setPositionInBeats(8);
    auto block = render();
    CHECK(block.startBeats == 8 && block.endBeats == 8 && block.discontinuity);
    auto revision = block.revision;
    state.setPlaying(true);
    block = render();
    CHECK(block.startBeats == 8 && std::abs(block.endBeats - 8.02) < tolerance);
    CHECK(!block.discontinuity && block.revision == revision);
    // A seek made after rendering but before polling cannot be overwritten by old feedback.
    state.setPositionInBeats(32);
    state.pollRenderPosition();
    CHECK(state.getPositionInBeats() == 32);
    state.setTempo(60);
    state.setTimeSignature(6, 8);
    block = render();
    CHECK(block.startBeats == 32 && block.tempo == 60 && block.meter.denominator == 8);
    CHECK(std::abs(block.endBeats - 32.01) < tolerance && block.discontinuity);
    state.pollRenderPosition();
    const auto beforeTempo = state.getPositionInBeats();
    state.setTempo(180);
    CHECK(state.getPositionInBeats() == beforeTempo);
    CHECK(std::abs(state.getPosition() - beforeTempo / 3) < tolerance);
    block = render();
    CHECK(block.startBeats == beforeTempo && std::abs(block.endBeats - beforeTempo - 0.03) < tolerance);
    const auto beforeRate = block.endBeats;
    block = render(960, 96000);
    CHECK(block.startBeats == beforeRate && std::abs(block.endBeats - beforeRate - 0.03) < tolerance);
    CHECK(block.discontinuity);
    state.stop();
    block = render();
    CHECK(!block.playing && block.discontinuity && block.startBeats == block.endBeats);
    state.pollRenderPosition();
    const auto stopped = state.getPositionInBeats();
    CHECK(render().endBeats == stopped);
    // Coalescing cannot lose cleanup, and publishing unrelated fields cannot replay a seek.
    state.stop(); state.setPlaying(true);
    for (int i = 0; i < 10000; ++i) state.setTimeSignature(i % 7 + 1, 8);
    block = render();
    CHECK(block.playing && block.discontinuity && block.startBeats == stopped);
    state.setMetronomeEnabled(true); state.setLoopRegion(0, 1); state.setLoopEnabled(true);
    block = render();
    CHECK(block.startBeats == 0 && block.endBeats < 1 && block.discontinuity);
    state.reset();
    block = render();
    CHECK(block.startBeats == 0 && block.playing && block.discontinuity);
    state.setPositionInBeats(0); state.setPositionInBeats(0); // Same-position seeks still acknowledge.
    CHECK(render().discontinuity);
    state.setRecording(true); CHECK(state.isRecording());
    state.setRecording(false); CHECK(!state.isRecording());
    for (double bad : {-1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN(), 1.0e100}) {
        state.setPositionInBeats(bad); CHECK(state.getPositionInBeats() == 0);
        state.setPosition(bad); CHECK(state.getPositionInBeats() == 0);
    }
    state.setTempo(std::numeric_limits<double>::quiet_NaN()); CHECK(state.getTempo() == 180);
    state.setTempo(0); CHECK(state.getTempo() == 20);
    state.setTempo(1000); CHECK(state.getTempo() == 300);
    state.setTimeSignature(0, 3);
    CHECK(state.getTimeSignature().numerator == 1 && state.getTimeSignature().denominator == 4);
    CHECK(TransportState::formatBarsBeatsTicks(4, {4, 4}) == "2:1:000");
    CHECK(TransportState::formatBarsBeatsTicks(3, {6, 8}) == "2:1:000");
    CHECK(TransportState::formatBarsBeatsTicks(0.75, {6, 8}) == "1:2:480");
    CHECK(TransportState::formatBarsBeatsTicks(3.5, {7, 8}) == "2:1:000");
    CHECK(TransportState::formatBarsBeatsTicks(1, {2, 2}) == "1:1:480");
    CHECK(TransportState::formatBarsBeatsTicks(0.25, {4, 16}) == "1:2:000");
    CHECK(listener.positions > 0);
    state.removeListener(&listener);

    // Exercise both SPSC directions concurrently; only main publishes UI commands/polls.
    TransportState concurrent;
    TransportClock concurrentClock;
    concurrent.setPlaying(true);
    std::atomic<bool> finished{false};
    std::thread audio([&] {
        for (int i = 0; i < 100000; ++i) {
            auto timing = concurrentClock.beginBlock(concurrent, 7, 44100);
            concurrentClock.endBlock(concurrent, timing);
        }
        finished.store(true);
    });
    while (!finished.load()) {
        concurrent.setTimeSignature(7, 8);
        concurrent.pollRenderPosition();
        CHECK(concurrent.getPositionInBeats() >= 0);
    }
    audio.join();
    concurrent.pollRenderPosition();
    CHECK(std::abs(concurrent.getPositionInBeats() - 700000.0 * 2 / 44100) < tolerance);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void transportEngineTests() {
    Probe probe;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* channel = channels.addChannel();
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.setActiveChannel(0);
    AudioEngine engine;
    engine.setProcessor(&mixer);
    AudioEngineTestAccess::prepare(engine, 48000, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    auto render = [&](int samples) {
        rendering = true;
        AudioEngineTestAccess::renderSamples(engine, buffer, samples);
        rendering = false;
        state.pollRenderPosition();
    };
    state.setPlaying(true);
    render(512);
    double end = state.getPositionInBeats();
    CHECK(std::abs(end - 512.0 / 24000) < 1e-10);
    auto revision = state.acquireRenderPosition().revision;
    { AudioQuiescence::Edit edit; render(512); }
    CHECK(state.getPositionInBeats() == end);
    render(0); render(-1);
    CHECK(state.getPositionInBeats() == end);
    render(128);
    CHECK(state.acquireRenderPosition().revision != revision);
    CHECK(std::abs(state.getPositionInBeats() - end - 128.0 / 24000) < 1e-10);
    AudioEngineTestAccess::prepare(engine, 96000, 256);
    end = state.getPositionInBeats();
    render(512); // Oversized: silence, freeze, cleanup at next admitted block.
    CHECK(state.getPositionInBeats() == end);
    render(256);
    CHECK(std::abs(state.getPositionInBeats() - end - 256.0 / 48000) < 1e-10);
    AudioEngineTestAccess::prepare(engine, 0, 256);
    end = state.getPositionInBeats(); render(128);
    CHECK(state.getPositionInBeats() == end);
    AudioEngineTestAccess::prepare(engine, 96000, 256);
    state.stop(); render(256); CHECK(state.getPositionInBeats() == end);
    state.setPositionInBeats(16); render(256); CHECK(state.getPositionInBeats() == 16);
    state.setPlaying(true); render(128);
    CHECK(std::abs(state.getPositionInBeats() - 16 - 128.0 / 48000) < 1e-10);
    engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 60, 0.5f));
    render(128); CHECK(probe.sounding == 1);
    const auto offs = probe.noteOffs;
    state.setPositionInBeats(24);
    render(128);
    CHECK(probe.sounding == 0 && probe.noteOffs == offs + 1 && probe.lastOffSample == 0);
    state.stop(); render(128);
    engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 61, 0.5f));
    render(128); CHECK(probe.sounding == 1); // Live audition remains available stopped.
    state.setPositionInBeats(24); render(128); CHECK(probe.sounding == 0);
    end = state.getPositionInBeats();
    mixer.releaseResources(); render(128); CHECK(state.getPositionInBeats() == end);
    mixer.prepareToPlay(96000, 256);
    state.setPlaying(true); render(128);
    CHECK(std::abs(state.getPositionInBeats() - end - 128.0 / 48000) < 1e-10);
    engine.clearProcessor();
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void externalMidiTests() {
    // T09: the device-selection path end to end. connectToDevice cannot open real
    // hardware here (ALSA/JACK are compiled out of this target and their JUCE
    // stubs return an empty device list), so sendMidiMessage stands in for the
    // connected device's callback: MidiManager routes it through exactly the same
    // handleIncomingMidiMessage path an open MidiInput would invoke.
    {
        MidiManager manager; // Never given an audio destination.
        CHECK(manager.getAvailableDevices().isEmpty());
        CHECK(!manager.isConnected() && manager.getCurrentDeviceName().isEmpty());
        CHECK(!manager.connectToDevice(-1));
        CHECK(!manager.connectToDevice(0)); // Empty device list.
        CHECK(!manager.connectToDevice("Not a device"));
        CHECK(!manager.isConnected());
        manager.disconnect(); // No device connected: must stay a safe no-op.
        manager.sendMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.5f)); // No destination: dropped.
        MidiManagerTestAccess::drain(manager); // No keyboard/listeners: harmless.
    }

    // Device event -> engine queue -> active channel, alongside arrangement
    // playback routed to a different destination, with keyboard feedback that
    // must never re-enter the audio path.
    Probe probe, other;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* active = channels.addChannel("Active");
    auto* background = channels.addChannel("Background");
    active->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    background->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(other)));
    auto source = std::make_unique<MidiClip>(4, 4);
    source->addNote(Note(40, 0, 2)); // Plays on the background destination only.
    const auto id = clips.addClip(std::move(source));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, background->getId(), 0, 4));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.setActiveChannel(0);
    mixer.prepareToPlay(48000, 256);
    AudioEngine engine;
    engine.setProcessor(&mixer);
    AudioEngineTestAccess::prepare(engine, 48000, 256);
    juce::MidiKeyboardState keyboard;
    keyboard.addListener(&engine);
    MidiManager manager;
    manager.setAudioDestination(engine, keyboard);
    struct Recorder : MidiListener {
        std::vector<juce::MidiMessage> received;
        void handleMidiMessage(const juce::MidiMessage& message, int) override {
            received.push_back(message);
        }
    } recorder;
    manager.addListener(&recorder);

    juce::AudioBuffer<float> buffer(2, 256);
    auto render = [&](int samples) {
        rendering = true;
        AudioEngineTestAccess::renderSamples(engine, buffer, samples);
        rendering = false;
        state.pollRenderPosition();
    };

    state.setPlaying(true);
    render(256);
    CHECK(other.sounding == 1 && probe.sounding == 0); // Arrangement reaches only its destination.

    const auto notesBefore = probe.notes;
    manager.sendMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.6f));
    render(256);
    CHECK(probe.sounding == 1 && probe.notes == notesBefore + 1);
    CHECK(other.sounding == 1); // Arrangement note keeps sounding independently.

    // The feedback timer marks the on-screen keyboard and notifies listeners,
    // but must never re-enqueue the note as fresh live input.
    MidiManagerTestAccess::drain(manager);
    CHECK(recorder.received.size() == 1);
    CHECK(keyboard.isNoteOn(1, 60));
    CHECK(probe.notes == notesBefore + 1); // Exactly one attack reached the plugin.
    render(256);
    CHECK(probe.sounding == 1 && probe.notes == notesBefore + 1);

    manager.sendMidiMessage(juce::MidiMessage::noteOff(1, 60));
    render(256);
    CHECK(probe.sounding == 0);
    MidiManagerTestAccess::drain(manager);
    CHECK(recorder.received.size() == 2);
    CHECK(!keyboard.isNoteOn(1, 60));

    // Live input follows the active channel: switching destinations reroutes the
    // next external event without disturbing the still-playing arrangement.
    mixer.setActiveChannel(1);
    manager.sendMidiMessage(juce::MidiMessage::noteOn(1, 62, 0.6f));
    render(256);
    CHECK(other.sounding == 2 && probe.sounding == 0); // Arrangement note + new live note.

    // Release the live note; the arrangement tail then ends on its own.
    manager.sendMidiMessage(juce::MidiMessage::noteOff(1, 62));
    render(256);
    CHECK(other.sounding == 1 && probe.sounding == 0);

    // Everything winds down: arrangement tail ends, no stuck notes anywhere.
    int guard = 0;
    while ((probe.sounding != 0 || other.sounding != 0) && guard++ < 1000) render(256);
    CHECK(guard < 1000 && probe.sounding == 0 && other.sounding == 0);
    const auto allClear = [](const Probe& p) {
        return std::all_of(p.held.begin(), p.held.end(), [](unsigned n) { return n == 0; });
    };
    CHECK(allClear(probe) && allClear(other));

    manager.removeListener(&recorder);
    keyboard.removeListener(&engine);
    state.setPlaying(false);
    render(256);
    engine.clearProcessor();
}

static void arrangementPlaybackTests() {
    // Exact absolute event times must be invariant under block partitioning. Source
    // origin/loop flag are deliberately irrelevant; placement tails are silent.
    for (double rate : {48000.0, 44100.0}) for (double tempo : {120.0, 137.0})
    for (int size : {7, 127, 512, 4096}) {
        Probe first, second;
        ChannelList channels;
        TrackList tracks;
        ClipPool clips;
        TransportState state;
        auto* a = channels.addChannel("A");
        auto* b = channels.addChannel("B");
        a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
        b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
        auto source = std::make_unique<MidiClip>(9, 4);
        source->setLoopEnabled(true);
        source->addNote(Note(62, 3.5, 1)); // Truncated at source end.
        source->addNote(Note(60, 0, 1));
        source->addNote(Note(60, 1, 0.5)); // Touching, not overlapping.
        source->addNote(Note(60, 0.5, 0.75)); // Union [0, 1.5), so next attack joins too.
        source->addNote(Note(64, 2, 0.5));
        source->addNote(Note(65, 2, 0.5));
        source->addNote(Note(66, 4, 1));
        source->addNote(Note(67, 0, 1, 0));
        Note muted(68, 0, 1); muted.setMuted(true); source->addNote(muted);
        Note ch2(60, 2, 0.25); ch2.setChannel(2); source->addNote(ch2);
        const auto id = clips.addClip(std::move(source));
        auto* x = tracks.addTrack();
        x->addClipInstance(std::make_unique<ClipInstance>(id, a->getId(), 0, 6));
        x->addClipInstance(std::make_unique<ClipInstance>(id, b->getId(), 0, 2.25));
        tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, a->getId(), 6, 4));
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.setActiveChannel(1);
        mixer.prepareToPlay(rate, size);
        state.setTempo(tempo); state.setPlaying(true);
        juce::AudioBuffer<float> buffer(2, size);
        juce::MidiBuffer midi; midi.ensureSize(32768);
        const double spb = rate * 60 / tempo;
        const int total = static_cast<int>(std::ceil(10 * spb)) + 1;
        int blocks = 0;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
            juce::AudioBuffer<float> block(pointers, 2, n);
            rendering = true; mixer.processBlock(block, midi); rendering = false;
            CHECK(renderAllocations == 0 && renderDeletions == 0);
            CHECK(first.lastInputCount <= 2048 && second.lastInputCount <= 2048);
            time += n; ++blocks;
        }
        CHECK(first.blocks == blocks && second.blocks == blocks);
        CHECK(first.sounding == 0 && second.sounding == 0);
        const auto verify = [&](const Probe& p, std::vector<std::tuple<double, bool, int, int>> expected) {
            std::sort(expected.begin(), expected.end());
            CHECK(p.events.size() == expected.size());
            for (size_t i = 0; i < expected.size(); ++i) {
                const auto [beat, on, channel, pitch] = expected[i];
                const auto sample = static_cast<long long>(std::floor(beat * spb + 1e-7));
                CHECK(p.events[i].time == sample && p.events[i].offset == sample % size);
                CHECK(p.events[i].on == on && p.events[i].channel == channel && p.events[i].pitch == pitch);
            }
        };
        std::vector<std::tuple<double, bool, int, int>> expected;
        for (double start : {0.0, 6.0}) {
            expected.insert(expected.end(), {{start, true, 1, 60}, {start + 1.5, false, 1, 60},
                {start + 2, true, 1, 64}, {start + 2.5, false, 1, 64},
                {start + 2, true, 1, 65}, {start + 2.5, false, 1, 65},
                {start + 2, true, 2, 60}, {start + 2.25, false, 2, 60},
                {start + 3.5, true, 1, 62}, {start + 4, false, 1, 62}});
        }
        verify(first, expected);
        verify(second, {{0, true, 1, 60}, {1.5, false, 1, 60}, {2, true, 1, 64},
            {2.25, false, 1, 64}, {2, true, 1, 65}, {2.25, false, 1, 65},
            {2, true, 2, 60}, {2.25, false, 2, 60}});
        CHECK(mixer.getOverflowCount() == 0);
    }
}

static void arrangementLifecycleTests() {
    Probe first, second;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    auto source = std::make_unique<MidiClip>(0, 4);
    auto* notes = source.get();
    notes->addNote(Note(60, 0, 1));
    notes->addNote(Note(60, 1, 1));
    notes->addNote(Note(62, 2, 1));
    const auto id = clips.addClip(std::move(source));
    auto* track = tracks.addTrack();
    track->addClipInstance(std::make_unique<ClipInstance>(id, a->getId(), 0, 4));
    auto* placement = track->getClipInstance(0);
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.prepareToPlay(48000, 24000);
    mixer.setActiveChannel(0);
    juce::AudioBuffer<float> buffer(2, 24000);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto render = [&](int n = 24000) {
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(first.lastInputCount <= 2048 && second.lastInputCount <= 2048);
    };
    auto dispatch = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(20); };
    auto restart = [&] { state.setPositionInBeats(0); state.setPlaying(true); };
    restart(); render();
    CHECK(first.events.size() == 1 && first.events[0].time == 0 && first.events[0].offset == 0);
    render();
    CHECK(first.events.size() == 3 && !first.events[1].on && first.events[2].on);
    CHECK(first.events[1].time == 24000 && first.events[2].time == 24000);
    render(); render(); CHECK(first.sounding == 0);

    // Live preempts an arrangement voice at its actual offset. Its late release
    // cannot kill the next arrangement attack; missed attacks are never chased.
    restart(); render(12000);
    const auto preempt = first.events.size();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 17);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 101);
    render(12000);
    CHECK(first.events.size() == preempt + 3);
    CHECK(!first.events[preempt].on && first.events[preempt].offset == 17);
    CHECK(first.events[preempt + 1].on && first.events[preempt + 1].offset == 17);
    CHECK(!first.events[preempt + 2].on && first.events[preempt + 2].offset == 101);
    CHECK(first.sounding == 0);
    render(12000); CHECK(first.sounding == 1); // Next arrangement attack at b1.
    const auto stale = first.events.size();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 5); render(12000);
    CHECK(first.events.size() == stale && first.sounding == 1);

    state.stop(); render(1);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render(1);
    CHECK(first.sounding == 1); // Stopped audition.
    state.setPlaying(true); // b2: independent pitch 62.
    render(1); CHECK(first.sounding == 2);
    mixer.setActiveChannel(1); render(1);
    CHECK(first.sounding == 1 && first.held[62] == 1); // Only live pitch was cleaned.

    restart(); render(12000);
    const auto seekOffs = first.noteOffs;
    state.setPositionInBeats(0.75); render(6000);
    CHECK(first.sounding == 0 && first.noteOffs == seekOffs + 1 && first.lastOffSample == 0);
    const auto attacks = first.notes;
    render(1); CHECK(first.notes == attacks + 1); // Exact b1 attack after seek through b0 note.
    state.setPositionInBeats(1.5); render(12001);
    CHECK(first.held[60] == 0 && first.held[62] == 1); // No orphan b2 off for missed b1 attack.

    // Publication is real AsyncUpdater delivery, not direct test injection.
    restart(); render(12000); placement->setMuted(true); dispatch(); render(1);
    CHECK(first.sounding == 0 && first.lastOffSample == 0);
    placement->setMuted(false); dispatch(); render(1); CHECK(first.sounding == 0);
    restart(); render(12000); notes->setMuted(true); dispatch(); render(1); CHECK(first.sounding == 0);
    notes->setMuted(false); dispatch(); restart(); render(12000);
    Note edited(65, 1, 1); notes->updateNote(&notes->getNotes()[0], edited); dispatch(); render(12000);
    CHECK(first.sounding == 0); // Replacement cleans, no chase of old note.
    render(1); CHECK(first.held[60] == 1 && first.held[65] == 1);
    placement->setChannelId(b->getId()); dispatch(); render(1);
    CHECK(first.sounding == 0 && second.sounding == 0);
    restart(); render(24000); render(1); CHECK(second.sounding == 2);
    channels.moveChannel(1, 0); dispatch(); render(1); // Stable ID remains B, conservative revision cleanup.
    restart(); render(24000); render(1); CHECK(second.sounding == 2 && first.sounding == 0);
    b->setMuted(true); dispatch(); render(1); CHECK(second.sounding == 0);
    b->setMuted(false); dispatch(); restart(); render(24000); render(1); CHECK(second.sounding == 2);
    track->removeClipInstance(0); dispatch(); render(1); CHECK(second.sounding == 0);
    track->addClipInstance(std::make_unique<ClipInstance>(id, b->getId(), 0, 4)); dispatch();
    restart(); render(24000); render(1); CHECK(second.sounding == 2);
    clips.removeClip(id); dispatch(); render(1); CHECK(second.sounding == 0);
    channels.removeChannel(0); dispatch(); render(1);
    CHECK(second.destroyed == 1 && !second.destroyedOnAudio && first.sounding == 0);
}

static void arrangementCapacityTests() {
    Probe probe, replacement, other;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* channel = channels.addChannel();
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    auto* independent = channels.addChannel();
    independent->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(other)));
    auto source = std::make_unique<MidiClip>(0, 8);
    auto* notes = source.get();
    notes->addNote(Note(60, 0, 1)); notes->addNote(Note(60, 1, 1));
    const auto id = clips.addClip(std::move(source));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, 8));
    auto independentSource = std::make_unique<MidiClip>(0, 8);
    independentSource->addNote(Note(70, 0, 8));
    const auto independentId = clips.addClip(std::move(independentSource));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(independentId, independent->getId(), 0, 8));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.setActiveChannel(0); mixer.prepareToPlay(48000, 24000);
    juce::AudioBuffer<float> buffer(2, 24000);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto render = [&](int n = 24000) {
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0 && probe.lastInputCount <= 2048);
    };
    auto dispatch = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(20); };
    auto restart = [&] { state.setPositionInBeats(0); state.setPlaying(true); };
    render(1); // Initial cleanup while stopped.
    const auto beforeTap = probe.notes;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    render(1); CHECK(probe.notes == beforeTap && probe.sounding == 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render(1);
    const auto held = probe.notes;
    state.setPlaying(true);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 23); render();
    CHECK(probe.notes == held && probe.sounding == 0 && probe.lastOffSample == 23);
    render(); CHECK(probe.notes == held + 1 && probe.sounding == 1);
    render(1); CHECK(probe.sounding == 0);

    // Quantized sub-sample silence and both sides of an exact 64-sample edge.
    notes->clearNotes();
    notes->addNote(Note(61, 63.25 / 24000, 0.25 / 24000));
    notes->addNote(Note(62, 63.75 / 24000, 0.5 / 24000));
    notes->addNote(Note(63, 64.0 / 24000, 64.0 / 24000));
    dispatch(); restart(); probe.events.clear(); render(64);
    CHECK(probe.events.size() == 1 && probe.events[0].pitch == 62 && probe.events[0].offset == 63);
    render(64);
    CHECK(probe.events.size() == 3 && !probe.events[1].on && probe.events[1].offset == 0);
    CHECK(probe.events[2].pitch == 63 && probe.events[2].on && probe.events[2].offset == 0);
    render(1); CHECK(probe.events.size() == 4 && probe.events[3].offset == 0 && probe.sounding == 0);

    notes->clearNotes();
    notes->addNote(Note(60, 0.25, 1.5, 120)); // Later attack inserted first.
    notes->addNote(Note(60, 0, 1, 30));
    dispatch(); restart(); probe.events.clear(); render(); render();
    CHECK(probe.events.size() == 2 && probe.events[0].on && probe.events[0].velocity == 30);
    CHECK(!probe.events[1].on && probe.events[1].offset == 18000 && probe.sounding == 0);

    // Tempo changes preserve a held lifetime and recompute its precise end; a
    // sample-rate reprepare cleans without chasing, then schedules future attacks.
    notes->clearNotes(); notes->addNote(Note(60, 0, 1)); notes->addNote(Note(62, 1, 1));
    dispatch(); restart(); render(12000);
    state.setTempo(60); render(23999); CHECK(probe.held[60] == 1);
    render(2); CHECK(probe.lastOffSample == 1 && probe.held[62] == 1 && probe.held[60] == 0);
    mixer.prepareToPlay(96000, 24000); render(1);
    CHECK(probe.sounding == 0 && probe.lastOffSample == 0);
    state.setTempo(120); mixer.prepareToPlay(48000, 24000);

    notes->clearNotes();
    for (unsigned i = 0; i < Channel::maxLiveEvents - 1; ++i) {
        Note note(i % 128, 0, 1); note.setChannel(i / 128 + 1); notes->addNote(note);
    }
    dispatch(); restart();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 0);
    const auto accepted = probe.notes; render();
    CHECK(probe.notes == accepted + 975 && probe.sounding == 975);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 23999); render();
    CHECK(probe.sounding == 0 && probe.lastInputCount == 976 && probe.lastOffSample == 0);
    restart(); render(); CHECK(probe.sounding == 975);
    auto rejected = probe.notes;
    auto overflows = mixer.getOverflowCount();
    // 975 arrangement releases + two live messages exceeds the shared budget.
    midi.addEvent(juce::MidiMessage::noteOn(16, 127, 0.5f), 0);
    midi.addEvent(juce::MidiMessage::noteOff(16, 127), 23999); render();
    CHECK(probe.notes == rejected && probe.sounding == 0 && probe.lastOffSample == 0);
    CHECK(mixer.getOverflowCount() == overflows + 1);
    CHECK(other.sounding == 1); // Destination-local rejection cannot silence another instrument.

    for (unsigned i = 975; i < 977; ++i) {
        Note note(i % 128, 0, 1); note.setChannel(i / 128 + 1); notes->addNote(note);
    }
    dispatch(); restart(); rejected = probe.notes; overflows = mixer.getOverflowCount(); render();
    CHECK(probe.notes == rejected && probe.sounding == 0 && mixer.getOverflowCount() == overflows + 1);
    render(); CHECK(probe.sounding == 0); // Rejected attacks never produce orphan releases.

    // Fewer than 976 events in each block can still exceed outstanding capacity.
    notes->clearNotes();
    for (unsigned i = 0; i < 1025; ++i) {
        Note note(i % 128, i < 976 ? 0 : 1, 4); note.setChannel(i / 128 + 1); notes->addNote(note);
    }
    dispatch(); restart(); render(); CHECK(probe.sounding == 976);
    rejected = probe.notes; overflows = mixer.getOverflowCount(); render();
    CHECK(probe.sounding == 0 && probe.notes == rejected && mixer.getOverflowCount() == overflows + 1);

    notes->clearNotes(); notes->addNote(Note(60, 0, 4)); dispatch(); restart(); render(1);
    CHECK(probe.held[60] == 1);
    for (size_t i = 0; i < ArrangementSnapshot::maxNotes; ++i) notes->addNote(Note(60, 0, 4));
    dispatch(); CHECK(mixer.arrangementOverflowed()); render(1);
    CHECK(probe.sounding == 0 && probe.lastOffSample == 0);

    // Arrangement cleanup uses the existing sustained-voice reset latch. While
    // pending, no attack is recorded; after service, only a future attack resumes.
    notes->clearNotes(); notes->addNote(Note(60, 0, 1)); notes->addNote(Note(62, 2, 1));
    dispatch(); CHECK(!mixer.arrangementOverflowed()); restart();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0); render(12000);
    state.setPositionInBeats(1); render(1);
    CHECK(channel->isVoiceResetPending() && probe.lastOffSample == 0);
    const auto blocked = probe.blocks; render(1); CHECK(probe.blocks == blocked);
    ChannelTestAccess::resetVoices(*channel);
    CHECK(probe.sounding == 0 && !channel->isVoiceResetPending() && probe.resetQuiescent && !probe.resetOnAudio);
    render(23998); CHECK(probe.sounding == 0);
    render(1); CHECK(probe.held[62] == 1 && !channel->isVoiceResetPending());
    state.stop(); render(1); CHECK(channel->isVoiceResetPending());
    ChannelTestAccess::resetVoices(*channel); CHECK(probe.sounding == 0);
    restart(); render(1); CHECK(probe.held[60] == 1);
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(replacement)));
    dispatch(); render(1);
    CHECK(probe.destroyed == 1 && !probe.destroyedOnAudio && replacement.notes == 0);
    state.setPositionInBeats(2); render(1); CHECK(replacement.held[62] == 1);
    channels.removeChannel(0); dispatch(); render(1);
    CHECK(replacement.destroyed == 1 && !replacement.destroyedOnAudio);
}

static void arrangementBoundaryOwnershipTests() {
    constexpr double rate = 48000, tempo = 137;
    const double spb = rate * 60 / tempo;
    for (double seek : {0.0, 2165.993971306134}) {
        const double boundary = seek + 127 * (tempo / 60) / rate;
        const double epsilon = 1e-7 + 8 * std::numeric_limits<double>::epsilon() * seek * spb;
        const double threshold = seek + (127 - epsilon) / spb;
        const double reported = seek == 0 ? threshold : 2166.00001262557;
        for (double end : {reported, boundary, std::nextafter(boundary, 0.0),
                           std::nextafter(boundary, std::numeric_limits<double>::infinity()), threshold,
                           std::nextafter(threshold, 0.0),
                           std::nextafter(threshold, std::numeric_limits<double>::infinity())})
        for (int size : {1, 7, 64, 127}) for (double nextTempo : {20.0, 137.0, 300.0}) {
            Probe probe;
            ChannelList channels;
            TrackList tracks;
            ClipPool clips;
            TransportState state;
            auto* channel = channels.addChannel();
            channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
            auto source = std::make_unique<MidiClip>(0, end + 1);
            source->addNote(Note(60, seek, end - seek));
            // The release and touching attack must share ownership and off-first order.
            source->addNote(Note(61, end, 100 / spb));
            const auto id = clips.addClip(std::move(source));
            tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, end + 1));
            ChannelMixer mixer(channels, tracks, clips, state);
            mixer.prepareToPlay(rate, size);
            state.setTempo(tempo); state.setPositionInBeats(seek); state.setPlaying(true);
            juce::AudioBuffer<float> buffer(2, size);
            juce::MidiBuffer midi; midi.ensureSize(32768);
            for (int time = 0; time < 1024;) {
                if (time == 127) state.setTempo(nextTempo);
                const int n = std::min(size, (time < 127 ? 127 : 1024) - time);
                float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
                juce::AudioBuffer<float> block(pointers, 2, n);
                rendering = true; mixer.processBlock(block, midi); rendering = false;
                CHECK(renderAllocations == 0 && renderDeletions == 0);
                time += n;
            }
            CHECK(probe.events.size() == 4 && probe.sounding == 0 && mixer.getOverflowCount() == 0);
            CHECK(probe.events[0].on && probe.events[0].pitch == 60 && probe.events[0].time == 0);
            CHECK(!probe.events[1].on && probe.events[1].pitch == 60);
            CHECK(probe.events[2].on && probe.events[2].pitch == 61);
            CHECK(probe.events[1].time == probe.events[2].time);
            CHECK(probe.events[1].time >= 126 && probe.events[1].time <= 127);
            if (end >= boundary || (size == 127 && seek != 0 && end == reported))
                CHECK(probe.events[1].time == 127);
            CHECK(!probe.events[3].on && probe.events[3].pitch == 61);
            const double remaining = (end + 100 / spb - boundary) * rate * 60 / nextTempo;
            CHECK(std::abs(probe.events[3].time - (127 + remaining)) < 1.000001);
        }
    }
}

static void arrangementMergedCapacityTests() {
    for (bool scheduledRelease : {false, true}) {
        Probe first, second;
        ChannelList channels;
        TrackList tracks;
        ClipPool clips;
        TransportState state;
        auto* a = channels.addChannel();
        auto* b = channels.addChannel();
        a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
        b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
        auto source = std::make_unique<MidiClip>(0, 16);
        // Fill 1024 delivered arrangement keys over two blocks, under the event budget.
        for (unsigned i = 0; i < 1024; ++i) {
            const double start = i < 512 ? 0 : 1;
            Note note(i % 128, start, i == 0 && scheduledRelease ? 2 : 8);
            note.setChannel(i / 128 + 1); source->addNote(note);
        }
        const auto id = clips.addClip(std::move(source));
        auto* track = tracks.addTrack();
        track->addClipInstance(std::make_unique<ClipInstance>(id, a->getId(), 0, 16));
        auto other = std::make_unique<MidiClip>(0, 16);
        other->addNote(Note(60, 0, 12));
        const auto otherId = clips.addClip(std::move(other));
        track->addClipInstance(std::make_unique<ClipInstance>(otherId, b->getId(), 0, 16));
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.prepareToPlay(48000, 24000); mixer.setActiveChannel(0); state.setPlaying(true);
        juce::AudioBuffer<float> buffer(2, 24000);
        juce::MidiBuffer midi; midi.ensureSize(32768);
        auto render = [&] {
            rendering = true; mixer.processBlock(buffer, midi); rendering = false;
            CHECK(renderAllocations == 0 && renderDeletions == 0);
            CHECK(first.lastInputCount <= 2048 && second.lastInputCount <= 2048);
        };
        render(); render(); CHECK(first.sounding == 1024 && second.sounding == 1);
        const auto previous = first.events.size();
        const int offset = scheduledRelease ? 0 : 37;
        const int midiChannel = scheduledRelease ? 16 : 1;
        midi.addEvent(juce::MidiMessage::noteOn(midiChannel, 0, 0.5f), offset);
        render();
        CHECK(mixer.getOverflowCount() == 0 && first.sounding == 1024);
        CHECK(first.events.size() == previous + 2);
        CHECK(!first.events[previous].on && first.events[previous].channel == 1 && first.events[previous].pitch == 0);
        CHECK(first.events[previous].offset == offset && first.events[previous].time == 48000 + offset);
        CHECK(first.events[previous + 1].on && first.events[previous + 1].channel == midiChannel);
        CHECK(first.events[previous + 1].offset == offset && first.events[previous + 1].time == 48000 + offset);
        CHECK(second.sounding == 1 && second.events.size() == 1 && second.noteOffs == 0);
        midi.addEvent(juce::MidiMessage::noteOff(midiChannel, 0), 19); render();
        CHECK(first.sounding == 1023 && first.lastOffSample == 19 && mixer.getOverflowCount() == 0);
        for (int i = 0; i < 10; ++i) render();
        CHECK(first.sounding == 0 && first.notes == 1025 && first.noteOffs == 1025);
        CHECK(second.events.size() == 2 && second.events[1].time == 288000 && !second.events[1].on);
        CHECK(second.sounding == 0 && mixer.getOverflowCount() == 0);
    }
}

static void independentMixerModelTests() {
    ChannelList channels;
    CHECK(channels.getNumChannels() == 0 && channels.getNumMixerChannels() == 1);
    auto* bus = channels.getMixerChannels().front().get();
    const auto originalId = bus->getId();
    CHECK(originalId >= 0 && bus->getName().isNotEmpty());
    CHECK(bus->getVolume() == 1 && bus->getPan() == 0 && !bus->isMuted() && !bus->isSolo());
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    CHECK(a->getMixerTrackId() == MasterDestination && b->getMixerTrackId() == MasterDestination);
    CHECK(channels.getNumMixerChannels() == 1);
    CHECK(channels.setChannelMixerDestination(a->getId(), originalId));
    CHECK(channels.setChannelMixerDestination(b->getId(), originalId));
    CHECK(!channels.setChannelMixerDestination(a->getId(), -2));
    CHECK(!channels.setChannelMixerDestination(a->getId(), originalId + 100));
    CHECK(!channels.setChannelMixerDestination(InvalidChannelId, originalId));
    CHECK(a->getMixerTrackId() == originalId && b->getMixerTrackId() == originalId);
    channels.moveChannel(0, 1);
    CHECK(channels.getMixerChannelById(originalId) == bus && a->getMixerTrackId() == originalId);
    bus->setVolume(-1); bus->setPan(9);
    CHECK(bus->getVolume() == 0 && bus->getPan() == 1);
    bus->setVolume(9); bus->setPan(-9);
    CHECK(bus->getVolume() == 2 && bus->getPan() == -1);
    bus->setVolume(std::numeric_limits<float>::quiet_NaN());
    bus->setPan(std::numeric_limits<float>::infinity());
    CHECK(bus->getVolume() == 2 && bus->getPan() == -1);
    CHECK(a->getVolume() == 1 && a->getPan() == 0);
    CHECK(!channels.restoreMixerChannel(originalId, "Duplicate"));
    CHECK(!channels.restoreMixerChannel(-1, "Master is not a bus"));
    CHECK(!channels.restoreMixerChannel(std::numeric_limits<int>::max(), "Overflow"));
    auto* other = channels.restoreMixerChannel(42, "Independent"); CHECK(other);
    channels.removeMixerChannel(originalId);
    CHECK(a->getMixerTrackId() == MasterDestination && b->getMixerTrackId() == MasterDestination);
    CHECK(channels.getNumChannels() == 2 && channels.getMixerChannelById(42) == other);
    auto* fresh = channels.addMixerChannel(); CHECK(fresh && fresh->getId() > 42);
    const auto freshId = fresh->getId();
    CHECK(channels.setChannelMixerDestination(a->getId(), freshId));
    channels.removeMixerChannel(originalId); // Stale removal cannot alias a new bus.
    CHECK(a->getMixerTrackId() == freshId);
    channels.removeChannel(channels.indexOfChannel(b));
    CHECK(channels.getNumMixerChannels() == 2);
    channels.clearMixerChannels();
    CHECK(channels.getNumChannels() == 1 && a->getMixerTrackId() == MasterDestination);
    fresh = channels.addMixerChannel(); CHECK(fresh && fresh->getId() > freshId);
    channels.clearChannels(); CHECK(channels.getNumMixerChannels() == 1);
    while (channels.getNumMixerChannels() < ChannelList::maxMixerChannels) CHECK(channels.addMixerChannel());
    CHECK(!channels.addMixerChannel() && !channels.restoreMixerChannel(999, "Full"));
    CHECK(channels.addChannel()->getMixerTrackId() == MasterDestination); // Independent capacities.
    channels.clearMixerChannels();
    CHECK(channels.getNumMixerChannels() == 0 && channels.getNumChannels() == 1);
}

static void independentMixerSignalTests() {
    Probe first, second, third;
    first.constantOutput = second.constantOutput = third.constantOutput = true;
    first.rightOutput = 0.5f;
    second.leftOutput = 0.125f; second.rightOutput = 0.25f;
    third.leftOutput = 0.0625f; third.rightOutput = 0.125f;
    ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
    auto* a = channels.addChannel("A"); auto* b = channels.addChannel("B"); auto* c = channels.addChannel("Direct");
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    c->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(third)));
    auto* x = channels.getMixerChannels().front().get();
    auto* y = channels.restoreMixerChannel(42, "Other bus"); CHECK(y); // IDs are not buffer indices.
    CHECK(channels.setChannelMixerDestination(a->getId(), x->getId()));
    CHECK(channels.setChannelMixerDestination(b->getId(), x->getId()));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.prepareToPlay(48000, 127);
    juce::AudioBuffer<float> buffer(2, 127);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto& master = channels.getMasterBus();
    const auto near = [](float actual, float expected) { return std::abs(actual - expected) < 1.0e-5f; };
    auto render = [&](float left, float right, float xl, float xr, float yl = 0, float yr = 0, int outputs = 2) {
        master.meter.clear();
        for (const auto& bus : channels.getMixerChannels()) bus->meter.clear();
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, outputs, 127);
        const auto calls = first.blocks;
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(first.blocks == calls + 1 && second.blocks == first.blocks && third.blocks == first.blocks);
        for (int i = 0; i < 127; ++i) {
            CHECK(near(block.getSample(0, i), left));
            if (outputs == 2) CHECK(near(block.getSample(1, i), right));
        }
        CHECK(near(master.meter.getLeft(), std::abs(left)) && near(master.meter.getRight(), std::abs(right)));
        CHECK(near(x->getMeter().getLeft(), std::abs(xl)) && near(x->getMeter().getRight(), std::abs(xr)));
        CHECK(near(y->getMeter().getLeft(), std::abs(yl)) && near(y->getMeter().getRight(), std::abs(yr)));
    };
    render(0.4375f, 0.875f, 0.375f, 0.75f); // Two instruments summed once into x, c bypasses it.
    x->setVolume(0.5f); x->setPan(0.5f);
    render(0.15625f, 0.5f, 0.09375f, 0.375f);
    CHECK(near(a->getLeftLevel(), 0.25f) && near(b->getRightLevel(), 0.25f)); // Instrument meters are pre-bus.
    master.setGain(0.5f); render(0.078125f, 0.25f, 0.09375f, 0.375f);
    master.setMuted(true); render(0, 0, 0.09375f, 0.375f);
    master.setMuted(false); master.setGain(1);
    x->setMuted(true); render(0.0625f, 0.125f, 0, 0); // Direct Master is unaffected by bus mute.
    x->setMuted(false); x->setVolume(1); x->setPan(0);
    CHECK(channels.setChannelMixerDestination(b->getId(), y->getId()));
    x->setSolo(true); render(0.25f, 0.5f, 0.25f, 0.5f); // Solo also gates direct Master audio.
    y->setSolo(true); render(0.375f, 0.75f, 0.25f, 0.5f, 0.125f, 0.25f);
    x->setMuted(true); render(0.125f, 0.25f, 0, 0, 0.125f, 0.25f);
    y->setSolo(false); render(0, 0, 0, 0); // Muted solo still participates in anySolo.
    x->setSolo(false); x->setMuted(false);
    y->setVolume(0);
    for (float pan : {-1.0f, -0.00001f, 0.0f, 0.00001f, 1.0f}) {
        x->setPan(pan);
        const float left = 0.25f * (1 - std::max(0.0f, pan));
        const float right = 0.5f * (1 + std::min(0.0f, pan));
        render(left + 0.0625f, right + 0.125f, left, right);
        render(0.3125f, 0.3125f, 0.25f, 0.25f, 0, 0, 1); // Mono balance stays unity.
    }
    x->setPan(-0.5f); x->setVolume(0.5f); y->setVolume(1);
    CHECK(channels.setChannelMixerDestination(b->getId(), x->getId()));
    a->setVolume(2); a->setPan(0.5f); b->setVolume(0.5f); b->setPan(-0.5f);
    render(0.21875f, 0.390625f, 0.15625f, 0.265625f); // Instrument controls then summed-bus controls.
    a->setVolume(1); a->setPan(0); b->setVolume(1); b->setPan(0); x->setVolume(1); x->setPan(0);
    second.leftOutput = -0.25f; second.rightOutput = -0.5f;
    render(0.0625f, 0.125f, 0, 0); // Bus meter measures summed audio, not summed magnitudes.
    second.leftOutput = 0.125f; second.rightOutput = 0.25f;
    a->setMixerTrackId(999999); // Defensive render fallback for a route outside the validated UI API.
    render(0.4375f, 0.875f, 0.125f, 0.25f);
    x->setSolo(true); render(0.125f, 0.25f, 0.125f, 0.25f); // Invalid fallback obeys direct-Master solo policy.
    x->setSolo(false);

    // Add destinations after preparation, including the last preallocated slot.
    mixer.prepareToPlay(96000, 127);
    MixerChannel* added = nullptr;
    while (channels.getNumMixerChannels() < ChannelList::maxMixerChannels) added = channels.addMixerChannel();
    CHECK(added && channels.setChannelMixerDestination(a->getId(), added->getId()));
    CHECK(added->getId() >= ChannelList::maxMixerChannels);
    render(0.4375f, 0.875f, 0.125f, 0.25f);
    CHECK(near(added->getMeter().getLeft(), 0.25f));
    first.constantOutput = false;
    rendering = true; mixer.processBlock(buffer, midi); rendering = false;
    const auto decay = static_cast<float>(0.25 * std::pow(0.01, 127.0 / (96000 * 0.5)));
    CHECK(near(added->getMeter().getLeft(), decay)); // Structure-time meter preparation uses the current rate.
    CHECK(renderAllocations == 0 && renderDeletions == 0);
    channels.clearMixerChannels();
    CHECK(a->getMixerTrackId() == MasterDestination && b->getMixerTrackId() == MasterDestination);
    rendering = true; mixer.processBlock(buffer, midi); rendering = false;
    CHECK(near(buffer.getSample(0, 0), 0.1875f) && near(buffer.getSample(1, 0), 0.375f));
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void independentMixerNoteTests() {
    Probe first, second;
    ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
    auto* a = channels.addChannel("Arrangement"); auto* b = channels.addChannel("Live");
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    auto* x = channels.getMixerChannels().front().get();
    auto* y = channels.restoreMixerChannel(17, "Reroute"); CHECK(y); y->setVolume(0.5f);
    const auto xid = x->getId(), yid = y->getId();
    CHECK(channels.setChannelMixerDestination(a->getId(), xid));
    CHECK(channels.setChannelMixerDestination(b->getId(), xid));
    auto source = std::make_unique<MidiClip>(0, 8);
    source->addNote(Note(60, 0, 4));
    source->addNote(Note(62, 64.0 / 24000, 64.0 / 24000)); // Attack and release while the bus is muted.
    const auto clipId = clips.addClip(std::move(source));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(clipId, a->getId(), 0, 8));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.prepareToPlay(48000, 64); mixer.setActiveChannel(1); state.setPlaying(true);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0);
    int liveAttacks = 1, liveReleases = 0, arrangementAttacks = 1, arrangementReleases = 0;
    auto render = [&](float level) {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20); // Includes route/name notifications.
        const auto calls = first.blocks;
        rendering = true; mixer.processBlock(buffer, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(first.blocks == calls + 1 && second.blocks == first.blocks);
        CHECK(first.held[60] == 1 && second.held[72] == 1 && first.notes == arrangementAttacks && second.notes == liveAttacks);
        CHECK(first.sounding == arrangementAttacks - arrangementReleases && second.sounding == liveAttacks);
        CHECK(first.noteOffs == arrangementReleases && second.noteOffs == liveReleases && first.resets == 0 && second.resets == 0);
        CHECK(!a->isVoiceResetPending() && !b->isVoiceResetPending());
        CHECK(mixer.getActiveChannelId() == b->getId() && mixer.getOverflowCount() == 0);
        for (int i = 0; i < 64; ++i) CHECK(std::abs(buffer.getSample(0, i) - level) < 1.0e-6f);
    };
    render(0.5f);
    x->setMuted(true); ++arrangementAttacks; render(0); CHECK(first.held[62] == 1);
    midi.addEvent(juce::MidiMessage::noteOn(1, 74, 0.5f), 0); ++liveAttacks;
    ++arrangementReleases;
    render(0); CHECK(first.held[62] == 0 && second.held[74] == 1); // Bus mute does not suppress fresh MIDI either.
    midi.addEvent(juce::MidiMessage::noteOff(1, 74), 3); ++liveReleases;
    render(0); CHECK(second.held[74] == 0 && second.lastOffSample == 3);
    x->setMuted(false); render(0.5f); // Held voices return without MIDI retrigger/chase.
    y->setSolo(true); render(0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 76, 0.5f), 0); ++liveAttacks;
    render(0); CHECK(second.held[76] == 1); // Solo suppression is audio-only as well.
    midi.addEvent(juce::MidiMessage::noteOff(1, 76), 7); ++liveReleases;
    render(0); CHECK(second.held[76] == 0 && second.lastOffSample == 7);
    CHECK(channels.setChannelMixerDestination(a->getId(), yid)); render(0.125f);
    CHECK(channels.setChannelMixerDestination(b->getId(), MasterDestination)); render(0.125f);
    y->setSolo(false); render(0.375f);
    CHECK(channels.setChannelMixerDestination(b->getId(), yid)); render(0.25f);
    y->setName("Renamed"); y->setColour(juce::Colours::orange); render(0.25f);
    channels.removeMixerChannel(xid); render(0.25f); // Compact bus indices without changing y's identity.
    channels.removeMixerChannel(yid); render(0.5f);
    CHECK(a->getMixerTrackId() == MasterDestination && b->getMixerTrackId() == MasterDestination);
    channels.removeMixerChannel(xid); render(0.5f);
    state.stop();
    rendering = true; mixer.processBlock(buffer, midi); rendering = false;
    CHECK(first.noteOffs == arrangementReleases + 1 && second.noteOffs == liveReleases + 1 && first.held[60] == 0 && second.held[72] == 0);
    CHECK(b->isVoiceResetPending()); // Original pedal/voice ledger survived all audio-only edits.
    ChannelTestAccess::resetVoices(*b);
    CHECK(first.sounding == 0 && second.sounding == 0 && second.resetQuiescent && !second.resetOnAudio);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void mixerEditEngineTests() {
    using Interruption = AudioQuiescence::Interruption;
    for (int scenario = 0; scenario < 10; ++scenario) {
        Probe arranged, live;
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        auto* a = channels.addChannel("Arrangement"); auto* b = channels.addChannel("Live");
        a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(arranged)));
        b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(live)));
        auto* bus = channels.getMixerChannels().front().get();
        const auto busId = bus->getId(); bus->setVolume(0.5f);
        CHECK(channels.setChannelMixerDestination(a->getId(), busId));
        CHECK(channels.setChannelMixerDestination(b->getId(), busId));
        auto source = std::make_unique<MidiClip>(0, 4);
        source->addNote(Note(60, 0, 128.0 / 24000));
        const auto clipId = clips.addClip(std::move(source));
        tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(clipId, a->getId(), 0, 4));
        ChannelMixer mixer(channels, tracks, clips, state); mixer.setActiveChannel(1);
        AudioEngine engine; engine.setProcessor(&mixer);
        AudioEngineTestAccess::prepare(engine, 48000, 64);
        juce::AudioBuffer<float> buffer(2, 64);
        const auto render = [&] {
            rendering = true; AudioEngineTestAccess::render(engine, buffer); rendering = false;
            CHECK(renderAllocations == 0 && renderDeletions == 0);
            state.pollRenderPosition();
        };
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        render(); // Consume preparation panic while stopped, before delivering any notes.
        state.setPlaying(true);
        engine.handleNoteOn(nullptr, 1, 72, 0.5f); engine.handleNoteOn(nullptr, 1, 74, 0.5f);
        render();
        CHECK(arranged.held[60] == 1 && live.held[72] == 1 && live.held[74] == 1);
        CHECK(arranged.notes == 1 && live.notes == 2 && arranged.noteOffs == 0 && live.noteOffs == 0);
        CHECK(std::abs(state.getPositionInBeats() - 64.0 / 24000) < 1e-10);
        const auto revision = state.acquireRenderPosition().revision;
        engine.handleNoteOff(nullptr, 1, 74, 0); engine.handleNoteOn(nullptr, 1, 76, 0.5f);

        int blockedCalls = 0;
        const auto blockedRender = [&](bool expectedReset) {
            bool reset = !expectedReset;
            CHECK(!AudioQuiescence::instance().enter(&reset) && reset == expectedReset);
            const auto position = state.acquireRenderPosition();
            const auto calls = arranged.blocks, liveCalls = live.blocks;
            const auto skipped = engine.getSkippedBlockCount();
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), 0.875f, 64);
            render();
            CHECK(engine.getSkippedBlockCount() == skipped + 1);
            CHECK(buffer.getMagnitude(0, 64) == 0);
            CHECK(arranged.blocks == calls && live.blocks == liveCalls);
            CHECK(state.acquireRenderPosition().beats == position.beats &&
                  state.acquireRenderPosition().revision == position.revision);
            CHECK(state.getPositionInBeats() == position.beats);
            CHECK(arranged.held[60] == 1 && live.held[72] == 1 && live.held[74] == 1 && live.held[76] == 0);
            CHECK(arranged.noteOffs == 0 && live.noteOffs == 0 && arranged.resets == 0 && live.resets == 0);
            ++blockedCalls;
        };
        {
            // Synchronous list notifications occur inside the real structural edit.
            struct DuringMixerEdit : ChannelList::Listener {
                DuringMixerEdit(ChannelList& owner, std::function<void()> callback)
                    : list(owner), render(std::move(callback)) { list.addListener(this); }
                ~DuringMixerEdit() override { list.removeListener(this); }
                void channelAdded(Channel*) override {}
                void channelRemoved(int) override {}
                void channelChanged(Channel*) override {}
                void channelListChanged() override {}
                void mixerChannelsChanged() override { ++notifications; render(); }
                ChannelList& list;
                std::function<void()> render;
                int notifications = 0;
            } listener(channels, [&] { blockedRender(false); });
            switch (scenario) {
                case 0: CHECK(channels.addMixerChannel("Add during playback")); break;
                case 1: {
                    AudioQuiescence::Edit edit(Interruption::PreserveVoices);
                    CHECK(channels.setChannelMixerDestination(b->getId(), MasterDestination));
                    blockedRender(false); // The route setter's nested edit must not upgrade to reset.
                    break;
                }
                case 2: channels.removeMixerChannel(busId); break;
                case 3: channels.clearMixerChannels(); break;
                case 4: CHECK(channels.restoreMixerChannel(42, "Restore during playback")); break;
                case 5: {
                    AudioQuiescence::Edit outer(Interruption::PreserveVoices);
                    blockedRender(false);
                    { AudioQuiescence::Edit destructive; blockedRender(true); }
                    blockedRender(true); // Upgrade remains sticky after the destructive child exits.
                    break;
                }
                case 6: {
                    AudioQuiescence::Edit outer;
                    { AudioQuiescence::Edit preserving(Interruption::PreserveVoices); blockedRender(true); }
                    blockedRender(true); // Preserving children cannot downgrade a destructive parent.
                    break;
                }
                case 7: {
                    engine.requestPanic();
                    AudioQuiescence::Edit edit(Interruption::PreserveVoices);
                    blockedRender(false); // Preserve must not clear a panic already waiting for admission.
                    break;
                }
                case 8: {
                    // Two events are queued already; one more than ingress capacity requests cleanup.
                    for (int i = 0; i < 2047; ++i)
                        engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::controllerEvent(1, 7, 100));
                    CHECK(engine.getMidiOverflowCount() == 1);
                    AudioQuiescence::Edit edit(Interruption::PreserveVoices);
                    blockedRender(false);
                    break;
                }
                case 9: {
                    { AudioQuiescence::Edit destructive; blockedRender(true); }
                    AudioQuiescence::Edit preserving(Interruption::PreserveVoices);
                    blockedRender(false); // A later preserving rejection must retain the earlier panic.
                    break;
                }
            }
            CHECK(listener.notifications == (scenario == 0 || scenario == 2 || scenario == 3 || scenario == 4 ? 1 : 0));
        }
        CHECK(blockedCalls > 0 && engine.getSkippedBlockCount() == static_cast<unsigned>(blockedCalls));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        const auto calls = arranged.blocks;
        render();
        CHECK(arranged.blocks == calls + 1 && live.blocks == arranged.blocks);
        CHECK(std::abs(state.getPositionInBeats() - 128.0 / 24000) < 1e-10);
        CHECK(mixer.getActiveChannelId() == b->getId() && mixer.getOverflowCount() == 0);
        CHECK(engine.getMidiOverflowCount() == (scenario == 8 ? 1u : 0u));
        if (scenario < 5) {
            CHECK(state.acquireRenderPosition().revision == revision);
            CHECK(arranged.held[60] == 1 && arranged.notes == 1 && arranged.noteOffs == 0);
            CHECK(live.held[72] == 1 && live.held[74] == 0 && live.held[76] == 1);
            CHECK(live.notes == 3 && live.noteOffs == 1 && live.lastOffSample == 0);
            CHECK(arranged.lastInputCount == 0 && live.lastInputCount == 2); // No hidden cleanup CCs.
            const float expected = scenario == 1 ? 0.375f : (scenario == 2 || scenario == 3 ? 0.5f : 0.25f);
            for (int ch = 0; ch < 2; ++ch)
                for (int sample = 0; sample < 64; ++sample) CHECK(buffer.getSample(ch, sample) == expected);
            render(); // Frozen clock resumes the scheduled release at the original boundary, not early.
            CHECK(arranged.noteOffs == 1 && arranged.lastOffSample == 0 && arranged.sounding == 0);
            CHECK(live.sounding == 2 && live.noteOffs == 1);
            engine.handleNoteOff(nullptr, 1, 72, 0); engine.handleNoteOff(nullptr, 1, 76, 0);
            render(); CHECK(live.sounding == 0 && live.noteOffs == 3);
        } else {
            CHECK(state.acquireRenderPosition().revision != revision);
            CHECK(arranged.notes == 1 && arranged.noteOffs == 1 && arranged.sounding == 0);
            CHECK(live.notes == 2 && live.noteOffs == 2 && live.sounding == 0); // Queued attack was discarded.
            CHECK(live.held[72] == 0 && live.held[74] == 0 && live.held[76] == 0);
            CHECK(arranged.lastOffSample == 0 && live.lastOffSample == 0 && buffer.getMagnitude(0, 64) == 0);
            render(); CHECK(arranged.noteOffs == 1 && live.noteOffs == 2); // Cleanup happens only once.
        }
        CHECK(!a->isVoiceResetPending() && !b->isVoiceResetPending() && arranged.resets == 0 && live.resets == 0);
        engine.clearProcessor();
    }
}

static void mixerSignalTests() {
    Probe first, second, third;
    first.constantOutput = second.constantOutput = true;
    first.rightOutput = 0.5f;
    second.leftOutput = 0.125f; second.rightOutput = 0.25f;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    auto* c = channels.addChannel("C");
    c->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(third)));
    c->setVolume(0); // A third distinct state catches short-circuit indexing in anySolo.
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    auto& master = channels.getMasterBus();
    ChannelMixer mixer(channels, tracks, clips, state);
    juce::AudioBuffer<float> buffer(2, 127);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto near = [](float a, float b) { return std::abs(a - b) < 1.0e-5f; };
    auto render = [&](float left, float right, float al, float ar, float bl, float br, int outputs = 2) {
        // Clear meter history so each assertion measures this actual signal, not a release envelope.
        mixer.prepareToPlay(48000, 127);
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, outputs, 127);
        const auto calls = first.blocks;
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(first.blocks == calls + 1 && second.blocks == first.blocks);
        for (int i = 0; i < 127; ++i) {
            CHECK(near(block.getSample(0, i), left));
            if (outputs == 2) CHECK(near(block.getSample(1, i), right));
        }
        CHECK(near(a->getLeftLevel(), al) && near(a->getRightLevel(), ar));
        CHECK(near(b->getLeftLevel(), bl) && near(b->getRightLevel(), br));
        CHECK(near(master.meter.getLeft(), std::abs(left)) && near(master.meter.getRight(), std::abs(right)));
    };
    CHECK(a->getVolume() == 1 && a->getPan() == 0 && !a->isMuted() && !a->isSolo());
    CHECK(master.getGain() == 1 && !master.isMuted());
    render(0.375f, 0.75f, 0.25f, 0.5f, 0.125f, 0.25f);
    a->setVolume(2); b->setVolume(0.5f);
    a->setPan(0.5f); b->setPan(-0.5f);
    render(0.3125f, 1.0625f, 0.25f, 1, 0.0625f, 0.0625f);
    master.setGain(0.5f);
    render(0.15625f, 0.53125f, 0.25f, 1, 0.0625f, 0.0625f);
    master.setMuted(true);
    render(0, 0, 0.25f, 1, 0.0625f, 0.0625f); // Master meter is not channel zero.
    master.setMuted(false); master.setGain(2);
    a->setVolume(1); b->setVolume(1); a->setPan(-1); b->setPan(1);
    render(0.5f, 0.5f, 0.25f, 0, 0, 0.25f);
    a->setSolo(true);
    render(0.5f, 0, 0.25f, 0, 0, 0);
    b->setSolo(true);
    render(0.5f, 0.5f, 0.25f, 0, 0, 0.25f);
    a->setMuted(true);
    render(0, 0.5f, 0, 0, 0, 0.25f); // Mute wins over solo.
    b->setSolo(false);
    render(0, 0, 0, 0, 0, 0); // Even a muted solo participates in anySolo.
    a->setSolo(false); a->setMuted(false); master.setGain(1); b->setVolume(0);
    for (float pan : {-1.0f, -0.00001f, 0.0f, 0.00001f, 1.0f}) {
        a->setPan(pan);
        const float left = 0.25f * (1 - std::max(0.0f, pan));
        const float right = 0.5f * (1 + std::min(0.0f, pan));
        render(left, right, left, right, 0, 0);
        render(0.25f, 0.25f, 0.25f, 0.25f, 0, 0, 1); // Mono ignores balance.
    }
    a->setVolume(-1); a->setPan(9); master.setGain(9);
    CHECK(a->getVolume() == 0 && a->getPan() == 1 && master.getGain() == 2);
    a->setVolume(std::numeric_limits<float>::quiet_NaN());
    a->setPan(std::numeric_limits<float>::infinity());
    master.setGain(std::numeric_limits<float>::quiet_NaN());
    CHECK(a->getVolume() == 0 && a->getPan() == 1 && master.getGain() == 2);
    a->setVolume(1); a->setPan(0); b->setVolume(1); b->setPan(0); master.setGain(1);
    second.leftOutput = -0.25f; second.rightOutput = -0.5f;
    render(0, 0, 0.25f, 0.5f, 0.25f, 0.5f); // Cancellation: never sum channel meter magnitudes.
    channels.clearChannels();
    mixer.prepareToPlay(48000, 127);
    rendering = true; mixer.processBlock(buffer, midi); rendering = false;
    CHECK(buffer.getMagnitude(0, 127) == 0 && master.meter.getLeft() == 0 && master.meter.getRight() == 0);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
    mixer.releaseResources();
}

static void mixerMeterTests() {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        float reference = 0;
        for (int partition : {1, 7, 127, 512, 4096}) {
            StereoMeter meter; meter.prepare(rate);
            juce::AudioBuffer<float> buffer(2, 4096); buffer.clear();
            buffer.setSample(0, 0, 1); buffer.setSample(1, 0, 0.5f);
            float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
            juce::AudioBuffer<float> attack(pointers, 2, 1); meter.update(attack);
            CHECK(meter.getLeft() == 1 && meter.getRight() == 0.5f);
            buffer.clear();
            int remaining = static_cast<int>(rate * 0.5);
            while (remaining > 0) {
                const int n = std::min(partition, remaining);
                juce::AudioBuffer<float> block(pointers, 2, n);
                rendering = true; meter.update(block); rendering = false;
                remaining -= n;
            }
            CHECK(std::abs(meter.getLeft() - 0.01f) < 0.00002f);
            CHECK(std::abs(meter.getRight() - 0.005f) < 0.00001f);
            if (partition == 1) reference = meter.getLeft();
            CHECK(meter.getLeft() == reference);
            meter.clear(); CHECK(meter.getLeft() == 0 && meter.getRight() == 0);
        }
    }
    CHECK(renderAllocations == 0 && renderDeletions == 0);
    StereoMeter source; source.prepare(48000);
    juce::AudioBuffer<float> buffer(1, 1); buffer.setSample(0, 0, 1);
    source.update(buffer);
    LevelMeter display;
    display.poll(source);
    CHECK(LevelMeterTestAccess::tick(display, 0.01) == 1);
    display.poll(source); // No new audio publication: old peak must not pin the UI.
    CHECK(std::abs(LevelMeterTestAccess::tick(display, 0.5) - 0.01f) < 0.0001f);
    source.update(buffer); display.poll(source);
    CHECK(LevelMeterTestAccess::tick(display, 0.01) == 1);
}

static void mixerSuppressionTests() {
    Probe first, second;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    const auto busId = channels.getMixerChannels().front()->getId();
    CHECK(channels.setChannelMixerDestination(a->getId(), busId));
    CHECK(channels.setChannelMixerDestination(b->getId(), busId));
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    auto source = std::make_unique<MidiClip>(0, 8);
    source->addNote(Note(60, 0, 4)); source->addNote(Note(62, 0.5, 1));
    source->addNote(Note(64, 1, 1)); source->addNote(Note(65, 2, 1));
    const auto id = clips.addClip(std::move(source));
    auto* track = tracks.addTrack();
    for (auto* c : {a, b}) track->addClipInstance(std::make_unique<ClipInstance>(id, c->getId(), 0, 8));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.setActiveChannel(0); mixer.prepareToPlay(48000, 24000);
    juce::AudioBuffer<float> buffer(2, 24000);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto render = [&](int n) {
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(first.lastInputCount <= 2048 && second.lastInputCount <= 2048);
    };
    auto dispatch = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(20); };
    state.setPlaying(true); render(6000);
    CHECK(first.sounding == 1 && second.sounding == 1);
    a->setVolume(0.5f); a->setPan(0.2f); a->setName("renamed"); a->setColour(juce::Colours::red);
    dispatch(); render(1);
    CHECK(first.noteOffs == 0 && second.noteOffs == 0); // No control/cosmetic snapshot panic.
    b->setSolo(true); dispatch(); render(5999);
    CHECK(first.sounding == 0 && first.noteOffs == 1 && first.lastOffSample == 0);
    CHECK(second.sounding == 1 && second.noteOffs == 0);
    const auto attacks = first.notes;
    midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 0);
    render(6000); // Consume suppressed arrangement b0.5 and live attacks.
    CHECK(first.notes == attacks && second.sounding == 2);
    b->setSolo(false); dispatch(); render(6000);
    CHECK(first.notes == attacks && first.sounding == 0); // No chase on unmute/unsolo.
    render(1); CHECK(first.held[64] == 1 && second.held[64] == 1);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 0); render(1);
    CHECK(first.held[64] == 1); // Stale live release cannot kill the new arrangement token.
    auto& master = channels.getMasterBus();
    master.setMuted(true); render(1);
    CHECK(first.held[64] == 1 && buffer.getMagnitude(0, 1) == 0);
    master.setMuted(false); render(1);
    CHECK(first.held[64] == 1 && buffer.getMagnitude(0, 1) > 0); // Master is output-only mute.
    a->setMuted(true); render(1); CHECK(first.sounding == 0);
    a->setMuted(false); render(23995); // End exactly at b2.
    CHECK(first.sounding == 0);
    render(1); CHECK(first.held[65] == 1 && first.sounding == 1);

    state.stop(); render(1);
    // Maximum outstanding ledger still fits explicit suppression cleanup, no CC reliance.
    for (unsigned i = 0; i < Channel::maxLiveEvents; ++i)
        midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 0);
    render(1);
    for (unsigned i = Channel::maxLiveEvents; i < Channel::maxDeliveredNotes; ++i)
        midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 0);
    render(1); CHECK(first.sounding == 1024);
    a->setMuted(true); render(1);
    CHECK(first.sounding == 0 && first.lastInputCount == 1072 && mixer.getOverflowCount() == 0);
    a->setMuted(false); render(1);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 0); render(1);
    b->setSolo(true); render(1);
    CHECK(a->isVoiceResetPending() && buffer.getMagnitude(0, 1) == 0);
    CHECK(first.held[70] == 0); // Explicit release precedes off-audio reset even if CCs are ignored.
    const auto calls = first.blocks; render(1); CHECK(first.blocks == calls);
    ChannelTestAccess::resetVoices(*a);
    CHECK(first.sounding == 0 && !first.resetOnAudio && first.resetQuiescent);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0); render(1);
    CHECK(first.lastInputCount == 1 && first.sounding == 0); // Pedal-up is delivered even while solo-suppressed.
    b->setSolo(false); render(1);
    CHECK(first.sounding == 0 && !a->isVoiceResetPending());
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0); render(1);
    CHECK(first.held[72] == 1);
    state.stop(); render(1); ChannelTestAccess::resetVoices(*a);
    CHECK(first.sounding == 0 && mixer.getOverflowCount() == 0);
}

static void mixerBindingTests() {
    Project project;
    auto& channels = project.getChannelList();
    auto* instrument = channels.addChannel("Instrument");
    channels.addChannel("Other instrument");
    channels.clearMixerChannels();
    auto* a = channels.addMixerChannel("Alpha");
    auto* b = channels.addMixerChannel("Beta");
    const auto aid = a->getId(), bid = b->getId();
    a->setVolume(1.75f); a->setPan(-0.4f); a->setMuted(true); a->setSolo(true);
    a->setColour(juce::Colours::red); project.setActiveChannel(1);
    const auto activeId = project.getActiveChannelId();
    project.getMasterBus().setGain(1.5f); project.getMasterBus().setMuted(true);
    std::function<void(const juce::String&)> renameReply;
    int prompts = 0;
    textPromptInterceptor() = [&](const juce::String&, const juce::String&, const juce::String&,
                                  juce::Component*, std::function<void(const juce::String&)> reply) {
        ++prompts; renameReply = std::move(reply);
    };
    struct ClearPrompt { ~ClearPrompt() { textPromptInterceptor() = {}; } } clearPrompt;
    std::function<void()> lateRename;
    {
        MixerPanel panel(project); // No peer/window, device, or application is created.
        panel.setBounds(0, 0, 160, 100);
        auto* content = panel.getContentComponent();
        auto* viewport = &panel.getViewport();
        CHECK(content && content != viewport && viewport->getParentComponent() == content);
        CHECK(viewport && viewport->getViewedComponent()->getWidth() > viewport->getWidth());
        CHECK(viewport->getViewedComponent()->getHeight() > viewport->getHeight());
        panel.setCollapsed(true, false); CHECK(!content->isVisible());
        panel.setCollapsed(false, false); CHECK(content->isVisible());
        CHECK(panel.getNumChannels() == 2);
        CHECK(panel.getSelectedMixerChannelId() == -1 && !panel.getRemoveChannelButton().isEnabled());
        panel.getChannelStrip(1)->onStripSelected();
        auto* strip = panel.getChannelStrip(0);
        CHECK(strip->getChannelId() == aid && strip->getTrackName() == "Alpha");
        CHECK(strip->getVolume() == 1.75f && strip->getPan() == -0.4f && strip->isMuted() && strip->isSolo());
        CHECK(strip->getTrackColour() == juce::Colours::red && panel.getChannelStrip(1)->isSelected());
        CHECK(panel.getMasterStrip()->getVolume() == 1.5f && panel.getMasterStrip()->isMuted());
        int feedback = 0;
        auto gainAction = strip->onVolumeChanged;
        strip->onVolumeChanged = [&](float v) { ++feedback; gainAction(v); };
        a->setVolume(0.8f); a->sendSynchronousChangeMessage();
        CHECK(feedback == 0 && strip->getVolume() == 0.8f);
        // Store actions, not pointers, to exercise late actions after rebuild/removal.
        const auto panAction = strip->onPanChanged;
        const auto muteAction = strip->onMuteToggled;
        const auto soloAction = strip->onSoloToggled;
        const auto selectAction = strip->onStripSelected;
        const auto renameAction = strip->onRenameRequested;
        channels.moveChannel(0, 1);
        CHECK(panel.getChannelStrip(0)->getChannelId() == aid && panel.getChannelStrip(1)->getChannelId() == bid);
        CHECK(panel.getChannelStrip(0)->getVolume() == 0.8f && panel.getChannelStrip(1)->isSelected());
        auto* temporary = channels.addMixerChannel("Rebuild");
        channels.removeMixerChannel(temporary->getId());
        gainAction(1.25f); panAction(0.6f); muteAction(false); soloAction(false); selectAction();
        CHECK(a->getVolume() == 1.25f && a->getPan() == 0.6f && !a->isMuted() && !a->isSolo());
        CHECK(b->getVolume() == 1 && project.getActiveChannelId() == activeId);
        CHECK(instrument->getVolume() == 1 && instrument->getPan() == 0 && !instrument->isMuted());
        CHECK(panel.getChannelStrip(0)->isSelected());
        renameAction(); CHECK(prompts == 1 && renameReply);
        renameReply("   "); CHECK(a->getName() == "Alpha");
        renameReply("  Renamed bus  "); a->sendSynchronousChangeMessage();
        CHECK(a->getName() == "Renamed bus" && panel.getChannelStrip(0)->getTrackName() == "Renamed bus");
        panel.getMasterStrip()->onVolumeChanged(0.3f); panel.getMasterStrip()->onMuteToggled(false);
        CHECK(project.getMasterBus().getGain() == 0.3f && !project.getMasterBus().isMuted());
        CHECK(channels.setChannelMixerDestination(instrument->getId(), aid));
        panel.getRemoveChannelButton().onClick();
        gainAction(2); panAction(-1); muteAction(true); soloAction(true); selectAction();
        renameAction(); renameReply("Gone"); CHECK(prompts == 1);
        CHECK(panel.getNumChannels() == 1 && b->getVolume() == 1 && !b->isMuted() && !b->isSolo());
        CHECK(project.getActiveChannelId() == activeId && instrument->getMixerTrackId() == MasterDestination);
        CHECK(!panel.getRemoveChannelButton().isEnabled());
        channels.clearMixerChannels(); CHECK(panel.getNumChannels() == 0 && panel.getChannelStrip(0) == nullptr);
        for (int i = 0; i < 20; ++i) panel.getAddChannelButton().onClick();
        CHECK(panel.getNumChannels() == 20);
        CHECK(channels.getNumChannels() == 2 && project.getActiveChannelId() == activeId);
        panel.getChannelStrip(0)->onStripSelected();
        // Actual strip background mouse path selects only the independent bus.
        auto* selected = panel.getChannelStrip(19);
        const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(), {5, 5},
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
            selected, selected, juce::Time::getCurrentTime(), {5, 5}, juce::Time::getCurrentTime(), 1, false);
        selected->mouseDown(click);
        CHECK(panel.getSelectedMixerChannelId() == selected->getChannelId() && selected->isSelected());
        CHECK(project.getActiveChannelId() == activeId);

        // Simulate the pop-out's external content parent without creating a native
        // window. Structural rebuilds must not apply the hidden panel's bounds.
        juce::Component externalOwner;
        externalOwner.addAndMakeVisible(content);
        const juce::Rectangle<int> externalBounds(0, 0, 760, 480);
        content->setBounds(externalBounds);
        const auto checkExternalLayout = [&] {
            CHECK(content->getParentComponent() == &externalOwner);
            CHECK(content->getBounds() == externalBounds);
            CHECK(viewport->getParentComponent() == content);
            CHECK(viewport->getBounds() == externalBounds.withTrimmedTop(28));
            CHECK(content->getLocalBounds().contains(panel.getAddChannelButton().getBounds()));
            CHECK(panel.getRemoveChannelButton().getBottom() <= viewport->getY());
            CHECK(panel.getMasterStrip()->getHeight() ==
                  viewport->getHeight() - viewport->getScrollBarThickness());
            CHECK(viewport->getViewedComponent()->getWidth() == (panel.getNumChannels() + 1) * 97);
        };
        auto* external = channels.addMixerChannel("External addition"); CHECK(external);
        checkExternalLayout();
        channels.removeMixerChannel(external->getId());
        checkExternalLayout();
        channels.moveChannel(0, 1);
        checkExternalLayout();
        panel.setBounds(0, 0, 220, 140);
        checkExternalLayout();

        // Exercise the real dock-return hook: reparent first, then use dock bounds.
        panel.onDisplayModeChanged(DisplayMode::Flex, DisplayMode::PopOut);
        CHECK(content->getParentComponent() == &panel);
        CHECK(content->getBounds() == juce::Rectangle<int>(4, panel.getTitleBarHeight(),
              panel.getWidth() - 8, panel.getHeight() - panel.getTitleBarHeight() - 4));
        panel.setSize(300, 240);
        CHECK(viewport->getWidth() == 292 && viewport->getHeight() == 236 - panel.getTitleBarHeight() - 28);
        while (channels.getNumMixerChannels() < ChannelList::maxMixerChannels) CHECK(channels.addMixerChannel());
        CHECK(!panel.getAddChannelButton().isEnabled());
        lateRename = panel.getChannelStrip(0)->onRenameRequested;
        lateRename(); CHECK(prompts == 2);
    }
    lateRename(); renameReply("After teardown"); CHECK(prompts == 2);
    channels.getMixerChannels().front()->setName("after teardown");
    channels.clearMixerChannels();
    channels.getChannel(0)->setName("after teardown");
    channels.clearChannels();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
}

static void loopTimestampTests() {
    for (double rate : {44100.0, 48000.0}) for (double tempo : {120.0, 137.0})
    for (int size : {1, 7, 127, 512, 4096}) for (double length : {1.0 / 64, 16.0})
    for (double start : {3.125, 2165.993971306134}) {
        if (size == 1 && length == 16) continue;
        Probe probe;
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        auto* channel = channels.addChannel("Loop");
        channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
        auto source = std::make_unique<MidiClip>(0, start + 40);
        source->addNote(Note(60, start, 32));
        source->addNote(Note(61, start + length, 1)); // Exclusive loop end never attacks.
        const auto id = clips.addClip(std::move(source));
        tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, start + 40));
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.prepareToPlay(rate, size); mixer.setActiveChannel(0);
        state.setTempo(tempo); state.setLoopRegion(start, start + length);
        state.setLoopEnabled(true); state.setPlaying(true);
        juce::AudioBuffer<float> storage(2, size);
        juce::MidiBuffer midi; midi.ensureSize(32768);
        midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0);
        const double period = length * rate * 60 / tempo;
        const int total = static_cast<int>(std::ceil(period * 4.5));
        int blocks = 0;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
            juce::AudioBuffer<float> block(pointers, 2, n);
            rendering = true; mixer.processBlock(block, midi); rendering = false;
            CHECK(probe.lastInputCount <= 2048);
            CHECK(!probe.invalidInputOffset);
            time += n; ++blocks;
        }
        CHECK(mixer.getOverflowCount() == 0 && probe.blocks == blocks);
        CHECK(probe.sounding == 2 && probe.held[72] == 1);
        std::vector<Probe::Event> notes;
        for (const auto& event : probe.events) if (event.pitch != 72) notes.push_back(event);
        CHECK(notes.size() == 9);
        for (size_t i = 0; i < notes.size(); ++i) {
            const auto pass = (i + 1) / 2;
            const auto expected = static_cast<long long>(std::floor(pass * period + 1e-7));
            CHECK(notes[i].time == expected);
            CHECK(notes[i].on == (i % 2 == 0) && notes[i].pitch == 60);
            CHECK(notes[i].offset >= 0 && notes[i].offset < size);
        }
        state.pollRenderPosition();
        const double expected = start + std::fmod(total * tempo / 60.0 / rate, length);
        CHECK(std::abs(state.getPositionInBeats() - expected) < 1e-10);
        state.stop();
        rendering = true; mixer.processBlock(storage, midi); rendering = false;
        CHECK(probe.sounding == 0);
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    }
}

static void deferredWrapOffsetTests() {
    const double reportedEnd = 0.021333333332;
    const double exactEnd = 512.0 / 24000;
    for (double end : {reportedEnd, std::nextafter(reportedEnd, 0.0),
            std::nextafter(reportedEnd, 1.0), exactEnd,
            std::nextafter(exactEnd, 0.0), std::nextafter(exactEnd, 1.0)})
    for (double tempo : {20.0, 120.0, 300.0}) {
        Probe probe;
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        auto* channel = channels.addChannel("Deferred wrap");
        channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
        auto source = std::make_unique<MidiClip>(0, 1);
        source->addNote(Note(60, 0, 1));
        const double eventBeat = 100.0 / 144000;
        source->addNote(Note(62, eventBeat, 2.0 / 144000));
        const auto id = clips.addClip(std::move(source));
        tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, 1));
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.prepareToPlay(48000, 512);
        state.setLoopRegion(0, end); state.setLoopEnabled(true);
        state.setMetronomeEnabled(true); state.setPlaying(true);
        juce::AudioBuffer<float> buffer(2, 512); juce::MidiBuffer midi; midi.ensureSize(32768);
        const auto render = [&] {
            rendering = true; mixer.processBlock(buffer, midi); rendering = false;
            CHECK(!probe.invalidInputOffset); // All plugin inputs, including controllers/cleanup.
            CHECK(renderAllocations == 0 && renderDeletions == 0);
        };
        render();
        CHECK(probe.held[60] == 1 && buffer.getSample(0, 0) == 0.5f);
        const auto firstCount = probe.events.size();
        state.setTempo(tempo); render();
        CHECK(probe.blocks == 2 && probe.events.size() >= firstCount + 2);
        const auto& off = probe.events[firstCount];
        const auto& on = probe.events[firstCount + 1];
        CHECK(!off.on && on.on && off.pitch == 60 && on.pitch == 60);
        CHECK(off.time == 512 && on.time == 512 && off.offset == 0 && on.offset == 0);
        CHECK(buffer.getSample(0, 0) == 0.5f); // Click retrigger aligns with wrap off/on.
        for (const auto& event : probe.events) CHECK(event.offset >= 0 && event.offset < 512);
        if (end == reportedEnd && tempo == 20) {
            // Keep the negative fractional span origin: clamping the origin itself
            // would incorrectly move this later attack from sample 99 to sample 100.
            CHECK(probe.events[firstCount + 2].pitch == 62 && probe.events[firstCount + 2].on);
            CHECK(probe.events[firstCount + 2].offset == 99);
        }
        state.stop(); render(); CHECK(probe.sounding == 0 && mixer.getOverflowCount() == 0);
    }
}

static void loopControlCapacityTests() {
    TransportState state;
    for (const auto bounds : {std::pair<double, double>{-1, 4}, {0, 0}, {1, 0},
            {0, 1e-8}, {0, 1e10}, {0, std::numeric_limits<double>::infinity()},
            {std::numeric_limits<double>::quiet_NaN(), 4}}) {
        state.setLoopRegion(bounds.first, bounds.second);
        CHECK(state.getLoopRegion().startBeats == 0 && state.getLoopRegion().endBeats == 4);
    }
    Probe probe;
    ChannelList channels; TrackList tracks; ClipPool clips;
    auto* channel = channels.addChannel("Control");
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    auto source = std::make_unique<MidiClip>(0, 16);
    source->addNote(Note(60, 0, 16)); source->addNote(Note(61, 4, 8));
    const auto id = clips.addClip(std::move(source));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, 16));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.prepareToPlay(48000, 4096); mixer.setActiveChannel(0);
    juce::AudioBuffer<float> storage(2, 4096); juce::MidiBuffer midi; midi.ensureSize(32768);
    const auto render = [&](int n = 4096) {
        float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        state.pollRenderPosition();
        CHECK(probe.lastInputCount <= 2048);
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    };
    state.setPlaying(true); render(); CHECK(probe.held[60] == 1);
    state.setLoopRegion(4, 5); state.setLoopEnabled(true); render();
    CHECK(probe.held[60] == 0 && probe.held[61] == 1 && probe.lastOffSample == 0);
    CHECK(state.getPositionInBeats() > 4 && state.getPositionInBeats() < 5);
    state.setLoopEnabled(false); render(); CHECK(probe.sounding == 0); // Disable is seek-like, no chase.
    state.setPositionInBeats(0); render(); CHECK(probe.held[60] == 1);
    state.setLoopRegion(0, 1.0 / 64); state.setLoopEnabled(true);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    render(); CHECK(probe.held[60] == 1); // Live priority survives all ordinary wraps.
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 101); render();
    CHECK(probe.held[60] == 1); // Only a later loop attack resumes, not immediate chase.
    state.stop(); render(); CHECK(probe.sounding == 0);

    // More than 128 spans rejects traversal, but keeps exact elapsed position and live audition.
    mixer.prepareToPlay(48000, 96000); storage.setSize(2, 96000);
    state.setPlaying(true); state.setPositionInBeats(0);
    const auto overflow = mixer.getOverflowCount();
    render(96000);
    CHECK(mixer.getOverflowCount() > overflow && probe.sounding == 0);
    CHECK(std::abs(state.getPositionInBeats()) < 1e-10);
    render(64); CHECK(probe.held[60] == 1); // Recovery resets consumed indices safely.

    // Dense passes exhaust normal capacity atomically, not the 1072 cleanup reserve.
    state.stop(); render(64);
    auto* clip = dynamic_cast<MidiClip*>(clips.getClip(id));
    for (int key = 1; key < 100; ++key) clip->addNote(Note(key, 0, 16));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    state.setPositionInBeats(0); state.setPlaying(true);
    const auto notesBefore = probe.notes;
    render(4096);
    CHECK(probe.notes == notesBefore && probe.sounding == 0 && probe.lastInputCount <= 1072);
}

static void metronomeTests() {
    for (double rate : {44100.0, 48000.0}) for (double tempo : {120.0, 137.0})
    for (TimeSignature meter : {TimeSignature{4, 4}, {3, 4}, {6, 8}})
    for (int size : {7, 127, 4096}) {
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.prepareToPlay(rate, size);
        state.setTempo(tempo); state.setTimeSignature(meter.numerator, meter.denominator);
        state.setMetronomeEnabled(true); state.setPlaying(true);
        channels.getMasterBus().setGain(0.5f);
        juce::AudioBuffer<float> storage(2, size); juce::MidiBuffer midi; midi.ensureSize(32768);
        const double period = rate * 60 / tempo * 4 / meter.denominator;
        const int total = static_cast<int>(std::ceil(period * (meter.numerator + 1))) + 1000;
        int tick = 0;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
            juce::AudioBuffer<float> block(pointers, 2, n);
            rendering = true; mixer.processBlock(block, midi); rendering = false;
            for (int i = 0; i < n; ++i) {
                const int sample = time + i;
                while (sample >= static_cast<int>(std::floor((tick + 1) * period + 1e-7))) ++tick;
                const int age = sample - static_cast<int>(std::floor(tick * period + 1e-7));
                const bool accent = tick % meter.numerator == 0;
                const double expected = age < std::ceil(rate * 0.02) ?
                    0.5 * (accent ? 0.25 : 0.15) * std::exp(-7.0 * age / (rate * 0.02)) *
                    std::cos(juce::MathConstants<double>::twoPi * (accent ? 1760 : 1320) * age / rate) : 0;
                CHECK(std::abs(block.getSample(0, i) - expected) < 1e-6);
                CHECK(block.getSample(0, i) == block.getSample(1, i));
            }
            time += n;
        }
        CHECK(channels.getMasterBus().meter.getLeft() > 0);
        state.setPositionInBeats(0);
        channels.getMasterBus().setMuted(true);
        rendering = true; mixer.processBlock(storage, midi); rendering = false;
        CHECK(storage.getMagnitude(0, size) == 0);
        channels.getMasterBus().setMuted(false);
        state.setMetronomeEnabled(false);
        rendering = true; mixer.processBlock(storage, midi); rendering = false;
        CHECK(storage.getMagnitude(0, size) == 0);
        state.setMetronomeEnabled(true); state.stop();
        rendering = true; mixer.processBlock(storage, midi); rendering = false;
        CHECK(storage.getMagnitude(0, size) == 0);
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    }
}

static void clickMasterAndOverflowTests() {
    ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
    auto* silent = channels.addChannel("Muted solo"); silent->setSolo(true); silent->setMuted(true);
    auto* bus = channels.getMixerChannels().front().get(); bus->setSolo(true); bus->setMuted(true);
    ChannelMixer mixer(channels, tracks, clips, state); mixer.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> storage(2, 512); juce::MidiBuffer midi; midi.ensureSize(32768);
    const auto render = [&](int n) {
        float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
    };
    state.setMetronomeEnabled(true); state.setPlaying(true); channels.getMasterBus().setGain(0.5f);
    render(1);
    CHECK(storage.getSample(0, 0) == 0.125f && channels.getMasterBus().meter.getLeft() == 0.125f);
    CHECK(silent->getMeter().getLeft() == 0 && bus->getMeter().getLeft() == 0); // Click bypasses both solo domains.
    channels.getMasterBus().setMuted(true); render(1); CHECK(storage.getSample(0, 0) == 0);
    channels.getMasterBus().setMuted(false); render(1);
    const double expected = 0.125 * std::exp(-14.0 / 960) * std::cos(juce::MathConstants<double>::twoPi * 1760 * 2 / 48000);
    CHECK(std::abs(storage.getSample(0, 0) - expected) < 1e-6); // Master gate doesn't restart click phase.
    mixer.prepareToPlay(1, 512); state.setTempo(300); state.setTimeSignature(4, 16); state.setPositionInBeats(0);
    const auto before = mixer.getOverflowCount(); render(512);
    CHECK(mixer.getOverflowCount() == before + 1 && storage.getMagnitude(0, 512) == 0);
    state.pollRenderPosition(); CHECK(state.getPositionInBeats() == 2560);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void sharedControlPaintTests(DawLookAndFeel& lookAndFeel) {
    juce::TextButton button("Apply");
    juce::ComboBox combo;
    juce::TextEditor editor;
    CHECK(&button.getLookAndFeel() == &lookAndFeel);
    CHECK(&combo.getLookAndFeel() == &lookAndFeel);
    CHECK(&editor.getLookAndFeel() == &lookAndFeel);
    CHECK(button.findColour(juce::TextButton::textColourOffId) == theme::textDefault);
    CHECK(button.findColour(juce::TextButton::textColourOnId) == theme::accent);
    CHECK(combo.findColour(juce::ComboBox::textColourId) == theme::textBright);
    CHECK(editor.findColour(juce::TextEditor::textColourId) == theme::textBright);
    CHECK(editor.findColour(juce::TextEditor::highlightedTextColourId) == theme::textBright);
    CHECK(lookAndFeel.findColour(juce::PopupMenu::textColourId) == theme::textBright);
    CHECK(lookAndFeel.findColour(juce::PopupMenu::highlightedTextColourId) == theme::accent);

    const auto luminance = [](juce::Colour c) {
        const auto linear = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * linear(c.getFloatRed()) + 0.7152 * linear(c.getFloatGreen()) + 0.0722 * linear(c.getFloatBlue());
    };
    // Small enabled labels must stay readable on both neutral and selected surfaces.
    for (const auto colours : {std::pair{theme::textDefault, theme::control},
                               std::pair{theme::textSecondary, theme::control},
                               std::pair{theme::textBright, theme::deepWell},
                               std::pair{theme::textBright, theme::highlightBackground},
                               std::pair{theme::textBright, theme::raised},
                               std::pair{theme::accent, theme::selectedSurface},
                               std::pair{theme::accent, theme::actionGreen}})
        CHECK((luminance(colours.first) + 0.05) / (luminance(colours.second) + 0.05) >= 4.5);

    const auto paint = [](auto draw) {
        juce::Image image(juce::Image::ARGB, 120, 32, true);
        juce::Graphics g(image);
        draw(g);
        return image;
    };
    button.setSize(120, 32);
    const auto paintButton = [&](bool over, bool down) {
        return paint([&](juce::Graphics& g) {
            lookAndFeel.drawButtonBackground(g, button,
                button.findColour(button.getToggleState() ? juce::TextButton::buttonOnColourId
                                                          : juce::TextButton::buttonColourId), over, down);
        });
    };
    const auto normal = paintButton(false, false);
    const auto hover = paintButton(true, false);
    const auto pressed = paintButton(true, true);
    CHECK(normal.getPixelAt(0, 0).isTransparent()); // Rounded corner, not a rectangular fill.
    CHECK(normal.getPixelAt(60, 16).isOpaque());
    CHECK(normal.getPixelAt(60, 6).getBrightness() > normal.getPixelAt(60, 26).getBrightness());
    CHECK(hover.getPixelAt(60, 16).getBrightness() > normal.getPixelAt(60, 16).getBrightness());
    CHECK(pressed.getPixelAt(60, 16).getBrightness() < normal.getPixelAt(60, 16).getBrightness());
    button.setToggleState(true, juce::dontSendNotification);
    CHECK(paintButton(false, false).getPixelAt(60, 16) != normal.getPixelAt(60, 16));
    button.setToggleState(false, juce::dontSendNotification);
    button.setEnabled(false);
    CHECK(paintButton(false, false).getPixelAt(60, 16).getBrightness() < normal.getPixelAt(60, 16).getBrightness());

    combo.setSize(120, 32);
    const auto paintCombo = [&](bool down) {
        return paint([&](juce::Graphics& g) { lookAndFeel.drawComboBox(g, 120, 32, down, 96, 0, 24, 32, combo); });
    };
    const auto closedCombo = paintCombo(false);
    CHECK(closedCombo.getPixelAt(0, 0).isTransparent());
    CHECK(closedCombo.getPixelAt(60, 16).isOpaque());
    CHECK(paintCombo(true).getPixelAt(60, 16).getBrightness() < closedCombo.getPixelAt(60, 16).getBrightness());
    const auto editorBackground = paint([&](juce::Graphics& g) {
        lookAndFeel.fillTextEditorBackground(g, 120, 32, editor);
        lookAndFeel.drawTextEditorOutline(g, 120, 32, editor);
    });
    CHECK(editorBackground.getPixelAt(0, 0).isTransparent());
    CHECK(editorBackground.getPixelAt(60, 16) == theme::deepWell);

    juce::ScrollBar bar(false);
    const auto paintThumb = [&](bool over, bool down, int size) {
        return paint([&](juce::Graphics& g) { lookAndFeel.drawScrollbar(g, bar, 0, 0, 120, 10, false, 20, size, over, down); });
    };
    CHECK(paintThumb(false, false, 40).getPixelAt(40, 5) == theme::borderStrong);
    CHECK(paintThumb(true, false, 40).getPixelAt(40, 5) == theme::tickMark);
    CHECK(paintThumb(true, true, 40).getPixelAt(40, 5) == theme::textSecondary);
    CHECK(paintThumb(false, false, 0).getPixelAt(40, 5) == theme::deepWell);
}

static void windowChromeTests() {
    struct TestWindow : DawWindow {
        TestWindow() : DawWindow("Untitled * - VibeDAW", allButtons, false) {}
        void closeButtonPressed() override { ++closes; }
        void minimiseButtonPressed() override { ++minimises; }
        void maximiseButtonPressed() override { ++maximises; }
        int closes = 0, minimises = 0, maximises = 0;
    } window; // No desktop peer, native window, or application launch.
    juce::Component content;
    window.setContentNonOwned(&content, false);
    for (int width : {600, 1000, 1560}) {
        window.setSize(width, 400);
        CHECK(!window.isUsingNativeTitleBar() && window.getPeer() == nullptr);
        CHECK(window.getBorderThickness() == juce::BorderSize<int>(4));
        CHECK(content.getBounds() == juce::Rectangle<int>(4, 36, width - 8, 360));
        auto* minimise = window.getMinimiseButton();
        auto* maximise = window.getMaximiseButton();
        auto* close = window.getCloseButton();
        CHECK(minimise && maximise && close);
        for (auto* button : {minimise, maximise, close}) {
            CHECK(window.getTitleBarArea().contains(button->getBounds()));
            CHECK(button->getWidth() == 36 && button->getHeight() == 28);
            CHECK(!button->getWantsKeyboardFocus());
        }
        CHECK(minimise->getRight() <= maximise->getX() && maximise->getRight() <= close->getX());
    }

    const auto paintButton = [](juce::Button& button, juce::Button::ButtonState state) {
        button.setState(state);
        juce::Image image(juce::Image::ARGB, button.getWidth(), button.getHeight(), true);
        juce::Graphics g(image);
        static_cast<juce::Component&>(button).paint(g);
        return image;
    };
    for (auto* button : {window.getMinimiseButton(), window.getMaximiseButton(), window.getCloseButton()}) {
        button->setEnabled(true); // Peerless windows are inactive by default.
        const auto idle = paintButton(*button, juce::Button::buttonNormal);
        const auto hover = paintButton(*button, juce::Button::buttonOver);
        const auto down = paintButton(*button, juce::Button::buttonDown);
        CHECK(idle.getPixelAt(8, 8).isTransparent());
        CHECK(hover.getPixelAt(8, 8).isOpaque());
        CHECK(down.getPixelAt(8, 8).getBrightness() < hover.getPixelAt(8, 8).getBrightness());
        CHECK(hover.getPixelAt(0, 0).isTransparent());
        int glyphPixels = 0;
        for (int y = 0; y < idle.getHeight(); ++y)
            for (int x = 0; x < idle.getWidth(); ++x)
                if (idle.getPixelAt(x, y).getAlpha() > 0) ++glyphPixels;
        CHECK(glyphPixels > 8);
        button->setState(juce::Button::buttonNormal);
        button->triggerClick();
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    CHECK(window.closes == 1 && window.minimises == 1 && window.maximises == 1);

    auto* maximise = window.getMaximiseButton();
    const auto normal = paintButton(*maximise, juce::Button::buttonNormal);
    maximise->setToggleState(true, juce::dontSendNotification);
    const auto restore = paintButton(*maximise, juce::Button::buttonNormal);
    int changedPixels = 0;
    for (int y = 0; y < normal.getHeight(); ++y)
        for (int x = 0; x < normal.getWidth(); ++x)
            if (normal.getPixelAt(x, y) != restore.getPixelAt(x, y)) ++changedPixels;
    CHECK(changedPixels > 10);

    juce::Image chrome(juce::Image::RGB, window.getWidth(), 40, true);
    juce::Graphics g(chrome);
    window.paint(g);
    CHECK(chrome.getPixelAt(0, 0) == theme::border); // Inactive hairline; resize gutter stays navy.
    CHECK(chrome.getPixelAt(2, 2) == theme::windowBackground);
    CHECK(chrome.getPixelAt(300, 35) == theme::hairline);
    CHECK(chrome.getPixelAt(300, 8).getBrightness() > chrome.getPixelAt(300, 30).getBrightness());
    window.clearContentComponent();
}

static void nativeMaximizeTests() {
#if JUCE_LINUX
    struct TestWindow : DawWindow {
        TestWindow() : DawWindow("WM test", allButtons, false) {}
        void closeButtonPressed() override {}
        std::optional<MaximizedState> state = MaximizedState{};
        std::vector<bool> requests;
        bool accept = true;
        void observe(bool horizontal, bool vertical) { state = MaximizedState{horizontal, vertical}; }
        std::optional<MaximizedState> readNativeMaximizedState() override { return state; }
        bool requestNativeMaximizedState(bool maximized) override {
            requests.push_back(maximized);
            return accept;
        }
    } window;
    window.setBounds(30, 40, 800, 600);
    const auto originalBounds = window.getBounds();
    CHECK((window.getDesktopWindowStyleFlags() & juce::ComponentPeer::windowIsResizable) != 0);
    juce::ResizableBorderComponent* border = nullptr;
    for (auto* child : window.getChildren())
        if (auto* candidate = dynamic_cast<juce::ResizableBorderComponent*>(child))
            if (candidate->isVisible()) border = candidate;
    CHECK(border && border->isVisible());

    window.maximiseButtonPressed();
    CHECK(window.requests == std::vector<bool>{true});
    CHECK(window.getMaximiseButton()->getToggleState() && !border->isVisible());
    CHECK(window.getBounds() == originalBounds && !window.isFullScreen()); // Geometry belongs to the WM.
    window.maximiseButtonPressed(); // Queue restore before KDE acknowledges maximize.
    CHECK(window.requests.size() == 1 && !window.getMaximiseButton()->getToggleState());
    window.observe(true, true);
    DawWindowTestAccess::poll(window);
    CHECK((window.requests == std::vector<bool>{true, false}));
    window.observe(true, false); // Partial restore is not acknowledgement of removing both axes.
    DawWindowTestAccess::poll(window);
    CHECK(!border->isVisible() && window.requests.size() == 2);
    window.observe(false, false);
    DawWindowTestAccess::poll(window);
    CHECK(border->isVisible() && !window.getMaximiseButton()->getToggleState());

    window.observe(true, true); // External KDE shortcut/taskbar action.
    DawWindowTestAccess::poll(window);
    window.resized();
    CHECK(window.getMaximiseButton()->getToggleState() && !border->isVisible());
    window.lookAndFeelChanged();
    CHECK(window.getMaximiseButton()->getToggleState());
    window.observe(false, false);
    DawWindowTestAccess::poll(window);
    CHECK(!window.getMaximiseButton()->getToggleState() && border->isVisible());

    window.observe(true, false);
    DawWindowTestAccess::poll(window);
    window.maximiseButtonPressed(); // Partial maximize becomes fully maximized, never flips axes.
    CHECK(window.requests.back());
    DawWindowTestAccess::expire(window); // WM refuses: stop waiting/retrying.
    CHECK(!window.getMaximiseButton()->getToggleState());
    const auto count = window.requests.size();
    DawWindowTestAccess::poll(window);
    CHECK(window.requests.size() == count);

    window.observe(false, false);
    window.accept = false;
    window.maximiseButtonPressed();
    CHECK(!window.getMaximiseButton()->getToggleState() && border->isVisible());
    window.accept = true;
    window.maximiseButtonPressed();
    window.state.reset(); // Failed/unknown query is not a restored state.
    DawWindowTestAccess::poll(window);
    CHECK(window.getMaximiseButton()->getToggleState() && !border->isVisible());
    DawWindowTestAccess::expire(window);
    CHECK(!window.getMaximiseButton()->getToggleState() && border->isVisible());
    const auto beforeUnknown = window.requests.size();
    window.maximiseButtonPressed();
    CHECK(window.requests.size() == beforeUnknown && window.getBounds() == originalBounds);
    window.observe(true, true); // Late acknowledgement is still observed after timeout.
    DawWindowTestAccess::poll(window);
    CHECK(window.getMaximiseButton()->getToggleState());

    window.observe(false, false);
    DawWindowTestAccess::poll(window);
    window.requests.clear();
    window.maximiseButtonPressed();
    window.maximiseButtonPressed();
    DawWindowTestAccess::expire(window); // A queued restore must survive a delayed maximize.
    CHECK((window.requests == std::vector<bool>{true, false}));
    window.observe(true, true);
    DawWindowTestAccess::poll(window);
    CHECK(!window.getMaximiseButton()->getToggleState());
    window.observe(false, false);
    DawWindowTestAccess::poll(window);
    CHECK(border->isVisible() && !window.getMaximiseButton()->getToggleState());

    const auto mouse = [&](juce::Point<float> point) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
            juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, border, border,
            juce::Time::getCurrentTime(), {799, 599}, juce::Time::getCurrentTime(), 1, true);
    };
    auto& resizeTarget = static_cast<juce::Component&>(*border);
    resizeTarget.mouseDown(mouse({799, 599}));
    window.observe(true, true); // External maximize while the resize border has mouse capture.
    DawWindowTestAccess::poll(window);
    resizeTarget.mouseDrag(mouse({899, 699}));
    CHECK(window.getBounds() == originalBounds);
    window.observe(false, false);
    DawWindowTestAccess::poll(window);
    resizeTarget.mouseDrag(mouse({899, 699})); // An old drag must not resume after restore either.
    CHECK(window.getBounds() == originalBounds);
    resizeTarget.mouseUp(mouse({899, 699}));
#endif
}

static void editorWindowChromeTests() {
    MidiClip clip(0, 4);
    auto piano = std::make_unique<ClipEditorWindow>(&clip, 0, nullptr, nullptr,
                                                   std::function<bool(double, double&)>(), false);
    CHECK(piano->getPeer() == nullptr && !piano->isUsingNativeTitleBar());
    CHECK(piano->getTitleBarHeight() == 32 && piano->getMaximiseButton());
    auto* content = piano->getContentComponent();
    CHECK(content->getWidth() == 800 && content->getHeight() == 532);
    auto* toolbar = content->getChildComponent(0);
    auto* editor = dynamic_cast<PianoRollEditor*>(content->getChildComponent(1));
    CHECK(toolbar && editor);
    for (const auto size : {juce::Point<int>(500, 300), juce::Point<int>(1200, 800)}) {
        piano->setSize(size.x, size.y);
        CHECK(toolbar->getBounds() == content->getLocalBounds().withHeight(32));
        CHECK(editor->getBounds() == content->getLocalBounds().withTrimmedTop(32));
        for (auto* control : toolbar->getChildren()) CHECK(toolbar->getLocalBounds().contains(control->getBounds()));
    }
    piano.reset();

    Probe probe;
    auto processor = std::make_unique<EditorInstrument>(probe);
    auto* instrument = processor.get();
    PluginHost host(std::move(processor));
    std::unique_ptr<PluginWindow> plugin;
    {
        AudioQuiescence::Edit edit;
        plugin = PluginWindowTestAccess::create(&host);
    }
    CHECK(plugin->getPeer() == nullptr && !plugin->isUsingNativeTitleBar());
    CHECK(plugin->getTitleBarHeight() == 32 && plugin->getCloseButton());
    CHECK(plugin->getMaximiseButton() == nullptr); // Don't impose maximize on third-party editors.
    CHECK(plugin->getContentComponent()->getBounds() == juce::Rectangle<int>(4, 36, 100, 100));
    plugin->getContentComponent()->setSize(240, 160); // Editor-originated resize retains its requested size.
    CHECK(plugin->getWidth() == 248 && plugin->getHeight() == 200);
    plugin.reset();
    CHECK(instrument->editorDestroyedSafely && instrument->getActiveEditor() == nullptr);
    auto fallback = PluginWindowTestAccess::create(nullptr);
    CHECK(fallback->getContentComponent()->getBounds() == juce::Rectangle<int>(4, 36, 420, 80));
}

static void keyboardPaintTests() {
    const auto paint = [](juce::Component& component) {
        juce::Image image(juce::Image::RGB, component.getWidth(), component.getHeight(), true);
        juce::Graphics g(image);
        component.paint(g);
        return image;
    };
    juce::MidiKeyboardState state;
    PianoComponent piano(state, nullptr);
    piano.setAvailableRange(60, 71);
    piano.setScrollButtonsVisible(false);
    piano.setLowestVisibleKey(60);
    piano.setSize(140, 100);
    const auto white = piano.getRectangleForKey(62);
    const auto black = piano.getRectangleForKey(63);
    const auto whitePoint = juce::Point<int>(juce::roundToInt(white.getCentreX()), 80);
    const auto blackPoint = black.getCentre().toInt();
    const auto pixel = [](const juce::Image& image, juce::Point<int> p) { return image.getPixelAt(p.x, p.y); };
    CHECK(piano.getLocalBounds().contains(whitePoint) && piano.getLocalBounds().contains(blackPoint));
    const auto idle = paint(piano);
    CHECK(pixel(idle, whitePoint).getBrightness() > pixel(idle, blackPoint).getBrightness());
    CHECK(idle.getPixelAt(blackPoint.x, 10).getBrightness() > idle.getPixelAt(blackPoint.x, 40).getBrightness());
    state.noteOn(1, 62, 0.8f);
    const auto whiteHeld = paint(piano);
    CHECK(pixel(whiteHeld, whitePoint) != pixel(idle, whitePoint));
    CHECK(pixel(whiteHeld, whitePoint).getGreen() > pixel(whiteHeld, whitePoint).getRed());
    CHECK(pixel(whiteHeld, blackPoint) == pixel(idle, blackPoint));
    state.noteOn(1, 63, 0.8f);
    const auto bothHeld = paint(piano);
    CHECK(pixel(bothHeld, blackPoint) != pixel(idle, blackPoint));
    CHECK(pixel(bothHeld, blackPoint).getGreen() > pixel(bothHeld, blackPoint).getRed());
    CHECK(pixel(bothHeld, whitePoint) == pixel(whiteHeld, whitePoint));
    state.noteOff(1, 62, 0.0f);
    state.noteOff(1, 63, 0.0f);
    const auto released = paint(piano);
    CHECK(pixel(released, whitePoint) == pixel(idle, whitePoint));
    CHECK(pixel(released, blackPoint) == pixel(idle, blackPoint));

    PianoRollKeyboard roll;
    roll.setLowestNote(60); roll.setNumKeys(12); roll.setKeyHeight(16); roll.setSize(60, 192);
    const auto rollIdle = paint(roll);
    for (int pitch : {62, 63}) roll.setHeldNote(pitch, true);
    const auto rollHeld = paint(roll);
    roll.clearHeldNotes();
    const auto rollReleased = paint(roll);
    for (int pitch : {62, 63, 64}) {
        const auto point = juce::Point<int>(30, roll.getYForKey(pitch) + 8);
        CHECK(pixel(rollReleased, point) == pixel(rollIdle, point));
        if (pitch == 64) CHECK(pixel(rollHeld, point) == pixel(rollIdle, point));
        else {
            CHECK(pixel(rollHeld, point) != pixel(rollIdle, point));
            CHECK(rollHeld.getPixelAt(58, point.y) == theme::accent.brighter(0.3f));
        }
    }
}

static void iconTests() {
    for (int i = 0; i <= static_cast<int>(IconId::clips); ++i) {
        const auto id = static_cast<IconId>(i);
        const auto& path = Icons::path(id);
        CHECK(path.getBounds().getWidth() > 0 && path.getBounds().getHeight() > 0);
        juce::Image image(juce::Image::ARGB, 36, 28, true);
        {
            juce::Graphics graphics(image);
            Icons::draw(graphics, id, theme::accent, {0.0f, 0.0f, 36.0f, 28.0f});
        }
        int lit = 0;
        for (int y = 0; y < 28; ++y)
            for (int x = 0; x < 36; ++x)
                if (image.getPixelAt(x, y).getAlpha() > 0) ++lit;
        CHECK(lit > 40); // Visible glyph, neither empty nor clipped to nothing.
    }
}

static void loopUiTests() {
    TransportState state;
    TransportComponent ui(state); ui.setSize(1000, 64);
    auto* loop = dynamic_cast<TransportButton*>(ui.findChildWithID("loopToggle"));
    auto* click = dynamic_cast<TransportButton*>(ui.findChildWithID("metronomeToggle"));
    CHECK(loop && click);
    CHECK(!ui.findChildWithID("record")->isEnabled());
    CHECK(!ui.findChildWithID("loopStart")); // Numeric editing moved to the popover.
    loop->onClick(); click->onClick();
    CHECK(state.isLoopEnabled() && state.isMetronomeEnabled() && loop->isActive() && click->isActive());
    // Right-click dispatches the context menu request without toggling.
    bool menuRequested = false;
    loop->onContextMenu = [&](const juce::MouseEvent&) { menuRequested = true; };
    auto buttonEvent = [&](juce::Point<float> point, juce::Point<float> down, int modifiers) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point, modifiers,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, loop, loop, juce::Time::getCurrentTime(),
            down, juce::Time::getCurrentTime(), 1, point != down);
    };
    const bool enabledBefore = state.isLoopEnabled();
    loop->mouseDown(buttonEvent({2, 2}, {2, 2},
        juce::ModifierKeys::rightButtonModifier | juce::ModifierKeys::ctrlModifier));
    CHECK(menuRequested);
    loop->mouseUp(buttonEvent({2, 2}, {2, 2}, 0));
    CHECK(state.isLoopEnabled() == enabledBefore);
    // Menu actions mirror the bar controls they replaced.
    ui.handleLoopMenuAction(1);
    CHECK(!state.isLoopEnabled() && !loop->isActive());
    ui.handleLoopMenuAction(1);
    CHECK(state.isLoopEnabled());
    ui.handleLoopMenuAction(3); // Clear: gone from the model and the ruler.
    CHECK(!state.isLoopEnabled() && !state.isLoopRegionSet() &&
          state.getLoopRegion().startBeats == 0 && state.getLoopRegion().endBeats == 4.0);
    loop->onClick(); // No region: the button materializes the default loop, enabled.
    CHECK(state.isLoopEnabled() && state.isLoopRegionSet() &&
          state.getLoopRegion().startBeats == 0 && state.getLoopRegion().endBeats == 4.0);
    bool editOpened = false;
    ui.openLoopEditorOverride = [&] { editOpened = true; };
    ui.handleLoopMenuAction(2);
    CHECK(editOpened);
    {
        // The popover is the relocated numeric editor; Apply enables looping.
        auto popover = ui.createLoopEditor();
        popover->setSize(240, 60);
        auto* start = dynamic_cast<juce::TextEditor*>(popover->findChildWithID("loopStart"));
        auto* end = dynamic_cast<juce::TextEditor*>(popover->findChildWithID("loopEnd"));
        auto* apply = dynamic_cast<juce::TextButton*>(popover->findChildWithID("applyLoop"));
        auto* validation = dynamic_cast<juce::Label*>(popover->findChildWithID("loopValidation"));
        CHECK(start && end && apply && validation);
        CHECK(popover->getLocalBounds().contains(start->getBounds()) &&
              popover->getLocalBounds().contains(end->getBounds()));
        for (const auto invalid : {"", "abc", "1foo", "nan", "inf", "-1"}) {
            start->setText(invalid); apply->onClick();
            CHECK(state.getLoopRegion().startBeats == 0 && validation->getText().startsWith("Invalid"));
        }
        start->setText("2.5"); end->setText("6.5"); start->onReturnKey();
        CHECK(state.getLoopRegion().startBeats == 2.5 && state.getLoopRegion().endBeats == 6.5);
        CHECK(state.isLoopEnabled()); // Commit enables looping.
        CHECK(validation->getText().startsWith("Quarter notes"));
        state.setLoopRegion(1, 3); // External updates refresh the fields.
        CHECK(start->getText().getDoubleValue() == 1 && end->getText().getDoubleValue() == 3);
    }
    CHECK(ui.getLocalBounds().getWidth() > 0);
    ui.setSize(600, 64);
    for (auto* child : ui.getChildren()) if (child->isVisible()) CHECK(ui.getLocalBounds().contains(child->getBounds()));
    Project project;
    TimelinePanel timeline(project); timeline.setSize(800, 400);
    TimelineContent* content = nullptr;
    juce::ScrollBar* horizontal = nullptr;
    for (auto* child : timeline.getChildren()) {
        if (auto* value = dynamic_cast<TimelineContent*>(child)) content = value;
        if (auto* value = dynamic_cast<juce::ScrollBar*>(child)) if (!value->isVertical()) horizontal = value;
    }
    CHECK(content && horizontal);
    project.getTransportState().setLoopRegion(100, 104);
    CHECK(content->getTotalWidth() == 108 * content->getPixelsPerBeat());
    CHECK(horizontal->getMaximumRangeLimit() == content->getTotalWidth());
    TimeRuler* ruler = nullptr;
    for (auto* child : content->getChildren()) if (auto* value = dynamic_cast<TimeRuler*>(child)) ruler = value;
    CHECK(ruler);
    project.getTransportState().setLoopEnabled(true);
    ruler->setSize(400, 24); ruler->setScrollOffset(100 * 50);
    juce::Image image(juce::Image::RGB, 400, 24, true);
    juce::Graphics graphics(image); ruler->paint(graphics);
    CHECK(image.getPixelAt(25, 1) == theme::loopEdgeActive);
    CHECK(image.getPixelAt(225, 1) == theme::control);
    // Ruler gestures: a click seeks; a drag previews locally and commits once.
    auto& transportState = project.getTransportState();
    auto rulerEvent = [&](juce::Point<float> point, juce::Point<float> down, int modifiers = juce::ModifierKeys::leftButtonModifier) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point, modifiers,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, ruler, ruler, juce::Time::getCurrentTime(),
            down, juce::Time::getCurrentTime(), 1, point != down);
    };
    ruler->mouseDown(rulerEvent({100, 12}, {100, 12}));
    ruler->mouseUp(rulerEvent({100, 12}, {100, 12}, 0));
    CHECK(transportState.getPositionInBeats() == 102.0); // (100 + 5000) / 50
    CHECK(!ruler->isDraggingLoop());
    transportState.setPositionInBeats(0);
    ruler->mouseDown(rulerEvent({300, 12}, {300, 12}));
    ruler->mouseDrag(rulerEvent({303, 12}, {300, 12})); // Below the threshold: still a click.
    CHECK(!ruler->isDraggingLoop());
    ruler->mouseUp(rulerEvent({303, 12}, {300, 12}, 0));
    CHECK(transportState.getPositionInBeats() == 106.0);
    CHECK(transportState.getLoopRegion().endBeats == 104.0);
    // Create on empty space: snapped preview, single commit on mouseUp, auto-enable.
    ruler->mouseDown(rulerEvent({275, 12}, {275, 12})); // beat 105.5
    ruler->mouseDrag(rulerEvent({279, 12}, {275, 12}));
    CHECK(!ruler->isDraggingLoop());
    CHECK(transportState.getLoopRegion().startBeats == 100.0);
    ruler->mouseDrag(rulerEvent({362, 12}, {275, 12})); // beat 107.24 -> snaps to 107.25
    CHECK(ruler->isDraggingLoop());
    CHECK(ruler->getLoopPreview().startBeats == 105.5 && ruler->getLoopPreview().endBeats == 107.25);
    CHECK(transportState.getLoopRegion().startBeats == 100.0); // Nothing published during drag.
    ruler->paint(graphics);
    CHECK(image.getPixelAt(300, 1) == theme::loopEdgePreview);
    CHECK(image.getPixelAt(25, 1) == theme::loopEdgeActive); // Existing region survives the preview.
    ruler->mouseUp(rulerEvent({362, 12}, {275, 12}, 0));
    CHECK(transportState.getLoopRegion().startBeats == 105.5 && transportState.getLoopRegion().endBeats == 107.25);
    CHECK(transportState.isLoopEnabled());
    ruler->setScrollOffset(100 * 50); // Committing grows the extent; the panel resyncs to the viewport (0).
    // Shift+drag inside the body moves; length preserved.
    const int shiftDrag = juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier;
    ruler->mouseDown(rulerEvent({300, 12}, {300, 12}, shiftDrag)); // beat 106, inside [105.5, 107.25)
    ruler->mouseDrag(rulerEvent({350, 12}, {300, 12}, shiftDrag)); // beat 107 -> delta 1
    CHECK(ruler->getLoopPreview().startBeats == 106.5 && ruler->getLoopPreview().endBeats == 108.25);
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::DraggingHandCursor);
    ruler->mouseUp(rulerEvent({350, 12}, {300, 12}, 0));
    CHECK(transportState.getLoopRegion().startBeats == 106.5 && transportState.getLoopRegion().endBeats == 108.25);
    ruler->setScrollOffset(100 * 50);
    // Plain drag inside the body creates a replacement region.
    ruler->mouseDown(rulerEvent({350, 12}, {350, 12})); // beat 107, interior of [106.5, 108.25)
    ruler->mouseDrag(rulerEvent({375, 12}, {350, 12})); // beat 107.5
    CHECK(ruler->getLoopPreview().startBeats == 107.0 && ruler->getLoopPreview().endBeats == 107.5);
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::CrosshairCursor);
    ruler->mouseUp(rulerEvent({375, 12}, {350, 12}, 0));
    CHECK(transportState.getLoopRegion().startBeats == 107.0 && transportState.getLoopRegion().endBeats == 107.5);
    ruler->setScrollOffset(100 * 50);
    // Cursor affordances preview the pending action under the mouse.
    ruler->mouseMove(rulerEvent({362.5f, 12}, {362.5f, 12}, 0)); // interior -> create
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::CrosshairCursor);
    ruler->mouseMove(rulerEvent({362.5f, 12}, {362.5f, 12}, juce::ModifierKeys::shiftModifier)); // shift interior -> move
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::DraggingHandCursor);
    ruler->mouseMove(rulerEvent({378, 12}, {378, 12}, 0)); // near end edge 107.5 -> resize
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::LeftRightResizeCursor);
    ruler->mouseMove(rulerEvent({353, 12}, {353, 12}, 0)); // near start edge 107 -> resize
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::LeftRightResizeCursor);
    ruler->mouseMove(rulerEvent({381, 12}, {381, 12}, 0)); // beyond the edge zone -> create
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::CrosshairCursor);
    // Shift wins over an edge grab: shift+drag near the end edge still moves.
    ruler->mouseDown(rulerEvent({374, 12}, {374, 12}, shiftDrag)); // beat 107.48, near end 107.5
    ruler->mouseDrag(rulerEvent({424, 12}, {374, 12}, shiftDrag)); // beat 108.48 -> delta 1
    CHECK(ruler->getLoopPreview().startBeats == 108.0 && ruler->getLoopPreview().endBeats == 108.5);
    ruler->mouseUp(rulerEvent({424, 12}, {374, 12}, 0));
    CHECK(transportState.getLoopRegion().startBeats == 108.0 && transportState.getLoopRegion().endBeats == 108.5);
    ruler->setScrollOffset(100 * 50);
    // Resize the end edge (grab within 0.1 beats = 5px of it).
    ruler->mouseDown(rulerEvent({428, 12}, {428, 12})); // beat 108.56, edge at 108.5
    ruler->mouseDrag(rulerEvent({450, 12}, {428, 12})); // beat 109
    CHECK(ruler->getLoopPreview().startBeats == 108.0 && ruler->getLoopPreview().endBeats == 109.0);
    ruler->mouseUp(rulerEvent({450, 12}, {428, 12}, 0));
    CHECK(transportState.getLoopRegion().startBeats == 108.0 && transportState.getLoopRegion().endBeats == 109.0);
    ruler->setScrollOffset(100 * 50);
    // Resize the start edge; clamped to the minimum length when crossing the end.
    ruler->mouseDown(rulerEvent({403, 12}, {403, 12})); // beat 108.06, within 0.1 of start 108
    ruler->mouseDrag(rulerEvent({550, 12}, {403, 12})); // beat 111 -> clamped to end - 1/16
    CHECK(ruler->getLoopPreview().startBeats == 108.9375);
    ruler->mouseUp(rulerEvent({550, 12}, {403, 12}, 0));
    CHECK(transportState.getLoopRegion().startBeats == 108.9375);
    ruler->setScrollOffset(100 * 50);
    // Alt bypasses the 1/16 snap; the commit keeps the unsnapped beat.
    const int alt = juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier;
    ruler->mouseDown(rulerEvent({300, 12}, {300, 12}, alt)); // beat 106
    ruler->mouseDrag(rulerEvent({309, 12}, {300, 12}, alt)); // beat 106.18, unsnapped
    CHECK(ruler->getLoopPreview().startBeats == 106.0 && ruler->getLoopPreview().endBeats == 106.18);
    ruler->mouseUp(rulerEvent({309, 12}, {300, 12}, 0));
    CHECK(transportState.getLoopRegion().startBeats == 106.0 && transportState.getLoopRegion().endBeats == 106.18);
    CHECK(!ruler->isDraggingLoop());
    // A set-but-disabled region paints dim and stays grabbable (it is visible);
    // committing any ruler gesture on it re-enables looping.
    transportState.setLoopRegion(105.5, 107.25); // Wider region: an interior beyond the edge zones.
    transportState.setLoopEnabled(false); // Every loop notification resyncs the ruler to the viewport offset.
    ruler->setScrollOffset(100 * 50);
    juce::Image dimmed(juce::Image::RGB, 400, 24, true);
    { juce::Graphics dimmedGraphics(dimmed); ruler->paint(dimmedGraphics); }
    CHECK(dimmed.getPixelAt(302, 1) == theme::loopEdgeIdle);
    // Use the renderer's alpha compositing, not a rounded approximation of the RGB blend.
    juce::Image idleFill(juce::Image::RGB, 1, 1, true);
    {
        juce::Graphics fillGraphics(idleFill);
        fillGraphics.fillAll(theme::control);
        fillGraphics.fillAll(theme::loopFillIdle.withAlpha(0.65f));
    }
    CHECK(dimmed.getPixelAt(305, 20) == idleFill.getPixelAt(0, 0)); // Below tick labels.
    CHECK(dimmed.getPixelAt(308, 20) == idleFill.getPixelAt(0, 0));
    ruler->mouseMove(rulerEvent({305, 12}, {305, 12}, 0)); // Plain inside the dim body: create.
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::CrosshairCursor);
    ruler->mouseMove(rulerEvent({305, 12}, {305, 12}, juce::ModifierKeys::shiftModifier)); // Shift moves a dim region too.
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::DraggingHandCursor);
    ruler->mouseMove(rulerEvent({278, 12}, {278, 12}, 0)); // Near the dim start edge 105.5: resize.
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::LeftRightResizeCursor);
    ruler->mouseDown(rulerEvent({305, 12}, {305, 12}));
    ruler->mouseDrag(rulerEvent({362, 12}, {305, 12})); // beat 107.24
    CHECK(ruler->isDraggingLoop());
    CHECK(ruler->getLoopPreview().startBeats == 106.125 && ruler->getLoopPreview().endBeats == 107.25);
    CHECK(ruler->getMouseCursor() == juce::MouseCursor::CrosshairCursor);
    ruler->mouseUp(rulerEvent({362, 12}, {305, 12}, 0));
    CHECK(transportState.isLoopEnabled()); // The created region re-enables looping.
    CHECK(!ruler->isDraggingLoop());
    // Clearing removes the loop from the model and the ruler entirely.
    transportState.clearLoop();
    CHECK(!transportState.isLoopRegionSet() && !transportState.isLoopEnabled());
    ruler->setScrollOffset(100 * 50); // clearLoop notified; resync.
    juce::Image cleared(juce::Image::RGB, 400, 24, true);
    { juce::Graphics clearedGraphics(cleared); ruler->paint(clearedGraphics); }
    CHECK(cleared.getPixelAt(25, 1) == theme::control);
    CHECK(cleared.getPixelAt(302, 1) == theme::control);
    CHECK(cleared.getPixelAt(305, 20) == theme::control);
    // A drag with no region creates one; enabling the cleared loop uses the default region.
    ruler->mouseDown(rulerEvent({305, 12}, {305, 12}));
    ruler->mouseDrag(rulerEvent({362, 12}, {305, 12}));
    CHECK(ruler->getLoopPreview().startBeats == 106.125 && ruler->getLoopPreview().endBeats == 107.25);
    ruler->mouseUp(rulerEvent({362, 12}, {305, 12}, 0));
    CHECK(transportState.isLoopEnabled() && transportState.isLoopRegionSet());
    CHECK(transportState.getLoopRegion().startBeats == 106.125 && transportState.getLoopRegion().endBeats == 107.25);
    transportState.clearLoop();
    transportState.setLoopEnabled(true); // Enabling a cleared loop materializes the default region.
    CHECK(transportState.isLoopRegionSet() && transportState.getLoopRegion().startBeats == 0 &&
          transportState.getLoopRegion().endBeats == 4.0);
    ruler->setScrollOffset(0); // The default [0, 4) band sits at the ruler origin.
    juce::Image defaulted(juce::Image::RGB, 400, 24, true);
    { juce::Graphics defaultedGraphics(defaulted); ruler->paint(defaultedGraphics); }
    CHECK(defaulted.getPixelAt(2, 1) == theme::loopEdgeActive);
    CHECK(defaulted.getPixelAt(225, 1) == theme::control); // Past the band, off the tick lines.
}

static void tempoControlTests() {
    TransportState state;
    TransportComponent ui(state); ui.setSize(1000, 64);
    auto* tempo = dynamic_cast<TempoControl*>(ui.findChildWithID("tempoControl"));
    CHECK(tempo);
    // Presets include 128; the menu model exposes Tap Tempo plus all presets.
    const auto& presets = TempoControl::presetTempos();
    CHECK(presets.size() == 13);
    const double expected[] = {70, 80, 90, 100, 110, 120, 128, 140, 150, 160, 174, 180, 200};
    for (int i = 0; i < 13; ++i) CHECK(presets[static_cast<std::size_t>(i)] == expected[i]);
    CHECK(TempoControl::buildMenu(128.0).getNumItems() == 14); // Separator not counted.
    // Right-click dispatches the menu request; nothing else happens.
    int menuRequests = 0;
    tempo->showMenuOverride = [&] { ++menuRequests; };
    std::vector<double> published;
    tempo->onTempoChanged = [&](double t) { published.push_back(t); state.setTempo(t); };
    int taps = 0;
    tempo->onTempoTapped = [&] { ++taps; };
    auto event = [&](juce::Point<float> point, juce::Point<float> down, int modifiers) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point, modifiers,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, tempo, tempo, juce::Time::getCurrentTime(),
            down, juce::Time::getCurrentTime(), 1, point != down);
    };
    const int left = juce::ModifierKeys::leftButtonModifier;
    tempo->mouseDown(event({35, 14}, {35, 14}, juce::ModifierKeys::rightButtonModifier));
    CHECK(menuRequests == 1 && !tempo->isEditing() && state.getTempo() == 120.0);
    // Menu dispatch: 1 = tap, 2..14 = presets in list order, other ids ignored.
    tempo->handleMenuAction(1);
    CHECK(taps == 1 && state.getTempo() == 120.0 && published.empty());
    for (int i = 0; i < 13; ++i) {
        tempo->handleMenuAction(2 + i);
        CHECK(state.getTempo() == expected[i]);
    }
    CHECK(published.size() == 13 && published.back() == 200.0);
    tempo->handleMenuAction(0); tempo->handleMenuAction(99);
    CHECK(published.size() == 13 && state.getTempo() == 200.0);
    // Drag scrubbing: threshold-gated, live, rounded to whole BPM.
    tempo->mouseDown(event({35, 14}, {35, 14}, left));
    tempo->mouseDrag(event({35, 12}, {35, 14}, left)); // Below the threshold.
    CHECK(!tempo->isScrubbing() && state.getTempo() == 200.0);
    tempo->mouseDrag(event({35, 4}, {35, 14}, left)); // dy = -10 -> start + 1.
    CHECK(tempo->isScrubbing() && state.getTempo() == 201.0);
    tempo->mouseDrag(event({35, 2014}, {35, 14}, left)); // dy = 2000 -> raw 0, clamped.
    CHECK(state.getTempo() == 20.0);
    tempo->mouseUp(event({35, 2014}, {35, 14}, left));
    CHECK(!tempo->isScrubbing() && !tempo->isEditing());
    // Shift scrubs in 0.1 steps instead of rounding to whole BPM.
    const int shiftLeft = left | juce::ModifierKeys::shiftModifier;
    tempo->mouseDown(event({35, 14}, {35, 14}, left));
    tempo->mouseDrag(event({35, 9}, {35, 14}, shiftLeft)); // 20 + 0.5 without rounding.
    CHECK(tempo->isScrubbing() && std::abs(state.getTempo() - 20.5) < 1e-9);
    tempo->mouseUp(event({35, 9}, {35, 14}, 0));
    // Upward clamps at 300.
    tempo->mouseDown(event({35, 500}, {35, 500}, left));
    tempo->mouseDrag(event({35, -2500}, {35, 500}, left)); // raw 320.5 -> 300.
    CHECK(state.getTempo() == 300.0);
    tempo->mouseUp(event({35, -2500}, {35, 500}, 0));
    CHECK(published.size() == 17 && published[13] == 201.0 && published[14] == 20.0 &&
          published[15] == 20.5 && published.back() == 300.0);
    // Cursor affordance: up/down resize while idle, normal while editing.
    CHECK(tempo->getMouseCursor() == juce::MouseCursor::UpDownResizeCursor);
    tempo->mouseMove(event({35, 14}, {35, 14}, 0));
    CHECK(tempo->getMouseCursor() == juce::MouseCursor::UpDownResizeCursor);
    // A press that stays under the threshold is a tap and opens the editor.
    tempo->mouseDown(event({35, 14}, {35, 14}, left));
    tempo->mouseDrag(event({36, 15}, {35, 14}, left));
    tempo->mouseUp(event({36, 15}, {35, 14}, left));
    CHECK(tempo->isEditing());
    auto* editor = dynamic_cast<juce::TextEditor*>(tempo->findChildWithID("tempoEditor"));
    CHECK(editor && editor->isVisible());
    CHECK(tempo->editingText() == "300.0");
    // Valid text commits through the live command path.
    tempo->setEditingText("128");
    CHECK(!tempo->isInvalidEntry());
    tempo->commitEdit();
    CHECK(!tempo->isEditing() && !editor->isVisible() && state.getTempo() == 128.0);
    // Invalid entries are retained, tinted, and keep the editor open.
    for (const char* bad : {"", "1.2.3", "0", "999", "-50"}) {
        tempo->beginEdit();
        tempo->setEditingText(bad);
        CHECK(tempo->isInvalidEntry());
        tempo->commitEdit();
        CHECK(tempo->isEditing() && state.getTempo() == 128.0);
    }
    tempo->cancelEdit();
    CHECK(!tempo->isEditing() && state.getTempo() == 128.0);
    // Fractional values commit exactly.
    tempo->beginEdit();
    tempo->setEditingText("137.5");
    tempo->commitEdit();
    CHECK(!tempo->isEditing() && state.getTempo() == 137.5);
    // External tempo updates never clobber text being typed; a following scrub
    // proves the internal value is still the edited one.
    tempo->beginEdit();
    state.setTempo(250.0); // transportTempoChanged -> setTempo is ignored while editing.
    CHECK(tempo->isEditing());
    tempo->cancelEdit();
    tempo->mouseDown(event({35, 14}, {35, 14}, left));
    tempo->mouseDrag(event({35, 4}, {35, 14}, left)); // 137.5 + 1 -> 138.5 rounds away to 139.
    tempo->mouseUp(event({35, 4}, {35, 14}, 0));
    CHECK(state.getTempo() == 139.0);
    // While editing, the box ignores clicks and the cursor goes back to normal.
    tempo->beginEdit();
    tempo->mouseMove(event({35, 14}, {35, 14}, 0));
    CHECK(tempo->getMouseCursor() == juce::MouseCursor::NormalCursor);
    tempo->mouseDown(event({35, 14}, {35, 14}, left));
    tempo->mouseUp(event({35, 14}, {35, 14}, left));
    CHECK(tempo->isEditing() && !tempo->isScrubbing());
    tempo->cancelEdit();
    tempo->mouseMove(event({35, 14}, {35, 14}, 0));
    CHECK(tempo->getMouseCursor() == juce::MouseCursor::UpDownResizeCursor);
}

static void loopPedalAndLedgerTests() {
    for (int size : {512, 1024}) {
        Probe a, b;
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        auto* first = channels.addChannel("Pedal"); auto* second = channels.addChannel("Independent");
        first->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(a)));
        second->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(b)));
        auto source = std::make_unique<MidiClip>(0, 4); source->addNote(Note(60, 0, 4));
        const auto id = clips.addClip(std::move(source));
        auto* track = tracks.addTrack();
        track->addClipInstance(std::make_unique<ClipInstance>(id, first->getId(), 0, 4));
        track->addClipInstance(std::make_unique<ClipInstance>(id, second->getId(), 0, 4));
        ChannelMixer mixer(channels, tracks, clips, state); mixer.prepareToPlay(48000, size); mixer.setActiveChannel(0);
        state.setLoopRegion(0, 1.0 / 32); state.setLoopEnabled(true); state.setPlaying(true);
        juce::AudioBuffer<float> buffer(2, size); juce::MidiBuffer midi; midi.ensureSize(32768);
        const auto render = [&] { rendering = true; mixer.processBlock(buffer, midi); rendering = false; };
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0);
        render();
        if (size == 512) { CHECK(a.sounding == 2); render(); }
        CHECK(first->isVoiceResetPending() && b.sounding == 1 && b.resets == 0);
        CHECK(buffer.getSample(0, 0) == 0.25f); // Only independent destination is audible.
        CHECK(a.lastInputCount <= 1072 && mixer.getOverflowCount() == 0);
        const auto calls = a.blocks; render(); CHECK(a.blocks == calls);
        ChannelTestAccess::resetVoices(*first);
        CHECK(a.resets == 1 && !a.resetOnAudio && a.resetQuiescent && a.sounding == 0);
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    }
    // A live-only pedal owner must not be reset merely because another destination wraps.
    Probe dense, live;
    ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
    auto* first = channels.addChannel("1024 notes"); auto* second = channels.addChannel("Live pedal");
    first->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(dense)));
    second->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(live)));
    auto source = std::make_unique<MidiClip>(0, 4);
    for (int key = 0; key < 1024; ++key) {
        Note note(key % 128, key < 512 ? 0 : 0.05, 3); note.setChannel(key / 128 + 1); source->addNote(note);
    }
    const auto id = clips.addClip(std::move(source));
    auto* track = tracks.addTrack();
    track->addClipInstance(std::make_unique<ClipInstance>(id, first->getId(), 0, 4));
    // Future events give the live-only destination a scheduler range, but no delivered arrangement history.
    track->addClipInstance(std::make_unique<ClipInstance>(id, second->getId(), 8, 4));
    ChannelMixer mixer(channels, tracks, clips, state); mixer.prepareToPlay(48000, 1024); mixer.setActiveChannel(1);
    state.setLoopRegion(0, 0.125); state.setLoopEnabled(true); state.setPlaying(true);
    juce::AudioBuffer<float> buffer(2, 1024); juce::MidiBuffer midi; midi.ensureSize(32768);
    const auto render = [&] { rendering = true; mixer.processBlock(buffer, midi); rendering = false; };
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0);
    render(); CHECK(dense.sounding == 512 && live.sounding == 1);
    render(); CHECK(dense.sounding == 1024);
    const auto notes = dense.notes;
    render();
    CHECK(dense.sounding == 0 && dense.notes == notes && dense.lastInputCount == 1072);
    CHECK(mixer.getOverflowCount() == 1 && live.sounding == 1 && live.noteOffs == 0 && live.resets == 0);
    CHECK(!second->isVoiceResetPending());
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void loopClickAndClockTests() {
    // Musical time remains partition-independent over one hour, including fractional wraps.
    for (int size : {127, 4096}) {
        TransportState state; TransportClock clock;
        state.setTempo(137); state.setLoopRegion(3.125, 19.125); state.setLoopEnabled(true); state.setPlaying(true);
        const int total = 44100 * 3600;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            rendering = true;
            const auto block = clock.beginBlock(state, n, 44100); clock.endBlock(state, block);
            rendering = false;
            CHECK(!block.spanOverflow && block.endBeats >= 3.125 && block.endBeats < 19.125);
            time += n;
        }
        state.pollRenderPosition();
        CHECK(std::abs(state.getPositionInBeats() - (3.125 + std::fmod(137.0 * 60, 16.0))) < 1e-10);
    }
    for (int size : {1, 7, 127, 4096}) {
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        ChannelMixer mixer(channels, tracks, clips, state); mixer.prepareToPlay(44100, size);
        state.setTempo(137); state.setTimeSignature(6, 8);
        state.setLoopRegion(0, 1.0 / 64); state.setLoopEnabled(true);
        state.setMetronomeEnabled(true); state.setPlaying(true);
        juce::AudioBuffer<float> storage(2, size); juce::MidiBuffer midi; midi.ensureSize(32768);
        const double period = 44100.0 * 60 / 137 / 64;
        int pass = 0;
        const int total = 10000;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
            juce::AudioBuffer<float> block(pointers, 2, n);
            rendering = true; mixer.processBlock(block, midi); rendering = false;
            for (int i = 0; i < n; ++i) {
                const int sample = time + i;
                while (sample >= static_cast<int>(std::floor((pass + 1) * period + 1e-7))) ++pass;
                const int age = sample - static_cast<int>(std::floor(pass * period + 1e-7));
                const double expected = 0.25 * std::exp(-7.0 * age / (44100 * 0.02)) *
                    std::cos(juce::MathConstants<double>::twoPi * 1760 * age / 44100);
                CHECK(std::abs(block.getSample(0, i) - expected) < 1e-6);
            }
            time += n;
        }
    }
    // Exact end ownership, seek/edit, tempo and denominator changes on captured boundaries.
    TransportState state; TransportClock clock; Metronome click;
    juce::AudioBuffer<float> storage(2, 24000);
    state.setLoopRegion(0, 1); state.setLoopEnabled(true); state.setMetronomeEnabled(true); state.setPlaying(true);
    const auto render = [&](int n) {
        float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n); block.clear();
        rendering = true;
        const auto timing = clock.beginBlock(state, n, 48000);
        click.render(block, timing); clock.endBlock(state, timing);
        rendering = false;
        return timing;
    };
    auto timing = render(24000);
    CHECK(timing.spanCount == 1 && !timing.spans[0].wrap && timing.endBeats == 0);
    CHECK(storage.getSample(0, 0) == 0.25f && storage.getSample(0, 23999) == 0);
    timing = render(1); CHECK(timing.spans[0].wrap && storage.getSample(0, 0) == 0.25f);
    state.setLoopEnabled(false); state.setPositionInBeats(0); state.setTempo(60); state.setTimeSignature(6, 8);
    timing = render(24000); CHECK(timing.discontinuity && timing.endBeats == 0.5);
    render(1); CHECK(storage.getSample(0, 0) == 0.15f); // Eighth-note beat, not dotted-quarter.
    state.setPositionInBeats(3); render(1); CHECK(storage.getSample(0, 0) == 0.25f);
    state.setPositionInBeats(2); state.setTimeSignature(3, 4); render(1); CHECK(storage.getSample(0, 0) == 0.15f);
    state.setPositionInBeats(3); render(1); CHECK(storage.getSample(0, 0) == 0.25f);
    state.setMetronomeEnabled(false); render(1); CHECK(storage.getSample(0, 0) == 0);
    state.setMetronomeEnabled(true); render(1); CHECK(storage.getSample(0, 0) == 0); // No off-grid chase.
    state.setPositionInBeats(0); state.setTempo(120); state.setTimeSignature(4, 4);
    render(1); state.setTempo(60); render(24000);
    const double tail = 0.25 * std::exp(-7.0 / 960) * std::cos(juce::MathConstants<double>::twoPi * 1760 / 48000);
    CHECK(std::abs(storage.getSample(0, 0) - tail) < 1e-6); // Tempo preserves the existing oscillator tail.
    render(23998); render(1); CHECK(storage.getSample(0, 0) == 0.15f); // New tempo owns the next grid tick.
    state.setTempo(300); state.setTimeSignature(6, 8); render(4800);
    CHECK(storage.getSample(0, 0) == 0 && storage.getSample(0, 4799) == 0.15f);
    state.setLoopRegion(0, 1.0 / 64); state.setLoopEnabled(true);
    timing = clock.beginBlock(state, 4, 1); // Below one sample per loop: bounded rejection, valid final clock.
    CHECK(timing.spanOverflow && timing.spanCount <= TransportClock::Block::maxSpans);
    clock.endBlock(state, timing);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static const juce::PopupMenu::Item* menuText(juce::PopupMenu& menu, juce::StringRef text) {
    const juce::PopupMenu::Item* found = nullptr;
    juce::PopupMenu::MenuItemIterator it(menu);
    while (it.next()) {
        const auto& item = it.getItem();
        if (item.text == text && found == nullptr) found = &item;
    }
    return found;
}

static void mixerRoutingMenuTests() {
    Project project;
    auto& channels = project.getChannelList();
    auto* a = channels.addChannel("Target"); auto* b = channels.addChannel("Active");
    const auto aid = a->getId(), bid = b->getId();
    project.setActiveChannel(1);
    channels.clearMixerChannels();
    auto* bus = channels.restoreMixerChannel(42, "Shared bus"); CHECK(bus);
    const auto busId = bus->getId();
    channels.addMixerChannel("Unused bus");
    std::function<void()> afterOwner;
    {
        ChannelRackContent rack(project);
        rack.setSize(250, 240);
        auto* row = dynamic_cast<ChannelRow*>(rack.getChildComponent(1));
        CHECK(row && row->getChannel() == a && row->getOutputButton().isVisible());
        CHECK(row->getOutputButton().getButtonText() == "Output: Master");
        CHECK(row->getLocalBounds().contains(row->getOutputButton().getBounds()));
        auto menu = row->createOutputMenu();
        CHECK(menu.getNumItems() == 3 && project.getActiveChannelId() == bid);
        CHECK(menuText(menu, "Master") && menuText(menu, "Master")->isTicked);
        const auto* item = menuText(menu, "Shared bus");
        CHECK(item && item->isEnabled && !item->isTicked && item->action);
        const auto route = item->action;
        const auto direct = menuText(menu, "Master")->action;
        auto context = row->createContextMenu();
        const auto* output = menuText(context, "Audio Output");
        CHECK(output && output->subMenu && menuText(*output->subMenu, "Shared bus"));
        channels.moveChannel(0, 1); // Saved actions still target a, not row index zero or active b.
        route();
        CHECK(a->getMixerTrackId() == busId && b->getMixerTrackId() == MasterDestination);
        CHECK(project.getActiveChannelId() == bid);
        row = dynamic_cast<ChannelRow*>(rack.getChildComponent(2));
        CHECK(row && row->getChannel() == a && row->getOutputButton().getButtonText() == "Output: Shared bus");
        auto* activeRow = dynamic_cast<ChannelRow*>(rack.getChildComponent(1));
        CHECK(activeRow && activeRow->getChannel() == b && activeRow->isSelected() && !row->isSelected());
        auto selected = row->createOutputMenu();
        CHECK(menuText(selected, "Shared bus")->isTicked && !menuText(selected, "Master")->isTicked);
        CHECK(rack.setChannelOutputById(bid, busId)); // Many-to-one is not exclusive assignment.
        CHECK(a->getMixerTrackId() == b->getMixerTrackId() && project.getActiveChannelId() == bid);
        bus->setName("Renamed output"); bus->sendSynchronousChangeMessage();
        CHECK(row->getOutputButton().getButtonText() == "Output: Renamed output");
        auto renamed = row->createOutputMenu(); CHECK(menuText(renamed, "Renamed output")->isTicked);
        direct(); CHECK(a->getMixerTrackId() == MasterDestination && b->getMixerTrackId() == busId);
        route(); CHECK(a->getMixerTrackId() == busId); // Rename does not invalidate a stable-ID action.
        channels.removeMixerChannel(busId);
        CHECK(a->getMixerTrackId() == MasterDestination && b->getMixerTrackId() == MasterDestination);
        CHECK(row->getOutputButton().getButtonText() == "Output: Master");
        auto* replacement = channels.addMixerChannel("Shared bus"); CHECK(replacement->getId() != busId);
        route(); CHECK(a->getMixerTrackId() == MasterDestination);
        CHECK(!rack.setChannelOutputById(aid, busId) && !rack.setChannelOutputById(aid, -2));
        auto liveMenu = rack.createOutputMenuForChannel(aid);
        const auto deletedTarget = menuText(liveMenu, "Shared bus")->action;
        auto survivorMenu = rack.createOutputMenuForChannel(bid);
        afterOwner = menuText(survivorMenu, "Shared bus")->action;
        channels.removeChannel(channels.indexOfChannel(a));
        deletedTarget(); direct(); route();
        CHECK(channels.getNumChannels() == 1 && b->getMixerTrackId() == MasterDestination);
        CHECK(rack.createOutputMenuForChannel(aid).getNumItems() == 0 && project.getActiveChannelId() == bid);
    }
    afterOwner(); CHECK(b->getMixerTrackId() == MasterDestination && project.getActiveChannelId() == bid);
    channels.clearMixerChannels();
    ChannelRackContent rack(project);
    auto masterOnly = rack.createOutputMenuForChannel(bid);
    CHECK(masterOnly.getNumItems() == 1 && menuText(masterOnly, "Master")->isTicked);
}

static void contextMenuTests() {
    // T14: target-specific menus and keyboard actions; all delayed actions
    // re-resolve stable IDs, so deleted targets become harmless no-ops.
    // Native dialogs are intercepted: offline coverage exercises show/accept/
    // cancel routing without windows; visible dialogs stay watcher/manual.
    int promptsShown = 0, confirmsShown = 0;
    juce::String nextPromptResponse = "Intercepted Name";
    vibedaw::textPromptInterceptor() = [&](const juce::String&, const juce::String&, const juce::String&,
                                           juce::Component*, std::function<void(const juce::String&)> accept) {
        ++promptsShown;
        accept(nextPromptResponse);
    };
    std::function<void()> pendingConfirm;
    juce::String confirmTitle, confirmMessage;
    vibedaw::confirmInterceptor() = [&](const juce::String& title, const juce::String& message, const juce::String&,
                                        juce::Component*, std::function<void()> ok) {
        ++confirmsShown;
        confirmTitle = title;
        confirmMessage = message;
        pendingConfirm = std::move(ok);
    };
    {
        // Channel row: menu acts on the clicked row without changing the
        // audition selection until an explicit Select action runs (T10 contract).
        Project project;
        ChannelRackContent rack(project);
        rack.setSize(250, 240);
        rack.setVisible(true);
        auto* alpha = project.getChannelList().addChannel("Alpha");
        auto* beta = project.getChannelList().addChannel("Beta");
        auto* rowAlpha = dynamic_cast<ChannelRow*>(rack.getChildComponent(1));
        CHECK(rowAlpha && rowAlpha->getChannel() == alpha);
        const auto alphaId = alpha->getId();
        CHECK(project.getActiveChannelId() == InvalidChannelId);
        auto menu = rowAlpha->createContextMenu();
        CHECK(menu.getNumItems() == 6); // Editor, reason header, Select, Audio Output, Rename, Remove.
        auto* open = menuText(menu, "Open Plugin Editor");
        CHECK(open && !open->isEnabled && open->action != nullptr); // No editor; reason shown.
        auto* select = menuText(menu, "Select for Live Audition & New Placements");
        CHECK(select && select->action != nullptr && !select->isTicked);
        CHECK(menuText(menu, "Rename Channel…") != nullptr);
        CHECK(menuText(menu, "Remove Channel…") != nullptr);
        select->action(); // The clicked row, not a prior selection, is resolved.
        CHECK(project.getActiveChannelId() == alpha->getId());
        CHECK(rowAlpha->isSelected());
        auto retick = rowAlpha->createContextMenu();
        CHECK(menuText(retick, "Select for Live Audition & New Placements")->isTicked);
        // Model actions by stable ID; blank names are rejected.
        rack.renameChannelById(alpha->getId(), "Renamed");
        CHECK(project.getChannelList().getChannelById(alpha->getId())->getName() == "Renamed");
        rack.renameChannelById(alpha->getId(), "   ");
        CHECK(project.getChannelList().getChannelById(alpha->getId())->getName() == "Renamed");
        // Placement impact is counted for the removal warning.
        auto& tracks = project.getTrackList();
        auto* track = tracks.addTrack();
        auto midi = std::make_unique<MidiClip>(0, 4);
        const auto clipId = project.getClipPool().addClip(std::move(midi));
        track->addClipInstance(std::make_unique<ClipInstance>(clipId, alpha->getId(), 0, 4));
        CHECK(rack.countPlacementsToChannel(alpha->getId()) == 1);
        // The rename prompt shares the validated action; the dialog is intercepted.
        const int promptsBeforeLiveRename = promptsShown;
        rowAlpha->renameRequested();
        CHECK(promptsShown == promptsBeforeLiveRename + 1);
        CHECK(project.getChannelList().getChannelById(alpha->getId())->getName() == "Intercepted Name");
        // Action copies outlive row rebuilds and channel deletion without mutation.
        auto selectAction = rowAlpha->selectAsActive;
        auto renameAction = rowAlpha->renameRequested;
        auto removeAction = rowAlpha->removeRequested;
        rack.removeChannelById(beta->getId());
        CHECK(project.getChannelList().getNumChannels() == 1 &&
              project.getChannelList().getChannelById(alpha->getId()) != nullptr);
        CHECK(project.getActiveChannelId() == alpha->getId());
        rack.removeChannelById(alpha->getId());
        CHECK(project.getChannelList().getNumChannels() == 0);
        const int promptsBeforeStale = promptsShown, confirmsBeforeStale = confirmsShown;
        selectAction();
        renameAction();
        removeAction(); // Deleted targets: no prompt, no dialog, no mutation.
        CHECK(promptsShown == promptsBeforeStale && confirmsShown == confirmsBeforeStale);
        CHECK(project.getActiveChannelId() == InvalidChannelId);
        CHECK(track->getNumClipInstances() == 1); // Placement remains as an unresolved placeholder.
        CHECK(track->getClipInstance(0)->getChannelId() == alphaId);
    }

    {
        // Pooled MIDI clip row: explicit edit, rename, and a distinct Delete Source.
        Project project;
        ClipsContent clips(project);
        clips.setSize(250, 240);
        clips.setVisible(true);
        auto* channel = project.getChannelList().addChannel("Dest");
        auto* track = project.getTrackList().addTrack("T");
        auto midi = std::make_unique<MidiClip>(0, 4);
        midi->setName("Source");
        const auto clipId = project.getClipPool().addClip(std::move(midi));
        track->addClipInstance(std::make_unique<ClipInstance>(clipId, channel->getId(), 0, 4));
        auto* row = dynamic_cast<ClipRow*>(clips.getChildComponent(2));
        CHECK(row && row->getClipId() == clipId);
        auto menu = row->createContextMenu();
        CHECK(menu.getNumItems() == 3);
        auto* editItem = menuText(menu, "Edit in Piano Roll");
        CHECK(editItem && editItem->isEnabled && editItem->action != nullptr);
        auto* renameItem = menuText(menu, "Rename Clip…");
        auto* deleteItem = menuText(menu, "Delete Source…");
        CHECK(renameItem && renameItem->isEnabled && renameItem->action != nullptr);
        CHECK(deleteItem && deleteItem->isEnabled && deleteItem->action != nullptr);
        CHECK(clips.countPlacementsOfClip(clipId) == 1);
        clips.renameClipById(clipId, "Renamed");
        CHECK(project.getClipPool().getClip(clipId)->getName() == "Renamed");
        clips.renameClipById(clipId, "");
        CHECK(project.getClipPool().getClip(clipId)->getName() == "Renamed");
        // The menu's rename prompt shares the validated action.
        const int promptsBeforeMenuRename = promptsShown;
        renameItem->action();
        CHECK(promptsShown == promptsBeforeMenuRename + 1);
        CHECK(project.getClipPool().getClip(clipId)->getName() == "Intercepted Name");
        clips.deleteSourceById(clipId);
        CHECK(project.getClipPool().getClip(clipId) == nullptr);
        CHECK(track->getNumClipInstances() == 1); // Placement survives as a placeholder.
        CHECK(track->getClipInstance(0)->getClipId() == clipId);

        // Button/menu parity: the footer Delete Source shares the menu's
        // confirmed, impact-warned path and never mutates before the dialog
        // resolves. Cancel preserves the source; confirm deletes it only.
        auto second = std::make_unique<MidiClip>(0, 4);
        second->setName("Second");
        const auto secondId = project.getClipPool().addClip(std::move(second));
        track->addClipInstance(std::make_unique<ClipInstance>(secondId, channel->getId(), 4, 4));
        clips.clipSelected(secondId, project.getClipPool().getClip(secondId));
        ClipsContentTestAccess::clickDeleteSource(clips);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50); // Deliver the click; the dialog is intercepted.
        CHECK(pendingConfirm != nullptr); // The impact warning is pending, nothing mutated.
        CHECK(confirmTitle == "Delete Source");
        CHECK(confirmMessage.contains("Second") && confirmMessage.contains("1 placement(s)"));
        CHECK(project.getClipPool().getClip(secondId) != nullptr);
        pendingConfirm = nullptr; // Cancel: no mutation.
        CHECK(project.getClipPool().getClip(secondId) != nullptr);
        ClipsContentTestAccess::clickDeleteSource(clips);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        CHECK(pendingConfirm != nullptr);
        pendingConfirm(); // Simulated OK.
        pendingConfirm = nullptr;
        CHECK(project.getClipPool().getClip(secondId) == nullptr); // Confirmed deletion.
        CHECK(track->getNumClipInstances() == 2); // Both placements remain as placeholders.
        CHECK(track->getClipInstance(1)->getClipId() == secondId);
        // The same menu item's delete action routes through the same confirmation.
        ClipsContentTestAccess::clickDeleteSource(clips); // Nothing selected: no dialog.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        CHECK(pendingConfirm == nullptr);
    }

    {
        // Track header: rename refreshes the header; removal is by stable UUID.
        TrackList tracks;
        TrackHeaderList list(tracks);
        list.setSize(150, 300);
        list.setVisible(true);
        auto* one = tracks.addTrack("One");
        tracks.addTrack("Two");
        auto* header = dynamic_cast<TrackHeader*>(list.getChildComponent(0));
        CHECK(header && header->getTrack() == one && header->getTrackName() == "One");
        auto menu = header->createContextMenu();
        CHECK(menu.getNumItems() == 2);
        CHECK(menuText(menu, "Rename Track…") != nullptr && menuText(menu, "Rename Track…")->isEnabled);
        CHECK(menuText(menu, "Remove Track…") != nullptr && menuText(menu, "Remove Track…")->isEnabled);
        auto renameAction = header->onRenameRequested;
        auto removeAction = header->onRemoveRequested;
        list.renameTrackById(one->getId(), "First");
        CHECK(one->getName() == "First");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        CHECK(header->getTrackName() == "First"); // Track change listener refreshes the header.
        list.renameTrackById("not-a-track", "X");
        list.renameTrackById(one->getId(), "  ");
        CHECK(one->getName() == "First");
        // The rename prompt shares the validated action; the dialog is intercepted.
        const int promptsBeforeTrackRename = promptsShown;
        renameAction();
        CHECK(promptsShown == promptsBeforeTrackRename + 1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        CHECK(one->getName() == "Intercepted Name" && header->getTrackName() == "Intercepted Name");
        tracks.addTrack("Three");
        const auto oneId = one->getId();
        list.removeTrackById(oneId);
        CHECK(tracks.getNumTracks() == 2 && tracks.getTrack(0)->getName() == "Two");
        list.removeTrackById(oneId); // Stale no-op.
        CHECK(tracks.getNumTracks() == 2);
        const int dialogsBeforeStaleTrack = promptsShown + confirmsShown;
        renameAction(); // Targets gone: prompts and confirmations are skipped entirely.
        removeAction();
        CHECK(promptsShown + confirmsShown == dialogsBeforeStaleTrack);
        CHECK(tracks.getNumTracks() == 2);
    }

    {
        // Timeline placement menu: contents, destination submenu, stale actions,
        // and Delete/Ctrl+E keyboard access.
        Project project;
        TimelinePanel panel(project);
        panel.setSize(800, 420);
        panel.setVisible(true);
        TimelineContent* content = nullptr;
        for (auto* child : panel.getChildren()) if (auto* c = dynamic_cast<TimelineContent*>(child)) content = c;
        CHECK(content);
        auto& tracks = project.getTrackList();
        auto& pool = project.getClipPool();
        auto& channels = project.getChannelList();
        auto* alpha = channels.addChannel("Alpha");
        auto* beta = channels.addChannel("Beta");
        auto* samp = channels.addChannel("Samp", Channel::Type::Sampler);
        auto midi = std::make_unique<MidiClip>(0, 4);
        midi->setName("Source");
        const auto clipId = pool.addClip(std::move(midi));
        auto audio = std::make_unique<AudioClip>(0, 2);
        const auto audioId = pool.addClip(std::move(audio));
        project.setActiveChannel(0);
        panel.setSelectedClip(clipId);
        auto* track = tracks.addTrack("T1");
        track->addClipInstance(std::make_unique<ClipInstance>(clipId, alpha->getId(), 1, 4));
        auto* instance = track->getClipInstance(0);
        const auto trackId = track->getId(), instanceId = instance->getId();

        auto menu = TimelineContentTestAccess::placementMenu(*content, *track, *instance);
        juce::PopupMenu::MenuItemIterator first(menu);
        CHECK(first.next() && first.getItem().isSectionHeader &&
              first.getItem().text.contains("T1 — Source"));
        auto* editItem = menuText(menu, "Edit Shared Source");
        CHECK(editItem && editItem->isEnabled && editItem->action != nullptr);
        auto* assign = menuText(menu, "Assign to Destination");
        CHECK(assign && assign->subMenu != nullptr);
        CHECK(assign->subMenu->getNumItems() == 2); // Instruments only; the sampler is excluded.
        auto* tickedAlpha = menuText(*assign->subMenu, "Alpha (#" + juce::String(alpha->getId()) + ")");
        CHECK(tickedAlpha && tickedAlpha->isTicked);
        auto* betaItem = menuText(*assign->subMenu, "Beta (#" + juce::String(beta->getId()) + ")");
        CHECK(betaItem && !betaItem->isTicked && betaItem->action != nullptr);
        betaItem->action();
        CHECK(instance->getChannelId() == beta->getId());
        CHECK(!content->assignPlacementDestinationById(trackId, instanceId, samp->getId()));
        CHECK(!content->assignPlacementDestinationById(trackId, instanceId, InvalidChannelId));
        CHECK(instance->getChannelId() == beta->getId());
        CHECK(content->assignPlacementDestinationById(trackId, instanceId, alpha->getId()));
        auto* mute = menuText(menu, "Mute");
        CHECK(mute && !mute->isTicked && mute->action != nullptr);
        mute->action();
        CHECK(instance->isMuted());
        auto menuAfterMute = TimelineContentTestAccess::placementMenu(*content, *track, *instance);
        auto* unmute = menuText(menuAfterMute, "Unmute");
        CHECK(unmute && unmute->isTicked && unmute->action != nullptr);
        unmute->action();
        CHECK(!instance->isMuted());
        CHECK(content->setPlacementStartById(trackId, instanceId, 3.25));
        CHECK(instance->getStartTime() == 3.25);
        CHECK(!content->setPlacementStartById(trackId, instanceId, -0.5));
        CHECK(!content->setPlacementStartById(trackId, instanceId, std::numeric_limits<double>::infinity()));
        CHECK(!content->setPlacementStartById(trackId, instanceId, std::numeric_limits<double>::quiet_NaN()));
        CHECK(!content->setPlacementStartById("gone", instanceId, 1.0));
        CHECK(instance->getStartTime() == 3.25);
        // The menu's Set Start Beat prompt shares the validated action and
        // rejects unparseable input without mutating.
        auto* startItem = menuText(menu, "Set Start Beat…");
        CHECK(startItem && startItem->isEnabled && startItem->action != nullptr);
        const int promptsBeforeStart = promptsShown;
        nextPromptResponse = "3.5";
        startItem->action();
        CHECK(promptsShown == promptsBeforeStart + 1);
        CHECK(instance->getStartTime() == 3.5);
        nextPromptResponse = "not a number";
        startItem->action();
        CHECK(promptsShown == promptsBeforeStart + 2);
        CHECK(instance->getStartTime() == 3.5);
        nextPromptResponse = "Intercepted Name";

        CHECK(content->removePlacementById(trackId, instanceId));
        CHECK(track->getNumClipInstances() == 0);
        CHECK(pool.getClip(clipId) != nullptr); // Removing a placement keeps the source.
        CHECK(!content->removePlacementById(trackId, instanceId)); // Stale IDs: harmless no-ops.
        CHECK(!content->setPlacementStartById(trackId, instanceId, 1.0));
        CHECK(!content->togglePlacementMuteById(trackId, instanceId));
        CHECK(!content->assignPlacementDestinationById(trackId, instanceId, alpha->getId()));
        CHECK(!content->editPlacementSourceById(trackId, instanceId));

        // A menu built before its target vanished performs no mutation and
        // raises no dialog.
        track->addClipInstance(std::make_unique<ClipInstance>(clipId, alpha->getId(), 0, 4));
        auto* second = track->getClipInstance(0);
        auto stale = TimelineContentTestAccess::placementMenu(*content, *track, *second);
        auto* removeItem = menuText(stale, "Remove Placement (source stays in Clips)");
        CHECK(removeItem && removeItem->action != nullptr);
        auto removeFn = removeItem->action;
        auto* staleStartItem = menuText(stale, "Set Start Beat…");
        CHECK(staleStartItem && staleStartItem->action != nullptr);
        auto staleStartFn = staleStartItem->action;
        track->clearClipInstances(); // Deleted while the menu is open.
        const int dialogsBeforeStaleMenu = promptsShown + confirmsShown;
        removeFn();
        staleStartFn();
        CHECK(promptsShown + confirmsShown == dialogsBeforeStaleMenu);
        CHECK(track->getNumClipInstances() == 0 && pool.getClip(clipId) != nullptr);

        // Unresolved source and destination are visible and disabled, not rerouted.
        auto* ghost = channels.addChannel("Ghost");
        const auto ghostId = ghost->getId();
        track->addClipInstance(std::make_unique<ClipInstance>(audioId, ghostId, 0, 2));
        pool.removeClip(audioId);
        channels.removeChannel(channels.indexOfChannel(ghost));
        auto* unresolved = track->getClipInstance(0);
        CHECK(pool.getClip(audioId) == nullptr && channels.getChannelById(ghostId) == nullptr);
        auto menuUnresolved = TimelineContentTestAccess::placementMenu(*content, *track, *unresolved);
        auto* editUnresolved = menuText(menuUnresolved, "Edit Shared Source");
        CHECK(editUnresolved && !editUnresolved->isEnabled);
        auto* assignUnresolved = menuText(menuUnresolved, "Assign to Destination");
        CHECK(assignUnresolved && assignUnresolved->subMenu != nullptr);
        auto* unresolvedItem = menuText(*assignUnresolved->subMenu,
            "(current destination unresolved #" + juce::String(ghostId) + ")");
        CHECK(unresolvedItem && !unresolvedItem->isEnabled);
        CHECK(assignUnresolved->subMenu->getNumItems() == 3); // Alpha, Beta, and the marker.

        // A live audio placement cannot open the MIDI editor.
        auto audio2 = std::make_unique<AudioClip>(0, 2);
        const auto audio2Id = pool.addClip(std::move(audio2));
        track->addClipInstance(std::make_unique<ClipInstance>(audio2Id, alpha->getId(), 5, 2));
        auto* audioPlacement = track->getClipInstance(1);
        auto menuAudio = TimelineContentTestAccess::placementMenu(*content, *track, *audioPlacement);
        CHECK(!menuText(menuAudio, "Edit Shared Source")->isEnabled);
        CHECK(!content->editPlacementSourceById(track->getId(), audioPlacement->getId()));

        // Keyboard: Delete/Backspace removes the selected placement, Ctrl+E edits
        // its shared source; unrelated keys and empty selections are untouched.
        ClipId edited = InvalidClipId;
        panel.onEditSource = [&](ClipId id) { edited = id; };
        track->addClipInstance(std::make_unique<ClipInstance>(clipId, alpha->getId(), 10, 4));
        auto* midiPlacement = track->getClipInstance(2);
        midiPlacement->setSelected(true);
        CHECK(content->editSelectedPlacementSource());
        CHECK(content->keyPressed(juce::KeyPress('E', juce::ModifierKeys::ctrlModifier, 0)));
        CHECK(edited == clipId);
        midiPlacement->setSelected(false);
        edited = InvalidClipId;
        CHECK(!content->keyPressed(juce::KeyPress('E', juce::ModifierKeys::ctrlModifier, 0)));
        CHECK(edited == InvalidClipId);
        midiPlacement->setSelected(true);
        CHECK(content->keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
        CHECK(track->getNumClipInstances() == 2 &&
              track->getClipInstance(0) == unresolved && track->getClipInstance(1) == audioPlacement);
        CHECK(!content->keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
        CHECK(!content->keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)));
        CHECK(content->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));
        CHECK(!content->keyPressed(juce::KeyPress('x')));

        // Empty-space menus place through the same commit path as drags.
        panel.setSelectedClip(clipId);
        project.setActiveChannel(channels.indexOfChannel(alpha));
        CHECK(content->pooledPlacementIsValid(2.5));
        struct TrackProbe : TrackList::Listener {
            explicit TrackProbe(TrackList& list) : tracks(list) { tracks.addListener(this); }
            ~TrackProbe() override { tracks.removeListener(this); }
            int added = 0;
            void trackAdded(Track*) override { ++added; }
            void trackRemoved(int) override {}
            void trackChanged(Track*) override {}
            void trackListChanged() override {}
            TrackList& tracks;
        } probe(tracks);
        auto menuNew = TimelineContentTestAccess::emptySpaceMenu(*content, {}, true, 2.5);
        CHECK(menuNew.getNumItems() == 3); // Section header, Place, Add Track.
        auto* placeNew = menuText(menuNew, "Place Selected Clip Here");
        CHECK(placeNew && placeNew->isEnabled && placeNew->action != nullptr);
        auto placeNewFn = placeNew->action;
        placeNewFn();
        CHECK(tracks.getNumTracks() == 2 && probe.added == 1);
        auto* newTrack = tracks.getTrack(1);
        CHECK(newTrack->getNumClipInstances() == 1);
        CHECK(newTrack->getClipInstance(0)->getStartTime() == 2.5);
        CHECK(newTrack->getClipInstance(0)->getChannelId() == alpha->getId());
        CHECK(newTrack->getClipInstance(0)->isSelected());
        // Existing empty lane space commits on that track; no new lane appears.
        auto menuExisting = TimelineContentTestAccess::emptySpaceMenu(*content, track->getId(), false, 10.0);
        auto* placeExisting = menuText(menuExisting, "Place Selected Clip Here");
        CHECK(placeExisting && placeExisting->isEnabled);
        placeExisting->action();
        CHECK(track->getNumClipInstances() == 3);
        CHECK(track->getClipInstance(2)->getStartTime() == 10.0);
        CHECK(tracks.getNumTracks() == 2 && probe.added == 1);
        // Enablement mirrors the commit contract.
        panel.setSelectedClip(InvalidClipId);
        CHECK(!content->pooledPlacementIsValid(1.0));
        auto menuNoClip = TimelineContentTestAccess::emptySpaceMenu(*content, {}, true, 1.0);
        CHECK(!menuText(menuNoClip, "Place Selected Clip Here")->isEnabled);
        panel.setSelectedClip(audio2Id); // Audio source.
        CHECK(!content->pooledPlacementIsValid(1.0));
        project.setActiveChannel(channels.indexOfChannel(samp));
        panel.setSelectedClip(clipId);
        CHECK(!content->pooledPlacementIsValid(1.0)); // Sampler destination.
        project.setActiveChannel(channels.indexOfChannel(alpha));
        panel.setSelectedClip(clipId);
        // A target deleted while the menu is open must not become a new lane.
        auto* victim = tracks.addTrack("Victim");
        const auto victimId = victim->getId();
        auto menuVictim = TimelineContentTestAccess::emptySpaceMenu(*content, victimId, false, 1.0);
        auto* placeVictim = menuText(menuVictim, "Place Selected Clip Here");
        CHECK(placeVictim && placeVictim->isEnabled);
        auto victimFn = placeVictim->action;
        tracks.removeTrack(tracks.indexOfTrack(victim));
        victimFn();
        CHECK(tracks.getNumTracks() == 2);
        auto menuAdd = TimelineContentTestAccess::emptySpaceMenu(*content, {}, true, 0.0);
        auto* addTrack = menuText(menuAdd, "Add Track");
        CHECK(addTrack && addTrack->isEnabled && addTrack->action != nullptr);
        addTrack->action();
        CHECK(tracks.getNumTracks() == 3 && probe.added == 3); // Two placements plus the victim each created one lane.
    }

    {
        // Browser plugin: the context action uses the same load path as a double-click.
        PluginScanner scanner;
        PluginSection section(scanner);
        section.setSize(200, 220);
        section.setVisible(true);
        struct PluginActions : PluginSection::Listener {
            juce::String doubleClicked;
            void pluginSelected(const juce::String&) override {}
            void pluginDoubleClicked(const juce::String& path) override { doubleClicked = path; }
        } actions;
        section.setPluginListener(&actions);
        section.createChannelFromPlugin("/offline/test.vst3");
        CHECK(actions.doubleClicked == "/offline/test.vst3");
    }

    {
        // Piano roll grid: focused Delete removes selected notes and clears
        // the borrowed selection on invalidation.
        MidiClip midi(0, 4);
        midi.addNote(Note(60, 0, 1));
        midi.addNote(Note(62, 1, 1));
        NoteGridComponent grid;
        grid.setMidiClip(&midi);
        struct NoteEvents : NoteGridComponent::Listener {
            int removed = 0;
            int pitch = -1;
            void noteAdded(const Note&) override {}
            void noteRemoved(const Note& note) override { ++removed; pitch = note.getPitch(); }
            void noteChanged(const Note&) override {}
        } events;
        grid.setListener(&events);
        grid.selectNote(&midi.getNotes()[0]); // Taken after all additions: borrowed pointers die on reallocation.
        CHECK(grid.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
        CHECK(midi.getNumNotes() == 1 && events.removed == 1 && events.pitch == 60);
        CHECK(!grid.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey))); // Selection was cleared.
        CHECK(!grid.keyPressed(juce::KeyPress('x')));
        auto* surviving = &midi.getNotes()[0];
        grid.selectNote(surviving);
        midi.removeNote(0); // Invalidation clears borrowed selection pointers.
        CHECK(grid.getSelectedNotes().empty());
    }

    vibedaw::textPromptInterceptor() = nullptr;
    vibedaw::confirmInterceptor() = nullptr;
    CHECK(pendingConfirm == nullptr); // Every intercepted dialog resolved; none lingers.
    CHECK(juce::Component::getNumCurrentlyModalComponents() == 0);
}

static void editorNavigationTests() {
    // ---- Shared geometry: pitch rows round-trip, boundaries and scroll agree.
    {
        PianoRollGeometry geo;
        geo.lowestNote = 0;
        geo.numKeys = 128;
        geo.keyHeight = 12;
        CHECK(geo.highestNote() == 127);
        CHECK(geo.gridHeight() == 128 * 12);
        for (int pitch = 0; pitch <= 127; ++pitch) {
            const int y = geo.yFromPitch(pitch, 0);
            CHECK(geo.pitchFromY(y, 0) == pitch);
            if (pitch > 0) CHECK(geo.pitchFromY(y + geo.keyHeight - 1, 0) == pitch);
        }
        // Every valid MIDI pitch is reachable, including 0 and 127.
        CHECK(geo.yFromPitch(127, 0) == 0);
        CHECK(geo.yFromPitch(0, 0) == 127 * 12);
        // The keyboard's viewport offset resolves the same row as the grid.
        CHECK(geo.pitchFromY(0, 5 * 12) == 122);
        CHECK(geo.yFromPitch(122, 5 * 12) == 0);
        // Fractional pixel scrolling still resolves the correct row.
        CHECK(geo.pitchFromY(0, 7) == 127);
        CHECK(geo.pitchFromY(4, 7) == 127); // Same row: 0 + 7 is still within [0, 12).
        CHECK(geo.pitchFromY(5, 7) == 126); // 5 + 7 crosses into the next row.
        CHECK(PianoRollGeometry::floorDiv(-1, 12) == -1);
    }

    // ---- Beat <-> x round-trips at multiple zoom levels.
    {
        for (double ppb : {20.0, 80.0, 320.0}) {
            for (double beat : {0.0, 0.25, 3.75, 40.5}) {
                const double x = PianoRollGeometry::xFromBeat(beat, ppb);
                CHECK(std::abs(PianoRollGeometry::beatFromX(x, ppb) - beat) < 1e-9);
            }
        }
    }

    // ---- Editable extent: clip length, notes, minimum and margin.
    {
        CHECK(pianoRollContentBeats(4.0, 0.0) == 12.0);   // Minimum beats dominate.
        CHECK(pianoRollContentBeats(20.0, 0.0) == 24.0);
        CHECK(pianoRollContentBeats(4.0, 15.5) == 19.5);  // Longest note end wins.
        CHECK(pianoRollContentBeats(std::numeric_limits<double>::quiet_NaN(), 0.0) == 12.0);
    }

    // ---- Arrangement -> source-local mapping across placements/gaps/tails.
    {
        double local = -1.0;
        CHECK(sourceLocalBeat(1.0, 0.0, 4.0, 4.0, local) && local == 1.0);
        CHECK(sourceLocalBeat(3.999, 0.0, 4.0, 4.0, local));
        CHECK(!sourceLocalBeat(4.0, 0.0, 4.0, 4.0, local));  // Exclusive end.
        CHECK(!sourceLocalBeat(9.0, 4.0, 8.0, 4.0, local));  // Placement longer than source.
        CHECK(sourceLocalBeat(5.0, 4.0, 8.0, 10.0, local) && local == 1.0);
        CHECK(!sourceLocalBeat(0.5, 4.0, 4.0, 4.0, local));  // Before the placement.
    }

    // ---- Follow band decision and clamping.
    {
        CHECK(followedScrollX(500.0, 400.0, 400.0, 4000.0) < 0.0); // Inside band.
        const double forward = followedScrollX(950.0, 400.0, 400.0, 4000.0);
        CHECK(std::abs(forward - (950.0 - 200.0)) < 1e-9);
        CHECK(followedScrollX(410.0, 800.0, 400.0, 4000.0) < 800.0); // Behind band.
        CHECK(followedScrollX(10.0, 100.0, 400.0, 4000.0) == 0.0);   // Clamp low.
        CHECK(followedScrollX(3990.0, 100.0, 400.0, 4000.0) == 3600.0); // Clamp high.
        CHECK(followedScrollX(0.0, 0.0, 400.0, 400.0) < 0.0); // Content fits: no scroll.
    }

    // ---- Real grid hit testing targets the note under the pointer.
    {
        MidiClip midi(0, 8);
        midi.addNote(Note(60, 5.0, 1.0));
        midi.addNote(Note(0, 0.0, 1.0));
        midi.addNote(Note(127, 7.0, 1.0));
        NoteGridComponent grid;
        grid.setMidiClip(&midi);
        grid.setPixelsPerBeat(80);
        grid.setKeyHeight(12);
        grid.setLowestNote(0);
        grid.setSize(40 * 80, 128 * 12);
        grid.setPlayheadBeats(6.5);
        CHECK(grid.getPlayheadBeats() == 6.5);

        auto mouse = [&](juce::Point<float> point, int modifiers) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point, modifiers,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &grid, &grid, juce::Time::getCurrentTime(),
                point, juce::Time::getCurrentTime(), 1, false);
        };
        const auto clickNote = [&](int pitch, double beat) {
            const int y = grid.getGeometry().yFromPitch(pitch, 0);
            const int x = static_cast<int>(beat * 80.0);
            const juce::Point<float> point(static_cast<float>(x + 4), static_cast<float>(y + 6));
            grid.mouseDown(mouse(point, juce::ModifierKeys::leftButtonModifier));
            grid.mouseUp(mouse(point, 0));
        };
        clickNote(60, 5.0);
        {
            const auto selected = grid.getSelectedNotes();
            CHECK(selected.size() == 1 && selected.front()->getPitch() == 60);
        }
        clickNote(0, 0.0);
        {
            const auto selected = grid.getSelectedNotes();
            CHECK(selected.size() == 1 && selected.front()->getPitch() == 0);
        }
        clickNote(127, 7.0);
        {
            const auto selected = grid.getSelectedNotes();
            CHECK(selected.size() == 1 && selected.front()->getPitch() == 127);
        }
    }

    // ---- Keyboard rows line up with the grid at the same viewport offset.
    {
        PianoRollKeyboard keyboard;
        keyboard.setLowestNote(0);
        keyboard.setKeyHeight(12);
        keyboard.setScrollOffset(300); // Viewport scrolled down 25 rows.
        CHECK(keyboard.getKeyForY(0) == 102);
        CHECK(keyboard.getKeyForY(11) == 102);
        CHECK(keyboard.getKeyForY(12) == 101);
        CHECK(keyboard.getYForKey(102) == 0);
        PianoRollGeometry geo;
        geo.keyHeight = 12;
        CHECK(geo.pitchFromY(300, 0) == 102); // Grid-local row matches the keyboard view.
    }

    // ---- Grid invalidation and detachment are pointer-safe.
    {
        MidiClip midi(0, 4);
        midi.addNote(Note(60, 0.0, 1.0));
        NoteGridComponent grid;
        struct ExtentEvents : NoteGridComponent::Listener {
            int contentChanges = 0;
            void noteAdded(const Note&) override {}
            void noteRemoved(const Note&) override {}
            void noteChanged(const Note&) override {}
            void gridContentChanged() override { ++contentChanges; }
        } events;
        grid.setListener(&events);
        grid.setMidiClip(&midi);
        CHECK(events.contentChanges >= 1);
        grid.selectNote(&midi.getNotes()[0]);
        midi.addNote(Note(62, 2.0, 1.0)); // Reallocation invalidates borrowed selection.
        CHECK(grid.getSelectedNotes().empty());
        grid.setMidiClip(nullptr);
        CHECK(grid.getMidiClip() == nullptr);
        grid.selectNote(nullptr); // No-op.
        CHECK(grid.getSelectedNotes().empty());
    }

    // ---- Editor playhead + follow contract.
    {
        MidiClip midi(0, 16);
        midi.addNote(Note(60, 0.0, 1.0));
        PianoRollEditor editor;
        editor.setSize(800, 500);
        editor.setMidiClip(&midi, 7);

        TransportState transport;
        editor.setLocalBeatProvider([&](double arrangementBeat, double& local) {
            return sourceLocalBeat(arrangementBeat, 8.0, 16.0, midi.getDuration(), local);
        });
        editor.setTransport(&transport);

        CHECK(!editor.isFollowEnabled());
        CHECK(!editor.isPlayheadVisible()); // Transport at 0 with no covering placement.

        editor.setFollowEnabled(true);
        CHECK(editor.isFollowEnabled() && !editor.isFollowSuspended());

        // A resize is programmatic and must not suspend following.
        editor.setSize(900, 600);
        CHECK(!editor.isFollowSuspended());

        transport.setPlaying(true);
        transport.setPositionInBeats(8.0 + 5.0);
        CHECK(editor.isPlayheadVisible());
        CHECK(std::abs(editor.getPlayheadBeat() - 5.0) < 1e-9);

        // A far playhead is scrolled into the comfort band.
        transport.setPositionInBeats(8.0 + 15.0);
        CHECK(editor.isPlayheadVisible());
        CHECK(editor.getHorizontalScroll() > 0.0);

        // Following must never move the audio position (read-only contract).
        const double held = transport.getPositionInBeats();
        editor.transportPositionChanged(0.0);
        CHECK(transport.getPositionInBeats() == held);

        // Manual scrolling suspends until the Follow toggle resumes.
        editor.setHorizontalScroll(1.0);
        CHECK(editor.isFollowSuspended());
        editor.setFollowEnabled(true);
        CHECK(!editor.isFollowSuspended());

        // A position with no covering placement hides the playhead.
        transport.setPositionInBeats(8.0 + 100.0);
        CHECK(!editor.isPlayheadVisible());

        // Zoom preserves the beat at the viewport's left edge.
        transport.stop();
        editor.setHorizontalScroll(3.0);
        const double anchorBefore = editor.getHorizontalScroll();
        editor.setZoomLevel(2.0);
        CHECK(std::abs(editor.getHorizontalScroll() - anchorBefore) < 0.01);

        editor.setTransport(nullptr);
        CHECK(!editor.isPlayheadVisible());
    }

    // ---- Ruler playhead storage.
    {
        TimeRulerComponent ruler;
        CHECK(ruler.getPlayhead() < 0.0);
        ruler.setPlayhead(3.5);
        CHECK(ruler.getPlayhead() == 3.5);
    }
}

static void integrationWorkflowTests() {
    // T09 integration fixture: the Main.cpp wiring (Project -> ChannelMixer ->
    // AudioEngine) with a shared clip placed on two routed tracks, looped
    // playback, a mid-playback tempo change, mix controls, and a save/load
    // round trip that keeps rendering through the same mixer. Probes are
    // declared before the Project so instrument destructors outlive them.
    const juce::File home(VIBEDAW_TEST_HOME);
    CHECK(home.isDirectory());

    Probe lead, pad;
    std::vector<std::unique_ptr<Probe>> restored; // Must outlive the Project.
    int restoreCalls = 0;
    Project project;
    ProjectTestAccess::restorer(project,
        [&](const juce::PluginDescription& description, const juce::MemoryBlock& state) -> std::unique_ptr<PluginHost> {
            ++restoreCalls;
            CHECK(description.uniqueId == 0x7ea77 && description.isInstrument);
            auto* probe = restored.emplace_back(std::make_unique<Probe>()).get();
            auto instrument = std::make_unique<StatefulInstrument>(*probe);
            if (state.getSize() > 0)
                instrument->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            return std::make_unique<PluginHost>(std::move(instrument));
        });

    auto& channels = project.getChannelList();
    auto& tracks = project.getTrackList();
    auto& pool = project.getClipPool();
    auto& transport = project.getTransportState();

    auto* leadChannel = channels.addChannel("Lead");
    auto* padChannel = channels.addChannel("Pad");
    const auto leadId = leadChannel->getId();
    const auto padId = padChannel->getId();
    leadChannel->setPlugin(std::make_unique<PluginHost>(std::make_unique<StatefulInstrument>(lead)));
    padChannel->setPlugin(std::make_unique<PluginHost>(std::make_unique<StatefulInstrument>(pad)));
    leadChannel->setVolume(0.5f);
    transport.setTempo(120.0);
    transport.setLoopRegion(0, 8);
    transport.setLoopEnabled(true);

    // One shared source; each placement routes to a different instrument.
    auto source = std::make_unique<MidiClip>(4, 4);
    source->addNote(Note(60, 0, 1));
    source->addNote(Note(67, 2, 1));
    const auto clipId = pool.addClip(std::move(source));
    tracks.addTrack("One")->addClipInstance(std::make_unique<ClipInstance>(clipId, leadId, 0, 4));
    tracks.addTrack("Two")->addClipInstance(std::make_unique<ClipInstance>(clipId, padId, 4, 4));

    ChannelMixer mixer(channels, tracks, pool, transport);
    mixer.setActiveChannel(0);
    mixer.prepareToPlay(48000, 256);
    AudioEngine engine;
    engine.setProcessor(&mixer);
    AudioEngineTestAccess::prepare(engine, 48000, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    auto render = [&](int samples) {
        rendering = true;
        AudioEngineTestAccess::renderSamples(engine, buffer, samples);
        rendering = false;
        transport.pollRenderPosition();
    };
    int guard = 0;
    auto renderUntil = [&](auto&& predicate) {
        while (!predicate() && guard++ < 40000) render(256);
        CHECK(guard < 40000);
    };

    // Playback through the loop: Lead fires at beats 0 and 2, position tracks tempo.
    transport.setPlaying(true);
    renderUntil([&] { return transport.getPositionInBeats() >= 0.5; });
    CHECK(lead.notes == 1 && lead.sounding == 1 && pad.notes == 0);
    CHECK(std::abs(buffer.getSample(0, 128) - 0.125f) < 1e-6f); // 0.25 * volume 0.5, master 1.
    project.getMasterBus().setGain(0.5f);
    render(256); render(256);
    CHECK(std::abs(buffer.getSample(0, 128) - 0.0625f) < 1e-6f); // Post-sum master gain.
    project.getMasterBus().setGain(1.0f);

    // Mid-playback tempo change: the musical position holds (T01) and the next
    // arrangement event fires where the new tempo places it.
    transport.setTempo(90.0);
    transport.setMetronomeEnabled(true); // Click rounds trip through the document later.
    renderUntil([&] { return lead.notes >= 2; }); // note67@2 under the new tempo.
    CHECK(pad.notes == 0); // Pad's placement starts at beat 4.

    // Mute Pad before its window; its first-pass attacks must be suppressed
    // with destination-local cleanup, not stuck or deferred.
    padChannel->setMuted(true);
    renderUntil([&] { return transport.getPositionInBeats() >= 6.5; });
    CHECK(pad.notes == 0 && pad.sounding == 0);
    padChannel->setMuted(false); // Next Pad attacks arrive; held notes do not chase.

    // Loop wrap: Lead's note60 refires, then Pad's unmuted second pass.
    renderUntil([&] { return lead.notes >= 3; });
    renderUntil([&] { return pad.notes >= 1; });
    CHECK(pad.sounding >= 1); // Sounding again after unmute.

    // Stop: all voices cleaned, nothing stuck.
    transport.setPlaying(false);
    render(256);
    CHECK(lead.sounding == 0 && pad.sounding == 0);
    const auto clear = [](const Probe& p) {
        return std::all_of(p.held.begin(), p.held.end(), [](unsigned n) { return n == 0; });
    };
    CHECK(clear(lead) && clear(pad));

    // ---- Save, mutate, load: the same mixer must keep working afterwards.
    const auto file = home.getChildFile("integration" + juce::String(Constants::PROJECT_FILE_EXTENSION));
    juce::String error;
    CHECK(project.saveProjectAs(file, error));
    CHECK(!project.isDirty());
    CHECK(transport.isMetronomeEnabled() && transport.isLoopEnabled() && transport.isLoopRegionSet());
    channels.addChannel("Doomed");
    transport.setTempo(200.0);
    transport.setLoopEnabled(false);
    CHECK(project.isDirty());
    CHECK(project.prepareLoad(file, error));
    project.commitLoad();
    CHECK(!project.isDirty());
    CHECK(channels.getNumChannels() == 2);
    CHECK(channels.getChannelById(leadId) != nullptr && channels.getChannelById(padId) != nullptr);
    CHECK(channels.getChannel(0)->getName() == juce::String("Lead"));
    CHECK(channels.getChannel(1)->getName() == juce::String("Pad"));
    CHECK(std::abs(transport.getTempo() - 90.0) < 1e-12);
    CHECK(transport.isLoopEnabled() && transport.isLoopRegionSet());
    CHECK(std::abs(transport.getLoopRegion().startBeats) < 1e-12 &&
          std::abs(transport.getLoopRegion().endBeats - 8.0) < 1e-12);
    CHECK(transport.isMetronomeEnabled());
    CHECK(tracks.getNumTracks() == 2 && pool.getNumClips() == 1);
    CHECK(restored.size() == 2 && restoreCalls == 2);
    CHECK(lead.destroyed == 1 && pad.destroyed == 1); // Replaced off audio by the load.
    CHECK(lead.destroyedQuiescent && pad.destroyedQuiescent);
    CHECK(!lead.destroyedOnAudio && !pad.destroyedOnAudio);

    // Restored channels carry identity + opaque state and are prepared again.
    for (const auto& probe : restored) CHECK(probe->prepared);
    CHECK(restored[0]->notes == 0 && restored[1]->notes == 0);

    // Playback resumes through the SAME mixer, on the restored instruments.
    transport.setPlaying(false);
    render(256);
    transport.setPositionInBeats(0);
    transport.setPlaying(true);
    renderUntil([&] { return restored[0]->notes >= 1; });
    CHECK(restored[0]->sounding == 1 && restored[1]->notes == 0);
    renderUntil([&] { return restored[1]->notes >= 1; }); // Pad's second placement at beat 4.
    CHECK(restored[1]->sounding >= 1);

    // Restored mix state: Lead keeps its saved 0.5 volume under master 1.
    transport.setPlaying(false);
    transport.setMetronomeEnabled(false);
    transport.setPositionInBeats(0);
    transport.setPlaying(true);
    renderUntil([&] { return restored[0]->sounding == 1; });
    render(256);
    CHECK(std::abs(buffer.getSample(0, 128) - 0.125f) < 1e-6f);
    transport.setPlaying(false);
    render(256);
    int wind = 0;
    while ((restored[0]->sounding != 0 || restored[1]->sounding != 0) && wind++ < 20000) render(256);
    CHECK(wind < 20000 && restored[0]->sounding == 0 && restored[1]->sounding == 0);
    CHECK(clear(*restored[0]) && clear(*restored[1]));
    engine.clearProcessor();
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void builtinSynthTests() {
    // The format claims its identifier end-to-end through the ordinary load path.
    {
        PluginHost host;
        CHECK(!host.isLoaded());
        CHECK(host.loadPlugin(InternalPluginFormat::identifier));
        CHECK(host.isLoaded());
        CHECK(host.hasPluginDescription());
        CHECK(host.getPluginDescription().pluginFormatName == juce::String("Internal"));
        CHECK(host.getPluginName() == juce::String("VibeSynth"));
        CHECK(host.getPluginDescription().isInstrument);
    }

    // Renders sound from a note-on and decays back to silence after note-off.
    {
        auto instance = InternalPluginFormat::createInstance();
        CHECK(instance != nullptr);
        instance->prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        juce::MidiBuffer noteOn;
        noteOn.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
        instance->processBlock(buffer, noteOn);
        CHECK(std::abs(buffer.getSample(0, 256)) > 1.0e-4f);
        CHECK(std::abs(buffer.getSample(1, 256)) > 1.0e-4f);

        juce::MidiBuffer noteOff;
        noteOff.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        for (int block = 0; block < 120; ++block) {
            buffer.clear();
            instance->processBlock(buffer, noteOff);
        }
        CHECK(buffer.getMagnitude(0, 0, buffer.getNumSamples()) < 1.0e-4f);
        CHECK(buffer.getMagnitude(1, 0, buffer.getNumSamples()) < 1.0e-4f);
    }

    // Opaque state round trip preserves parameter values.
    {
        auto source = InternalPluginFormat::createInstance();
        auto* sourceSynth = dynamic_cast<VibeSynthProcessor*>(source.get());
        CHECK(sourceSynth != nullptr);
        if (auto* gain = sourceSynth->getParameterState().getParameter("gain"))
            gain->setValueNotifyingHost(0.25f);
        juce::MemoryBlock state;
        source->getStateInformation(state);
        CHECK(state.getSize() > 0);

        auto restored = InternalPluginFormat::createInstance();
        restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        auto* restoredSynth = dynamic_cast<VibeSynthProcessor*>(restored.get());
        CHECK(restoredSynth != nullptr);
        const auto* gain = restoredSynth->getParameterState().getParameter("gain");
        CHECK(gain != nullptr && std::abs(gain->getValue() - 0.25f) < 1.0e-3f);
    }

    // Saved projects restore the built-in instrument through the default
    // (un-overridden) plugin restorer, like any VST identity.
    {
        Project source;
        CHECK(source.loadPlugin(InternalPluginFormat::identifier));
        CHECK(source.getChannelList().getNumChannels() == 1);
        CHECK(source.getActiveChannel() == 0);

        const auto file = juce::File(VIBEDAW_TEST_HOME).getChildFile("builtin-roundtrip.vibedaw");
        juce::String error;
        CHECK(source.saveProjectAs(file, error));

        Project target;
        CHECK(target.prepareLoad(file, error));
        target.commitLoad();
        CHECK(target.getChannelList().getNumChannels() == 1);
        const auto* channel = target.getChannelList().getChannel(0);
        CHECK(channel != nullptr && channel->getPlugin() != nullptr);
        CHECK(channel->getPlugin()->getPluginName() == juce::String("VibeSynth"));
        CHECK(channel->getPlugin()->getPluginDescription().pluginFormatName == juce::String("Internal"));
        file.deleteFile();
    }

    // A project load must not overwrite the browser scan path with the
    // built-in identifier, and new sessions always seed a playable channel.
    {
        Project fresh;
        fresh.newProject();
        CHECK(fresh.getChannelList().getNumChannels() == 1);
        CHECK(fresh.getSettings().pluginPath != juce::String(InternalPluginFormat::identifier));
    }

    // The drag payload accepts the internal identifier alongside file paths.
    {
        const auto info = DragDropInfo::fromDragDescription(DragDropInfo::plugin(InternalPluginFormat::identifier));
        CHECK(info.type == DragSourceType::Plugin);
        CHECK(info.path == juce::String(InternalPluginFormat::identifier));
    }
}

static void mixerProjectFileTests() {
    const juce::File home(VIBEDAW_TEST_HOME);
    juce::String error;
    Project project;
    auto& channels = project.getChannelList();
    auto* a = channels.addChannel("Keep A"); auto* b = channels.addChannel("Keep B");
    const auto aid = a->getId(), bid = b->getId();
    auto* bus = channels.getMixerChannels().front().get();
    const auto busId = bus->getId();
    bus->setName("Keep mixer"); bus->setVolume(0.75f); bus->setPan(-0.25f);
    CHECK(channels.setChannelMixerDestination(aid, busId) && channels.setChannelMixerDestination(bid, busId));
    project.getTrackList().addTrack("Keep lane");
    const auto validFile = home.getChildFile("mixer-valid.vibedaw");
    CHECK(project.saveProjectAs(validFile, error));
    CHECK(!project.isDirty());
    ProjectDocument::Staged staged;
    CHECK(ProjectDocument::stage(validFile.loadFileAsString(), staged, error));
    CHECK(staged.channels.size() == 2 && staged.channels[0].mixerTrackId == busId && staged.channels[1].mixerTrackId == busId);
    bus->setName("Dirty mixer"); bus->sendSynchronousChangeMessage();
    CHECK(project.isDirty());
    project.setActiveChannel(1);
    project.getTransportState().setPlaying(true); project.getTransportState().setPositionInBeats(2);
    const auto before = ProjectDocument::serialize(project);
    const auto invalidFile = home.getChildFile("mixer-invalid.vibedaw");
    for (int invalid = 0; invalid < 21; ++invalid) {
        auto document = juce::JSON::parse(before);
        auto* root = document.getDynamicObject(); CHECK(root);
        auto entry = root->getProperty("mixerChannels")[0];
        auto* mixer = entry.getDynamicObject(); CHECK(mixer);
        auto* instrument = root->getProperty("channels")[0].getDynamicObject(); CHECK(instrument);
        // Invalid-ID cases must fail on the ID itself, not an incidental dangling route.
        if (invalid >= 4 && invalid <= 7)
            for (auto& channel : *root->getProperty("channels").getArray())
                channel.getDynamicObject()->setProperty("mixerTrackId", MasterDestination);
        switch (invalid) {
            case 0: root->removeProperty("mixerChannels"); break;
            case 1: root->setProperty("mixerChannels", 42); break;
            case 2: root->setProperty("mixerChannels", juce::Array<juce::var>{42}); break;
            case 3: root->setProperty("mixerChannels", juce::Array<juce::var>{entry, entry}); break;
            case 4: mixer->setProperty("id", MasterDestination); break;
            case 5: mixer->setProperty("id", std::numeric_limits<int>::max()); break;
            case 6: mixer->setProperty("id", 1.5); break;
            case 7: mixer->setProperty("id", "not an ID"); break;
            case 8: mixer->setProperty("volume", -0.01); break;
            case 9: mixer->setProperty("volume", 2.01); break;
            case 10: mixer->setProperty("volume", "nan"); break;
            case 11: mixer->setProperty("pan", -1.01); break;
            case 12: mixer->setProperty("pan", 1.01); break;
            case 13: mixer->setProperty("muted", "true"); break;
            case 14: mixer->setProperty("solo", 1); break;
            case 15: instrument->setProperty("mixerTrackId", 999999); break;
            case 16: instrument->setProperty("mixerTrackId", -2); break;
            case 17: instrument->setProperty("mixerTrackId", "Master"); break;
            case 18: {
                juce::Array<juce::var> oversized;
                for (int id = 0; id < ChannelList::maxMixerChannels; ++id) {
                    auto copy = juce::JSON::parse(juce::JSON::toString(entry));
                    copy.getDynamicObject()->setProperty("id", id);
                    oversized.add(copy);
                }
                root->setProperty("mixerChannels", oversized);
                CHECK(ProjectDocument::stage(juce::JSON::toString(document), staged, error));
                CHECK(staged.mixerChannels.size() == ChannelList::maxMixerChannels);
                auto extra = juce::JSON::parse(juce::JSON::toString(entry));
                extra.getDynamicObject()->setProperty("id", ChannelList::maxMixerChannels);
                oversized.add(extra);
                root->setProperty("mixerChannels", oversized);
                break;
            }
            case 19: mixer->setProperty("colour", "not a colour"); break;
            case 20: mixer->setProperty("name", 17); break;
        }
        const auto json = juce::JSON::toString(document);
        CHECK(!ProjectDocument::stage(json, staged, error) && error.isNotEmpty());
        CHECK(invalidFile.replaceWithText(json));
        CHECK(project.prepareLoad(validFile, error)); // A later failed prepare must discard this pending load too.
        CHECK(!project.prepareLoad(invalidFile, error) && error.isNotEmpty());
        project.commitLoad();
        CHECK(ProjectDocument::serialize(project) == before);
        CHECK(channels.getChannelById(aid) == a && channels.getChannelById(bid) == b);
        CHECK(channels.getMixerChannelById(busId) == bus && project.getActiveChannelId() == bid);
        CHECK(project.isDirty() && project.getProjectFile() == validFile);
        CHECK(project.getTransportState().isPlaying() && project.getTransportState().getPositionInBeats() == 2);
    }
    CHECK(project.prepareLoad(validFile, error)); project.commitLoad();
    CHECK(!project.isDirty() && channels.getNumMixerChannels() == 1);
    CHECK(channels.getChannelById(aid)->getMixerTrackId() == busId && channels.getChannelById(bid)->getMixerTrackId() == busId);
    CHECK(channels.getMixerChannelById(busId)->getVolume() == 0.75f && channels.getMixerChannelById(busId)->getPan() == -0.25f);

    // A true v1 document has no mixerChannels section. Even route 0 must not
    // connect to the new default bus: the old property never affected audio.
    const juce::String legacy = R"json({
        "formatVersion": 1,
        "master": {"gain": 0.8, "muted": false},
        "channels": [
            {"id": 7, "name": "Legacy A", "type": "instrument", "volume": 0.75,
             "pan": -0.25, "muted": true, "solo": true, "mixerTrackId": 0, "colour": "ffffa500"},
            {"id": 11, "name": "Legacy B", "type": "instrument", "volume": 1.5,
             "pan": 0.5, "muted": false, "solo": false, "mixerTrackId": 999, "colour": "ff6a6aff"}
        ],
        "clips": [], "tracks": [],
        "transport": {"tempo": 96, "numerator": 7, "denominator": 8,
                      "loop": {"exists": false, "enabled": false}, "metronome": false}
    })json";
    CHECK(ProjectDocument::stage(legacy, staged, error));
    CHECK(staged.version == 1 && staged.mixerChannels.size() == 1 && staged.channels.size() == 2);
    CHECK(staged.channels[0].mixerTrackId == MasterDestination && staged.channels[1].mixerTrackId == MasterDestination);
    const auto legacyFile = home.getChildFile("mixer-legacy-v1.vibedaw");
    CHECK(legacyFile.replaceWithText(legacy) && project.prepareLoad(legacyFile, error));
    project.commitLoad();
    CHECK(!project.isDirty() && channels.getNumMixerChannels() == 1 && channels.getNumChannels() == 2);
    auto* migrated = channels.getChannelById(7); auto* migratedB = channels.getChannelById(11);
    CHECK(migrated && migratedB && migrated->getName() == "Legacy A");
    CHECK(migrated->getVolume() == 0.75f && migrated->getPan() == -0.25f && migrated->isMuted() && migrated->isSolo());
    CHECK(migrated->getColour() == juce::Colour(0xffffa500));
    CHECK(migratedB->getVolume() == 1.5f && migratedB->getPan() == 0.5f && !migratedB->isMuted() && !migratedB->isSolo());
    CHECK(migrated->getMixerTrackId() == MasterDestination && migratedB->getMixerTrackId() == MasterDestination);
    bus = channels.getMixerChannels().front().get();
    CHECK(bus->getVolume() == 1 && bus->getPan() == 0 && !bus->isMuted() && !bus->isSolo());
    CHECK(project.getMasterBus().getGain() == 0.8f && project.getTransportState().getTempo() == 96);
    CHECK(ProjectDocument::stage(ProjectDocument::serialize(project), staged, error));
    CHECK(staged.version == ProjectDocument::currentVersion && staged.mixerChannels.size() == 1);

    // Zero is a saved v2 collection, not a request to seed the v1/default bus.
    channels.clearMixerChannels(); CHECK(project.isDirty());
    const auto zeroFile = home.getChildFile("mixer-zero-v2.vibedaw");
    CHECK(project.saveProjectAs(zeroFile, error));
    CHECK(ProjectDocument::stage(zeroFile.loadFileAsString(), staged, error) && staged.mixerChannels.empty());
    channels.addMixerChannel("Discard on load");
    CHECK(project.prepareLoad(zeroFile, error)); project.commitLoad();
    CHECK(channels.getNumMixerChannels() == 0 && channels.getNumChannels() == 2 && !project.isDirty());
    CHECK(channels.getChannelById(7)->getMixerTrackId() == MasterDestination);
    CHECK(ProjectDocument::stage(ProjectDocument::serialize(project), staged, error) && staged.mixerChannels.empty());
    bus = channels.addMixerChannel("Dirty controls"); CHECK(project.isDirty());
    CHECK(project.saveProject(error));
    bus->setVolume(0.5f); bus->sendSynchronousChangeMessage(); CHECK(project.isDirty());
    CHECK(project.saveProject(error));
    CHECK(channels.setChannelMixerDestination(7, bus->getId()));
    channels.getChannelById(7)->sendSynchronousChangeMessage(); CHECK(project.isDirty());
}

static void projectFileTests() {
    const juce::File home(VIBEDAW_TEST_HOME);
    CHECK(home.isDirectory());

    const auto extension = juce::String(Constants::PROJECT_FILE_EXTENSION);
    juce::String error;

    // ---- Build a two-channel sketch with a shared clip and full document state.
    Probe first, second;
    std::vector<std::unique_ptr<Probe>> restoredProbes; // Probes must outlive the Project's plugin hosts.
    Project project;
    auto& channels = project.getChannelList();
    auto& pool = project.getClipPool();
    auto& tracks = project.getTrackList();
    auto& transport = project.getTransportState();

    CHECK(!project.isDirty() && project.getProjectName() == juce::String("Untitled"));
    // Restore seam: only the fake offline format resolves; state is applied
    // exactly as the default restorer would (T07 contract).
    int restoreCalls = 0;
    ProjectTestAccess::restorer(project,
        [&](const juce::PluginDescription& description, const juce::MemoryBlock& state) -> std::unique_ptr<PluginHost> {
            ++restoreCalls;
            CHECK(description.uniqueId == 0x7ea77 && description.isInstrument);
            auto* probe = restoredProbes.emplace_back(std::make_unique<Probe>()).get();
            auto instrument = std::make_unique<StatefulInstrument>(*probe);
            if (state.getSize() > 0)
                instrument->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            return std::make_unique<PluginHost>(std::move(instrument));
        });
    auto* alpha = channels.addChannel("Alpha");
    auto* beta = channels.addChannel("Beta");
    channels.clearMixerChannels();
    auto* group = channels.restoreMixerChannel(3, "Shared group"); CHECK(group);
    group->setVolume(1.25f); group->setPan(0.5f); group->setMuted(true); group->setSolo(true);
    group->setColour(juce::Colours::cyan);
    auto* unusedGroup = channels.restoreMixerChannel(19, "Unused group"); CHECK(unusedGroup);
    unusedGroup->setVolume(0.25f); unusedGroup->setPan(-1);
    const auto alphaId = alpha->getId();
    const auto betaId = beta->getId();
    CHECK(project.isDirty()); // Channel creation is document content.
    alpha->setPlugin(std::make_unique<PluginHost>(std::make_unique<StatefulInstrument>(first)));
    beta->setPlugin(std::make_unique<PluginHost>(std::make_unique<StatefulInstrument>(second)));
    alpha->setVolume(0.75f);
    alpha->setPan(-0.25f);
    alpha->setSolo(true);
    CHECK(channels.setChannelMixerDestination(alpha->getId(), 3));
    beta->setVolume(1.5f);
    beta->setMuted(true);
    beta->setColour(juce::Colour(0xffffa500));
    project.getMasterBus().setGain(0.8f);
    transport.setTempo(96.0);
    transport.setTimeSignature(7, 8);
    transport.setLoopRegion(4, 20);
    transport.setLoopEnabled(true);
    transport.setMetronomeEnabled(true);

    auto source = std::make_unique<MidiClip>(0, 4);
    source->addNote(Note(60, 0, 1, 90));
    source->addNote(Note(64, 1.5, 2, 111));
    source->setName("Sketch");
    const auto midiClipId = pool.addClip(std::move(source));
    auto audioSource = std::make_unique<AudioClip>(0, 8);
    const auto audioPath = home.getChildFile("tone.wav");
    audioSource->setAudioFile(audioPath);
    const auto audioClipId = pool.addClip(std::move(audioSource));
    auto patternSource = std::make_unique<PatternClip>(0, 4);
    patternSource->setPatternLength(2.0);
    patternSource->setLoopCount(3);
    const auto patternClipId = pool.addClip(std::move(patternSource));

    auto* trackOne = tracks.addTrack("One");
    auto* trackTwo = tracks.addTrack("Two");
    const auto trackOneId = trackOne->getId();
    const auto trackTwoId = trackTwo->getId();
    auto placed = std::make_unique<ClipInstance>(midiClipId, alphaId, 8, 4);
    const auto placedId = placed->getId();
    trackOne->addClipInstance(std::move(placed));
    trackOne->addClipInstance(std::make_unique<ClipInstance>(midiClipId, betaId, 16, 2));
    trackTwo->addClipInstance(std::make_unique<ClipInstance>(audioClipId, alphaId, 8, 4));
    // Unresolved source placeholder (T03): preserved across save/load.
    trackTwo->addClipInstance(std::make_unique<ClipInstance>(9999, alphaId, 0, 1));

    // ---- Serialize: the file never carries playback state or selection.
    transport.setPlaying(true);
    transport.setPositionInBeats(7);
    {
        const auto json = ProjectDocument::serialize(project);
        CHECK(json.contains("\"channels\"") && json.contains("\"tracks\"") && json.contains("\"clips\""));
        CHECK(json.contains("\"state\""));
        ProjectDocument::Staged staged;
        if (!ProjectDocument::stage(json, staged, error)) {
            std::cerr << "STAGE-ERR: " << error << "\nJSON: " << json << "\n";
            CHECK(false);
        }
        CHECK(staged.version == ProjectDocument::currentVersion);
        CHECK(staged.channels.size() == 2 && staged.clips.size() == 3 && staged.tracks.size() == 2);
        CHECK(staged.version == ProjectDocument::currentVersion && staged.mixerChannels.size() == 2);
        CHECK(staged.mixerChannels[0].id == 3 && staged.mixerChannels[1].id == 19);
        CHECK(staged.channels[0].mixerTrackId == 3 && staged.channels[1].mixerTrackId == MasterDestination);
        CHECK(!staged.tracks[0].instances[0].id.isEmpty());
        CHECK(staged.channels[0].plugin.has_value());
        CHECK(std::memcmp(staged.channels[0].plugin->state.getData(), "STATE-BYTES", 11) == 0);
    }

    // ---- Save, mutate further, then load the file back into the same project.
    const auto projectFile = home.getChildFile("roundtrip" + extension);
    CHECK(project.saveProjectAs(projectFile, error) && error.isEmpty());
    CHECK(projectFile.existsAsFile() && !project.isDirty());
    CHECK(project.getProjectName() == juce::String("roundtrip"));
    transport.setTempo(200.0); // Post-save mutation must be discarded by the load.
    channels.addChannel("Doomed");
    channels.clearMixerChannels();
    CHECK(project.prepareLoad(projectFile, error));
    project.commitLoad();
    CHECK(restoreCalls == 2);
    CHECK(restoredProbes.size() == 2);
    CHECK(!project.isDirty() && project.getProjectFile() == projectFile);
    CHECK(project.getProjectName() == juce::String("roundtrip"));
    CHECK(!transport.isPlaying() && !transport.isRecording());

    // Channels: stable IDs, order, names and mix state. The pre-load channel
    // pointers died in the replacement; resolve the restored ones by stable ID.
    CHECK(channels.getNumChannels() == 2);
    auto* restoredAlpha = channels.getChannelById(alphaId);
    auto* restoredBeta = channels.getChannelById(betaId);
    CHECK(restoredAlpha != nullptr && restoredBeta != nullptr);
    CHECK(channels.getChannel(0)->getId() == alphaId && channels.getChannel(1)->getId() == betaId);
    CHECK(restoredAlpha->getName() == juce::String("Alpha") && restoredAlpha->getVolume() == 0.75f &&
          restoredAlpha->getPan() == -0.25f);
    CHECK(restoredAlpha->isSolo() && !restoredAlpha->isMuted() && restoredAlpha->getMixerTrackId() == 3);
    CHECK(restoredBeta->getVolume() == 1.5f && restoredBeta->isMuted() && !restoredBeta->isSolo());
    CHECK(restoredBeta->getColour() == juce::Colour(0xffffa500));
    CHECK(restoredBeta->getMixerTrackId() == MasterDestination);
    CHECK(channels.getNumMixerChannels() == 2);
    group = channels.getMixerChannelById(3); unusedGroup = channels.getMixerChannelById(19);
    CHECK(group && unusedGroup && channels.getMixerChannels()[0].get() == group && channels.getMixerChannels()[1].get() == unusedGroup);
    CHECK(group->getName() == "Shared group" && group->getVolume() == 1.25f && group->getPan() == 0.5f);
    CHECK(group->isMuted() && group->isSolo() && group->getColour() == juce::Colours::cyan);
    CHECK(unusedGroup->getName() == "Unused group" && unusedGroup->getVolume() == 0.25f && unusedGroup->getPan() == -1);
    CHECK(!unusedGroup->isMuted() && !unusedGroup->isSolo());
    CHECK(restoredAlpha->hasPlugin() && restoredBeta->hasPlugin());
    CHECK(project.getActiveChannelId() == alphaId);
    {
        // Plugin identity round-trips beyond the bundle path (T07 contract).
        auto* restoredHost = restoredAlpha->getPlugin();
        const auto& description = restoredHost->getPluginDescription();
        CHECK(description.name == juce::String("Stateful instrument"));
        CHECK(description.pluginFormatName == juce::String("Offline"));
        CHECK(description.fileOrIdentifier == juce::String("/offline/stateful.vst3"));
        CHECK(description.manufacturerName == juce::String("vibedaw tests"));
        CHECK(description.version == juce::String("1.2.3"));
        CHECK(description.uniqueId == 0x7ea77 && description.isInstrument);
        auto* instrument = dynamic_cast<StatefulInstrument*>(restoredHost->getPluginInstance());
        CHECK(instrument != nullptr);
        CHECK(instrument->received.getSize() == 11);
        CHECK(std::memcmp(instrument->received.getData(), "STATE-BYTES", 11) == 0);
        auto* secondInstrument = dynamic_cast<StatefulInstrument*>(restoredBeta->getPlugin()->getPluginInstance());
        CHECK(secondInstrument != nullptr);
        CHECK(secondInstrument->received.getSize() == 11);
        CHECK(std::memcmp(secondInstrument->received.getData(), "STATE-BYTES", 11) == 0);
    }

    // Clips: notes, file references, pattern fields.
    auto* restoredMidi = dynamic_cast<MidiClip*>(pool.getClip(midiClipId));
    CHECK(restoredMidi != nullptr && restoredMidi->getName() == juce::String("Sketch"));
    CHECK(restoredMidi->getDuration() == 4.0);
    CHECK(restoredMidi->getNumNotes() == 2);
    const auto* note1 = restoredMidi->findNoteAt(0, 60);
    const auto* note2 = restoredMidi->findNoteAt(1.5, 64);
    CHECK(note1 != nullptr && note1->getVelocity() == 90 && note1->getChannel() == 1);
    CHECK(note2 != nullptr && note2->getVelocity() == 111 && note2->getDuration() == 2.0);
    auto* restoredAudio = dynamic_cast<AudioClip*>(pool.getClip(audioClipId));
    CHECK(restoredAudio != nullptr && restoredAudio->getAudioFile() == audioPath);
    auto* restoredPattern = dynamic_cast<PatternClip*>(pool.getClip(patternClipId));
    CHECK(restoredPattern != nullptr && restoredPattern->getPatternLength() == 2.0 &&
          restoredPattern->getLoopCount() == 3);

    // Tracks: UUID identities and placements restored exactly.
    CHECK(tracks.getNumTracks() == 2);
    CHECK(tracks.getTrack(0)->getId() == trackOneId && tracks.getTrack(1)->getId() == trackTwoId);
    CHECK(tracks.getTrack(0)->getName() == juce::String("One"));
    CHECK(tracks.getTrack(0)->getNumClipInstances() == 2);
    CHECK(tracks.getTrack(0)->getClipInstance(0)->getId() == placedId);
    CHECK(tracks.getTrack(0)->getClipInstance(0)->getClipId() == midiClipId);
    CHECK(tracks.getTrack(0)->getClipInstance(0)->getChannelId() == alphaId);
    CHECK(tracks.getTrack(0)->getClipInstance(0)->getStartTime() == 8.0);
    CHECK(tracks.getTrack(1)->getClipInstance(1)->getClipId() == 9999); // Unresolved placeholder.
    CHECK(pool.getClip(9999) == nullptr);

    // Transport and master: musical state restored, playback state not.
    CHECK(!transport.isPlaying() && !transport.isRecording());
    CHECK(transport.getTempo() == 96.0);
    const auto meter = transport.getTimeSignature();
    CHECK(meter.numerator == 7 && meter.denominator == 8);
    const auto loop = transport.getLoopRegion();
    CHECK(loop.exists && loop.enabled && loop.startBeats == 4.0 && loop.endBeats == 20.0);
    CHECK(transport.isMetronomeEnabled());
    CHECK(project.getMasterBus().getGain() == 0.8f && !project.getMasterBus().isMuted());

    // ---- Newly created objects receive non-colliding IDs after load.
    const auto freshClipId = pool.addClip(std::make_unique<MidiClip>());
    CHECK(freshClipId > midiClipId && freshClipId > audioClipId && freshClipId > patternClipId);
    auto* freshChannel = channels.addChannel("Fresh");
    CHECK(freshChannel->getId() > alphaId && freshChannel->getId() > betaId);
    CHECK(freshChannel->getMixerTrackId() == MasterDestination);
    auto* freshMixer = channels.addMixerChannel(); CHECK(freshMixer && freshMixer->getId() > 19);
    channels.removeMixerChannel(freshMixer->getId());
    // Channel reorder preserves per-instance routing identity.
    auto* restoredTrackOne = tracks.getTrackById(trackOneId);
    CHECK(restoredTrackOne != nullptr);
    channels.moveChannel(0, 1);
    CHECK(channels.getChannel(0)->getId() == betaId);
    CHECK(restoredTrackOne->getClipInstance(0)->getChannelId() == alphaId);
    channels.moveChannel(1, 0);
    pool.removeClip(freshClipId);
    channels.removeChannel(channels.indexOfChannel(freshChannel));

    // ---- Missing plugins stay as unresolved channels preserving identity+blob.
    {
        Probe probe;
        Project source;
        auto* channel = source.getChannelList().addChannel("Unavailable");
        channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<StatefulInstrument>(probe)));
        const auto missingFile = home.getChildFile("missing" + extension);
        CHECK(source.saveProjectAs(missingFile, error));

        Project target;
        int refused = 0;
        ProjectTestAccess::restorer(target,
            [&](const juce::PluginDescription&, const juce::MemoryBlock&) -> std::unique_ptr<PluginHost> {
                ++refused;
                return nullptr; // Simulates an unavailable plugin.
            });
        CHECK(target.prepareLoad(missingFile, error));
        target.commitLoad();
        CHECK(refused == 1);
        auto* restored = target.getChannelList().getChannel(channel->getId());
        CHECK(restored != nullptr && restored->getName() == juce::String("Unavailable"));
        CHECK(!restored->hasPlugin());
        const auto* missing = restored->getMissingPlugin();
        CHECK(missing != nullptr);
        CHECK(missing->description.name == juce::String("Stateful instrument"));
        CHECK(missing->state.getSize() == 11);
        CHECK(std::memcmp(missing->state.getData(), "STATE-BYTES", 11) == 0);
        // Re-saving preserves the blob for later recovery.
        const auto resaved = home.getChildFile("missing-resaved" + extension);
        CHECK(target.saveProjectAs(resaved, error));
        ProjectDocument::Staged resavedStage;
        CHECK(ProjectDocument::stage(resaved.loadFileAsString(), resavedStage, error));
        CHECK(resavedStage.channels.size() == 1 && resavedStage.channels[0].plugin.has_value());
        CHECK(resavedStage.channels[0].plugin->state.getSize() == 11);
        CHECK(std::memcmp(resavedStage.channels[0].plugin->state.getData(), "STATE-BYTES", 11) == 0);
        CHECK(resavedStage.channels[0].plugin->description.uniqueId == 0x7ea77);
    }

    // ---- Corruption and validation failures never touch the live session.
    {
        Project live;
        auto* keeper = live.getChannelList().addChannel("Keep");
        const auto keeperId = keeper->getId();
        auto* lane = live.getTrackList().addTrack("Keep");
        CHECK(live.isDirty());

        CHECK(!live.prepareLoad(home.getChildFile("does-not-exist" + extension), error));
        CHECK(error.isNotEmpty());
        CHECK(live.getChannelList().getNumChannels() == 1 &&
              live.getChannelList().getChannelById(keeperId) == keeper);

        const auto corruptFile = home.getChildFile("corrupt" + extension);
        corruptFile.replaceWithText("{ not a complete document");
        CHECK(!live.prepareLoad(corruptFile, error) && error.isNotEmpty());
        CHECK(live.getChannelList().getNumChannels() == 1);
        live.commitLoad(); // Without a successful prepare this is a no-op.
        CHECK(live.getChannelList().getNumChannels() == 1 && live.getTrackList().getNumTracks() == 1);

        // Complete-but-invalid documents must each be rejected before staging.
        auto buildDocument = []() {
            auto* root = new juce::DynamicObject();
            root->setProperty("formatVersion", ProjectDocument::currentVersion);
            root->setProperty("mixerChannels", juce::Array<juce::var>());
            auto* masterObject = new juce::DynamicObject();
            masterObject->setProperty("gain", 1.0);
            masterObject->setProperty("muted", false);
            root->setProperty("master", juce::var(masterObject));
            root->setProperty("clips", juce::Array<juce::var>());
            root->setProperty("tracks", juce::Array<juce::var>());
            auto* transportObject = new juce::DynamicObject();
            transportObject->setProperty("tempo", 120.0);
            transportObject->setProperty("numerator", 4);
            transportObject->setProperty("denominator", 4);
            auto* loopObject = new juce::DynamicObject();
            loopObject->setProperty("exists", false);
            loopObject->setProperty("enabled", false);
            transportObject->setProperty("loop", juce::var(loopObject));
            transportObject->setProperty("metronome", false);
            root->setProperty("transport", juce::var(transportObject));
            return root;
        };
        auto channelObject = [](int id, const char* state = nullptr) {
            auto* object = new juce::DynamicObject();
            object->setProperty("id", id);
            object->setProperty("name", juce::String("Broken"));
            object->setProperty("type", juce::String("instrument"));
            object->setProperty("volume", 1.0);
            object->setProperty("pan", 0.0);
            object->setProperty("muted", false);
            object->setProperty("solo", false);
            object->setProperty("mixerTrackId", -1);
            object->setProperty("colour", juce::String("ff6a6aff"));
            if (state != nullptr) {
                auto* plugin = new juce::DynamicObject();
                auto* description = new juce::DynamicObject();
                description->setProperty("name", juce::String("Broken"));
                description->setProperty("descriptiveName", juce::String());
                description->setProperty("format", juce::String("Offline"));
                description->setProperty("fileOrIdentifier", juce::String("/offline/broken.vst3"));
                description->setProperty("manufacturer", juce::String("tests"));
                description->setProperty("version", juce::String("1.0"));
                description->setProperty("category", juce::String());
                description->setProperty("uid", 0x7ea77);
                description->setProperty("isInstrument", true);
                description->setProperty("numInputChannels", 0);
                description->setProperty("numOutputChannels", 2);
                description->setProperty("hasSharedContainer", false);
                plugin->setProperty("description", juce::var(description));
                plugin->setProperty("state", juce::String(state));
                object->setProperty("plugin", juce::var(plugin));
            }
            return juce::var(object);
        };

        ProjectDocument::Staged staged;
        {
            auto* valid = buildDocument();
            valid->setProperty("channels", juce::Array<juce::var>{channelObject(0)});
            CHECK(ProjectDocument::stage(juce::JSON::toString(juce::var(valid)), staged, error));
            CHECK(staged.version == ProjectDocument::currentVersion && staged.mixerChannels.empty());
        }
        {
            auto* future = buildDocument();
            future->setProperty("formatVersion", ProjectDocument::currentVersion + 1);
            future->setProperty("channels", juce::Array<juce::var>{channelObject(0)});
            CHECK(!ProjectDocument::stage(juce::JSON::toString(juce::var(future)), staged, error));
            CHECK(error.contains("Unsupported project format version"));
        }
        {
            auto* noChannels = buildDocument();
            noChannels->setProperty("channels", juce::var(42));
            CHECK(!ProjectDocument::stage(juce::JSON::toString(juce::var(noChannels)), staged, error));
            CHECK(error.isNotEmpty());
        }
        {
            auto* duplicate = buildDocument();
            duplicate->setProperty("channels", juce::Array<juce::var>{channelObject(0), channelObject(0)});
            CHECK(!ProjectDocument::stage(juce::JSON::toString(juce::var(duplicate)), staged, error));
            CHECK(error.contains("more than once"));
        }
        {
            auto* corruptBlob = buildDocument();
            corruptBlob->setProperty("channels", juce::Array<juce::var>{channelObject(0, "!!!not base64!!!")});
            CHECK(!ProjectDocument::stage(juce::JSON::toString(juce::var(corruptBlob)), staged, error));
            CHECK(error.contains("state"));
        }
        // Out-of-range numeric values must be rejected too.
        {
            auto* badVolume = buildDocument();
            auto channel = channelObject(0);
            channel.getDynamicObject()->setProperty("volume", 99.0);
            badVolume->setProperty("channels", juce::Array<juce::var>{channel});
            CHECK(!ProjectDocument::stage(juce::JSON::toString(juce::var(badVolume)), staged, error));
            CHECK(error.contains("volume out of range"));
        }
        {
            auto* badTempo = buildDocument();
            badTempo->setProperty("channels", juce::Array<juce::var>());
            auto transport = badTempo->getProperty("transport");
            transport.getDynamicObject()->setProperty("tempo", 999.0);
            CHECK(!ProjectDocument::stage(juce::JSON::toString(juce::var(badTempo)), staged, error));
            CHECK(error.contains("tempo"));
        }
        {
            auto* badLoop = buildDocument();
            badLoop->setProperty("channels", juce::Array<juce::var>());
            auto transport = badLoop->getProperty("transport");
            transport.getDynamicObject()->setProperty("loop", juce::var(new juce::DynamicObject()));
            CHECK(!ProjectDocument::stage(juce::JSON::toString(juce::var(badLoop)), staged, error));
            CHECK(error.isNotEmpty());
        }

        CHECK(live.getChannelList().getNumChannels() == 1 &&
              live.getChannelList().getChannelById(keeperId) != nullptr);
        CHECK(lane->getNumClipInstances() == 0);

        // A successful prepare stages the document without mutating anything:
        // cancelling the discard prompt simply never calls commitLoad.
        const auto pendingFile = home.getChildFile("pending" + extension);
        CHECK(live.saveProjectAs(pendingFile, error));
        CHECK(!live.isDirty());
        live.getChannelList().addChannel("Extra");
        CHECK(live.isDirty());
        CHECK(live.prepareLoad(pendingFile, error));
        CHECK(live.getChannelList().getNumChannels() == 2); // Still live; staged not applied.
    }

    // ---- Unwritable destinations fail without corrupting anything.
    {
        Project project;
        project.getChannelList().addChannel("Saveable");
        CHECK(project.isDirty());
        juce::String saveError;
        CHECK(!project.saveProjectAs(juce::File::getSpecialLocation(juce::File::userHomeDirectory), saveError));
        CHECK(saveError.isNotEmpty());
        CHECK(project.isDirty()); // A failed save must not report success.
        CHECK(!project.saveProject(saveError)); // No path chosen yet.
        CHECK(saveError.isNotEmpty());
    }

    // ---- Dirty tracking: mix/transport edits mark dirty; save/new clear it.
    {
        Project fresh;
        CHECK(!fresh.isDirty());
        const auto file = home.getChildFile("dirty" + extension);
        juce::String saveError;
        CHECK(fresh.saveProjectAs(file, saveError));
        CHECK(!fresh.isDirty() && fresh.getProjectName() == juce::String("dirty"));
        fresh.getTransportState().setTempo(150.0);
        CHECK(fresh.isDirty());
        fresh.getChannelList().addChannel("X")->setVolume(0.5f);
        CHECK(fresh.isDirty());
        CHECK(fresh.saveProject(saveError)); // Now has a path.
        CHECK(!fresh.isDirty());
        fresh.getClipPool().addClip(std::make_unique<MidiClip>());
        CHECK(fresh.isDirty());
        CHECK(fresh.getChannelList().addMixerChannel("Discard on New"));
        CHECK(fresh.getChannelList().getNumMixerChannels() == 2);
        fresh.newProject();
        CHECK(!fresh.isDirty() && fresh.getProjectName() == juce::String("Untitled"));
        // New sessions seed the built-in instrument so first-time users can play.
        CHECK(fresh.getChannelList().getNumChannels() == 1);
        const auto* seeded = fresh.getChannelList().getChannel(0);
        CHECK(seeded != nullptr && seeded->getPlugin() != nullptr);
        CHECK(seeded->getPlugin()->getPluginName() == juce::String("VibeSynth"));
        CHECK(seeded->getMixerTrackId() == MasterDestination && fresh.getChannelList().getNumMixerChannels() == 1);
        CHECK(fresh.getTrackList().getNumTracks() == 0 && fresh.getClipPool().getNumClips() == 0);
        CHECK(fresh.getTransportState().getTempo() == 120.0);
        CHECK(!fresh.getTransportState().isLoopRegionSet() && !fresh.getTransportState().isLoopEnabled());
        CHECK(!fresh.getTransportState().isMetronomeEnabled());
        CHECK(fresh.getProjectFile().getFullPathName().isEmpty());
    }
}

#include "midi_timing_tests.h"
#include "recording_model_tests.h"
#include "recording_session_tests.h"
#include "recording_ui_tests.h"
#include "panel_layout_tests.h"

int main() {
    try {
        // Project's real destructor saves settings. Never touch the user's home.
        const juce::File home(VIBEDAW_TEST_HOME);
        CHECK(home.createDirectory().wasOk());
        CHECK(setenv("HOME", home.getFullPathName().toRawUTF8(), 1) == 0);
        juce::ScopedJuceInitialiser_GUI juceInitialiser; // Framework only; no app/window/device.
        DawLookAndFeel lookAndFeel;
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
        struct ClearDefaultLookAndFeel {
            ~ClearDefaultLookAndFeel() { juce::LookAndFeel::setDefaultLookAndFeel(nullptr); }
        } clearDefaultLookAndFeel; // Clear before lookAndFeel dies, including assertion failures.
        sharedControlPaintTests(lookAndFeel);
        windowChromeTests();
        nativeMaximizeTests();
        editorWindowChromeTests();
        keyboardPaintTests();
        boundaryTests();
        modelTests();
        renderTests();
        reviewRegressionTests();
        pluginEditorBindingTests();
        rackDropAndClipCreationTests();
        timelineDragTests();
        timelineCommitTests();
        timelineInvalidationTests();
        timelineGestureReviewTests();
        sidebarRailTests();
        sidebarAnimationTests();
        panelLayoutTests();
        contextMenuTests();
        pluginEditorCreationTests();
        transportClockTests();
        transportEngineTests();
        externalMidiTests();
        midiTimingTests();
        arrangementPlaybackTests();
        arrangementLifecycleTests();
        arrangementCapacityTests();
        arrangementBoundaryOwnershipTests();
        arrangementMergedCapacityTests();
        independentMixerModelTests();
        independentMixerSignalTests();
        independentMixerNoteTests();
        mixerEditEngineTests();
        mixerSignalTests();
        mixerMeterTests();
        mixerSuppressionTests();
        mixerBindingTests();
        mixerRoutingMenuTests();
        loopTimestampTests();
        deferredWrapOffsetTests();
        loopControlCapacityTests();
        metronomeTests();
        clickMasterAndOverflowTests();
        iconTests();
        loopUiTests();
        tempoControlTests();
        loopPedalAndLedgerTests();
        loopClickAndClockTests();
        editorNavigationTests();
        builtinSynthTests();
        projectFileTests();
        mixerProjectFileTests();
        integrationWorkflowTests();
        expressiveClipTests();
        recorderSessionTests();
        recordingUiTests();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        std::cout << "Theme/control/keyboard paint, T03/T06/T01/T02/T04/T05, independent mixer/v2 project files, T10 editor access, T14 context menu, T16 loop UX, T07 project file, T08 editor navigation, built-in synth, T09 external MIDI and tempo control tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
