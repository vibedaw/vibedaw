#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Channel.h"
#include "Clip.h"
#include "Note.h"
#include "core/TransportState.h"
#include <optional>
#include <vector>

namespace vibedaw {

class Project;

// Versioned project-file model, separate from device/sidebar Settings (T07).
// Schema reference: .docs/PROJECT_FORMAT.md. Timing conventions: quarter-note
// beats for MIDI notes, clip source lengths, placement positions/lengths and
// loop bounds; linear gain 0..2 and pan -1..1 for mix state. Never serialized:
// playing/recording state, playhead position, transient selection, active-note
// ledgers, waveform caches, or the audio-device connection.
namespace ProjectDocument {

constexpr int currentVersion = 1;

// One channel's saved plugin identity and opaque vendor state.
struct PluginInfo {
    juce::PluginDescription description;
    juce::MemoryBlock state;
};

struct ChannelData {
    ChannelId id = InvalidChannelId;
    juce::String name;
    Channel::Type type = Channel::Type::Instrument;
    juce::File sampleFile;
    float volume = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
    int mixerTrackId = -1;
    juce::Colour colour{0xff6a6aff};
    // Present for a loaded plugin or an unresolved (missing) one; absent when
    // the channel has never had a plugin.
    std::optional<PluginInfo> plugin;
};

struct ClipData {
    ClipId id = InvalidClipId;
    Clip::Type type = Clip::Type::Midi;
    juce::String name;
    juce::Colour colour{0xff6a6aff};
    double startBeats = 0.0;
    double durationBeats = 1.0;
    bool loopEnabled = false; // Midi clip-local repetition flag (reserved).
    juce::File audioFile;     // Audio clips only.
    double patternLength = 1.0; // Pattern clips only.
    int loopCount = 1;
    std::vector<Note> notes;  // Midi clips only; local beats.
};

struct InstanceData {
    juce::String id; // Session UUID restored across loads (T12 handoff).
    ClipId clipId = InvalidClipId;
    ChannelId channelId = InvalidChannelId;
    double startBeats = 0.0;
    double durationBeats = 1.0;
    bool muted = false;
};

struct TrackData {
    juce::String id; // Restored UUID; empty means generated fresh on restore.
    juce::String name;
    int height = 80;
    juce::Colour colour{0xffaaaaaa};
    std::vector<InstanceData> instances;
};

struct TransportData {
    double tempo = 120.0;
    int numerator = 4;
    int denominator = 4;
    LoopRegion loop;
    bool metronome = false;
};

// A fully validated snapshot. Building it never touches the live project; it
// is the temporary model the commit contract requires before replacement.
struct Staged {
    int version = currentVersion;
    std::vector<ChannelData> channels;
    float masterGain = 1.0f;
    bool masterMuted = false;
    std::vector<ClipData> clips;
    std::vector<TrackData> tracks;
    TransportData transport;
};

// Serializes the current project to pretty JSON. Message thread; plugin state
// extraction is guarded by audio quiescence.
juce::String serialize(const Project& project);

// Parses and validates into a temporary model. Malformed input never throws;
// returns false with an actionable error message.
bool stage(const juce::String& json, Staged& out, juce::String& error);

// Writes through a temporary file plus safe replacement. Success is reported
// only after the target file has actually been replaced.
bool writeToFile(const Project& project, const juce::File& file, juce::String& error);

} // namespace ProjectDocument

} // namespace vibedaw