#include "ProjectDocument.h"
#include "Project.h"
#include "plugins/PluginHost.h"
#include "core/Constants.h"
#include "utils/Logger.h"
#include <cmath>
#include <limits>

namespace vibedaw {
namespace ProjectDocument {

namespace {

using DynamicObjectPtr = juce::DynamicObject*;

bool isNumber(const juce::var& value) {
    return value.isDouble() || value.isInt() || value.isInt64();
}

bool readNumber(const juce::var& value, double& out) {
    if (!isNumber(value)) return false;
    out = static_cast<double>(value);
    return std::isfinite(out);
}

bool readNumber(const juce::var& value, float& out) {
    double raw = 0;
    if (!readNumber(value, raw)) return false;
    out = static_cast<float>(raw);
    return std::isfinite(out);
}

bool readInt(const juce::var& value, int& out) {
    if (value.isInt() || value.isInt64()) {
        out = static_cast<int>(static_cast<juce::int64>(value));
        return true;
    }
    double raw = 0;
    if (!readNumber(value, raw)) return false;
    if (raw != std::floor(raw) || raw < -std::numeric_limits<int>::max() ||
        raw > std::numeric_limits<int>::max()) return false;
    out = static_cast<int>(raw);
    return true;
}

bool readBool(const juce::var& value, bool& out) {
    if (!value.isBool()) return false;
    out = static_cast<bool>(value);
    return true;
}

bool readString(const juce::var& value, juce::String& out) {
    if (!value.isString()) return false;
    out = value.toString();
    return true;
}

const juce::var property(const juce::DynamicObject* object, const char* name) {
    return object != nullptr ? object->getProperty(juce::Identifier(name)) : juce::var();
}

juce::String colourToString(const juce::Colour& colour) {
    return colour.toString();
}

bool readColour(const juce::var& value, juce::Colour& out) {
    juce::String text;
    if (!readString(value, text)) return false;
    const auto length = text.length();
    if (length != 6 && length != 8) return false;
    for (int i = 0; i < length; ++i) {
        const auto character = text[i];
        if (!(juce::CharacterFunctions::isDigit(character) ||
              (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F'))) return false;
    }
    out = juce::Colour::fromString(text);
    return true;
}

bool readDescription(const juce::DynamicObject* object, juce::PluginDescription& out) {
    if (!readString(object->getProperty("name"), out.name)) return false;
    if (!readString(object->getProperty("descriptiveName"), out.descriptiveName)) return false;
    if (!readString(object->getProperty("format"), out.pluginFormatName)) return false;
    if (!readString(object->getProperty("fileOrIdentifier"), out.fileOrIdentifier)) return false;
    if (!readString(object->getProperty("manufacturer"), out.manufacturerName)) return false;
    if (!readString(object->getProperty("version"), out.version)) return false;
    if (!readString(object->getProperty("category"), out.category)) return false;
    if (!readInt(object->getProperty("uid"), out.uniqueId)) return false;
    if (!readBool(object->getProperty("isInstrument"), out.isInstrument)) return false;
    if (!readInt(object->getProperty("numInputChannels"), out.numInputChannels)) return false;
    if (!readInt(object->getProperty("numOutputChannels"), out.numOutputChannels)) return false;
    if (!readBool(object->getProperty("hasSharedContainer"), out.hasSharedContainer)) return false;
    return true;
}

bool readPlugin(const juce::DynamicObject* object, PluginInfo& out, juce::String& error) {
    const auto descriptionVar = object->getProperty("description");
    if (!descriptionVar.isObject() || !readDescription(descriptionVar.getDynamicObject(), out.description)) {
        error = "Malformed plugin identity in project file.";
        return false;
    }
    const auto state = object->getProperty("state");
    if (!state.isString()) {
        error = "Malformed plugin state in project file.";
        return false;
    }
    juce::MemoryBlock blob;
    if (!blob.fromBase64Encoding(state.toString())) {
        error = "Corrupt plugin state blob in project file.";
        return false;
    }
    out.state = std::move(blob);
    return true;
}

struct SerializationContext {
    juce::Array<juce::var> channelArray;
    juce::Array<juce::var> clipArray;
    juce::Array<juce::var> trackArray;
};

juce::var descriptionToVar(const juce::PluginDescription& description) {
    auto* object = new juce::DynamicObject();
    object->setProperty("name", description.name);
    object->setProperty("descriptiveName", description.descriptiveName);
    object->setProperty("format", description.pluginFormatName);
    object->setProperty("fileOrIdentifier", description.fileOrIdentifier);
    object->setProperty("manufacturer", description.manufacturerName);
    object->setProperty("version", description.version);
    object->setProperty("category", description.category);
    object->setProperty("uid", description.uniqueId);
    object->setProperty("isInstrument", description.isInstrument);
    object->setProperty("numInputChannels", description.numInputChannels);
    object->setProperty("numOutputChannels", description.numOutputChannels);
    object->setProperty("hasSharedContainer", description.hasSharedContainer);
    return juce::var(object);
}

} // namespace

juce::String serialize(const Project& project) {
    JUCE_ASSERT_MESSAGE_THREAD
    auto& channels = project.getChannelList();
    auto& pool = project.getClipPool();
    auto& tracks = project.getTrackList();
    auto& transport = project.getTransportState();

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("application", "vibedaw");
    root->setProperty("formatVersion", currentVersion);

    {
        // Plugin state extraction touches plugin-owned memory; keep the render
        // callback out while vendor state is captured (T06/T07 contract).
        AudioQuiescence::Edit edit;

        juce::Array<juce::var> channelArray;
        for (const auto& channel : channels.getChannels()) {
            auto* object = new juce::DynamicObject();
            object->setProperty("id", channel->getId());
            object->setProperty("name", channel->getName());
            object->setProperty("type", channel->getType() == Channel::Type::Sampler ? "sampler" : "instrument");
            object->setProperty("volume", channel->getVolume());
            object->setProperty("pan", channel->getPan());
            object->setProperty("muted", channel->isMuted());
            object->setProperty("solo", channel->isSolo());
            object->setProperty("mixerTrackId", channel->getMixerTrackId());
            object->setProperty("colour", colourToString(channel->getColour()));
            if (channel->getType() == Channel::Type::Sampler && !channel->getSampleFile().getFullPathName().isEmpty())
                object->setProperty("sampleFile", channel->getSampleFile().getFullPathName());

            std::optional<PluginInfo> info;
            if (auto* host = channel->getPlugin(); host && host->isLoaded()) {
                PluginInfo captured;
                captured.description = host->getPluginDescription();
                if (auto* instance = host->getPluginInstance())
                    instance->getStateInformation(captured.state);
                info = std::move(captured);
            } else if (auto* missing = channel->getMissingPlugin()) {
                PluginInfo preserved;
                preserved.description = missing->description;
                preserved.state = missing->state;
                info = std::move(preserved);
            }
            if (info) {
                auto* pluginObject = new juce::DynamicObject();
                pluginObject->setProperty("description", descriptionToVar(info->description));
                pluginObject->setProperty("state", info->state.toBase64Encoding());
                object->setProperty("plugin", juce::var(pluginObject));
            }
            channelArray.add(juce::var(object));
        }
        root->setProperty("channels", channelArray);

        auto* masterObject = new juce::DynamicObject();
        masterObject->setProperty("gain", channels.getMasterBus().getGain());
        masterObject->setProperty("muted", channels.getMasterBus().isMuted());
        root->setProperty("master", juce::var(masterObject));

        juce::Array<juce::var> clipArray;
        for (const auto& entry : pool.getClips()) {
            const auto* clip = entry.second.get();
            auto* object = new juce::DynamicObject();
            object->setProperty("id", entry.first);
            const char* typeName = "midi";
            if (clip->getType() == Clip::Type::Audio) typeName = "audio";
            else if (clip->getType() == Clip::Type::Pattern) typeName = "pattern";
            object->setProperty("type", typeName);
            object->setProperty("name", clip->getName());
            object->setProperty("colour", colourToString(clip->getColour()));
            object->setProperty("startBeats", clip->getStartTime());
            object->setProperty("durationBeats", clip->getDuration());
            if (clip->getType() == Clip::Type::Midi) {
                if (const auto* midi = dynamic_cast<const MidiClip*>(clip)) {
                    object->setProperty("loopEnabled", midi->isLoopEnabled());
                    juce::Array<juce::var> noteArray;
                    for (const auto& note : midi->getNotes()) {
                        auto* noteObject = new juce::DynamicObject();
                        noteObject->setProperty("pitch", note.getPitch());
                        noteObject->setProperty("startBeats", note.getStartTime());
                        noteObject->setProperty("durationBeats", note.getDuration());
                        noteObject->setProperty("velocity", note.getVelocity());
                        noteObject->setProperty("channel", note.getChannel());
                        noteArray.add(juce::var(noteObject));
                    }
                    object->setProperty("notes", noteArray);
                }
            } else if (clip->getType() == Clip::Type::Audio) {
                if (const auto* audio = dynamic_cast<const AudioClip*>(clip))
                    object->setProperty("file", audio->getAudioFile().getFullPathName());
            } else if (clip->getType() == Clip::Type::Pattern) {
                if (const auto* pattern = dynamic_cast<const PatternClip*>(clip)) {
                    object->setProperty("patternLength", pattern->getPatternLength());
                    object->setProperty("loopCount", pattern->getLoopCount());
                }
            }
            clipArray.add(juce::var(object));
        }
        root->setProperty("clips", clipArray);

        juce::Array<juce::var> trackArray;
        for (const auto& track : tracks.getTracks()) {
            auto* trackObject = new juce::DynamicObject();
            trackObject->setProperty("id", track->getId());
            trackObject->setProperty("name", track->getName());
            trackObject->setProperty("height", track->getHeight());
            trackObject->setProperty("colour", colourToString(track->getColour()));
            juce::Array<juce::var> instanceArray;
            for (const auto& instance : track->getClipInstances()) {
                auto* instanceObject = new juce::DynamicObject();
                instanceObject->setProperty("id", instance->getId());
                instanceObject->setProperty("clipId", instance->getClipId());
                instanceObject->setProperty("channelId", instance->getChannelId());
                instanceObject->setProperty("startBeats", instance->getStartTime());
                instanceObject->setProperty("durationBeats", instance->getDuration());
                instanceObject->setProperty("muted", instance->isMuted());
                instanceArray.add(juce::var(instanceObject));
            }
            trackObject->setProperty("instances", instanceArray);
            trackArray.add(juce::var(trackObject));
        }
        root->setProperty("tracks", trackArray);

        auto* loopObject = new juce::DynamicObject();
        const auto loop = transport.getLoopRegion();
        loopObject->setProperty("exists", loop.exists);
        loopObject->setProperty("enabled", loop.enabled);
        loopObject->setProperty("startBeats", loop.startBeats);
        loopObject->setProperty("endBeats", loop.endBeats);
        auto* transportObject = new juce::DynamicObject();
        transportObject->setProperty("tempo", transport.getTempo());
        transportObject->setProperty("numerator", transport.getTimeSignature().numerator);
        transportObject->setProperty("denominator", transport.getTimeSignature().denominator);
        transportObject->setProperty("loop", juce::var(loopObject));
        transportObject->setProperty("metronome", transport.isMetronomeEnabled());
        root->setProperty("transport", juce::var(transportObject));
    }

    return juce::JSON::toString(juce::var(root.get()), true);
}

namespace {

// Every failure exits with an actionable message; nothing throws on malformed
// input. The staged snapshot is only mutated after all reads of a section pass.
bool stageChannels(const juce::var& value, Staged& out, juce::String& error) {
    if (!value.isArray()) { error = "Project file is missing its channel list."; return false; }
    const int count = value.size();
    if (count > ChannelList::maxChannels) {
        error = "Project file declares " + juce::String(count) + " channels; at most " +
                juce::String(ChannelList::maxChannels) + " are supported.";
        return false;
    }
    for (int i = 0; i < count; ++i) {
        const auto& item = value[i];
        if (!item.isObject()) { error = "Malformed channel entry " + juce::String(i) + "."; return false; }
        auto* object = item.getDynamicObject();
        ChannelData data;
        if (!readInt(object->getProperty("id"), data.id) || data.id < 0) {
            error = "Channel entry " + juce::String(i) + " has an invalid ID.";
            return false;
        }
        for (const auto& existing : out.channels)
            if (existing.id == data.id) {
                error = "Project file declares channel ID " + juce::String(data.id) + " more than once.";
                return false;
            }
        if (!readString(object->getProperty("name"), data.name)) {
            error = "Channel " + juce::String(data.id) + " has an invalid name.";
            return false;
        }
        juce::String typeName;
        if (!readString(object->getProperty("type"), typeName) || (typeName != "instrument" && typeName != "sampler")) {
            error = "Channel " + juce::String(data.id) + " has an invalid type.";
            return false;
        }
        data.type = typeName == "sampler" ? Channel::Type::Sampler : Channel::Type::Instrument;
        if (!readNumber(object->getProperty("volume"), data.volume) || data.volume < 0.0f || data.volume > 2.0f) {
            error = "Channel " + juce::String(data.id) + " has a volume out of range 0..2.";
            return false;
        }
        if (!readNumber(object->getProperty("pan"), data.pan) || data.pan < -1.0f || data.pan > 1.0f) {
            error = "Channel " + juce::String(data.id) + " has a pan out of range -1..1.";
            return false;
        }
        if (!readBool(object->getProperty("muted"), data.muted) ||
            !readBool(object->getProperty("solo"), data.solo)) {
            error = "Channel " + juce::String(data.id) + " has invalid mute/solo flags.";
            return false;
        }
        if (!readInt(object->getProperty("mixerTrackId"), data.mixerTrackId)) {
            error = "Channel " + juce::String(data.id) + " has an invalid mixer track ID.";
            return false;
        }
        if (!readColour(object->getProperty("colour"), data.colour)) {
            error = "Channel " + juce::String(data.id) + " has an invalid colour.";
            return false;
        }
        if (object->hasProperty("sampleFile")) {
            juce::String path;
            if (!readString(object->getProperty("sampleFile"), path)) {
                error = "Channel " + juce::String(data.id) + " has an invalid sample file.";
                return false;
            }
            data.sampleFile = juce::File(path);
        }
        if (object->hasProperty("plugin")) {
            const auto pluginVar = object->getProperty("plugin");
            if (!pluginVar.isObject()) {
                error = "Channel " + juce::String(data.id) + " has a malformed plugin entry.";
                return false;
            }
            PluginInfo info;
            if (!readPlugin(pluginVar.getDynamicObject(), info, error)) return false;
            data.plugin = std::move(info);
        }
        out.channels.push_back(std::move(data));
    }
    return true;
}

bool stageClips(const juce::var& value, Staged& out, juce::String& error) {
    if (!value.isArray()) { error = "Project file is missing its clip list."; return false; }
    for (int i = 0; i < value.size(); ++i) {
        const auto& item = value[i];
        if (!item.isObject()) { error = "Malformed clip entry " + juce::String(i) + "."; return false; }
        auto* object = item.getDynamicObject();
        ClipData data;
        if (!readInt(object->getProperty("id"), data.id) || data.id < 0) {
            error = "Clip entry " + juce::String(i) + " has an invalid ID.";
            return false;
        }
        for (const auto& existing : out.clips)
            if (existing.id == data.id) {
                error = "Project file declares clip ID " + juce::String(data.id) + " more than once.";
                return false;
            }
        juce::String typeName;
        if (!readString(object->getProperty("type"), typeName) ||
            (typeName != "midi" && typeName != "audio" && typeName != "pattern")) {
            error = "Clip " + juce::String(data.id) + " has an invalid type.";
            return false;
        }
        data.type = typeName == "audio" ? Clip::Type::Audio
                  : typeName == "pattern" ? Clip::Type::Pattern : Clip::Type::Midi;
        if (!readString(object->getProperty("name"), data.name)) {
            error = "Clip " + juce::String(data.id) + " has an invalid name.";
            return false;
        }
        if (!readColour(object->getProperty("colour"), data.colour)) {
            error = "Clip " + juce::String(data.id) + " has an invalid colour.";
            return false;
        }
        if (!readNumber(object->getProperty("startBeats"), data.startBeats) || data.startBeats < 0.0 ||
            data.startBeats > TransportState::maxPositionBeats) {
            error = "Clip " + juce::String(data.id) + " has an invalid start position.";
            return false;
        }
        if (!readNumber(object->getProperty("durationBeats"), data.durationBeats) ||
            data.durationBeats <= 0.0 || !std::isfinite(data.startBeats + data.durationBeats)) {
            error = "Clip " + juce::String(data.id) + " has an invalid duration.";
            return false;
        }
        if (data.type == Clip::Type::Midi) {
            if (!readBool(object->getProperty("loopEnabled"), data.loopEnabled)) {
                error = "MIDI clip " + juce::String(data.id) + " has an invalid loop flag.";
                return false;
            }
            const auto notes = object->getProperty("notes");
            if (!notes.isArray()) { error = "MIDI clip " + juce::String(data.id) + " is missing its note list."; return false; }
            for (int n = 0; n < notes.size(); ++n) {
                const auto& noteVar = notes[n];
                if (!noteVar.isObject()) { error = "MIDI clip " + juce::String(data.id) + " has a malformed note."; return false; }
                auto* noteObject = noteVar.getDynamicObject();
                Note note;
                int pitch = 0, velocity = 0, channel = 0;
                double start = 0, duration = 0;
                if (!readInt(noteObject->getProperty("pitch"), pitch) || pitch < Note::minPitch || pitch > Note::maxPitch) {
                    error = "MIDI clip " + juce::String(data.id) + " has a note with an invalid pitch.";
                    return false;
                }
                if (!readNumber(noteObject->getProperty("startBeats"), start) || start < 0.0 ||
                    !std::isfinite(start)) {
                    error = "MIDI clip " + juce::String(data.id) + " has a note with an invalid start.";
                    return false;
                }
                if (!readNumber(noteObject->getProperty("durationBeats"), duration) || duration <= 0.0 ||
                    !std::isfinite(start + duration)) {
                    error = "MIDI clip " + juce::String(data.id) + " has a note with an invalid duration.";
                    return false;
                }
                if (!readInt(noteObject->getProperty("velocity"), velocity) || velocity < Note::minVelocity ||
                    velocity > Note::maxVelocity) {
                    error = "MIDI clip " + juce::String(data.id) + " has a note with an invalid velocity.";
                    return false;
                }
                if (!readInt(noteObject->getProperty("channel"), channel) || channel < 1 || channel > 16) {
                    error = "MIDI clip " + juce::String(data.id) + " has a note with an invalid channel.";
                    return false;
                }
                note.setPitch(pitch);
                note.setStartTime(start);
                note.setDuration(duration);
                note.setVelocity(velocity);
                note.setChannel(channel);
                data.notes.push_back(note);
            }
        } else if (data.type == Clip::Type::Audio) {
            juce::String path;
            if (!readString(object->getProperty("file"), path)) {
                error = "Audio clip " + juce::String(data.id) + " is missing its file path.";
                return false;
            }
            data.audioFile = juce::File(path);
        } else {
            if (!readNumber(object->getProperty("patternLength"), data.patternLength) ||
                data.patternLength <= 0.0 || !std::isfinite(data.patternLength)) {
                error = "Pattern clip " + juce::String(data.id) + " has an invalid pattern length.";
                return false;
            }
            if (!readInt(object->getProperty("loopCount"), data.loopCount) || data.loopCount < 1) {
                error = "Pattern clip " + juce::String(data.id) + " has an invalid loop count.";
                return false;
            }
        }
        out.clips.push_back(std::move(data));
    }
    return true;
}

bool stageTracks(const juce::var& value, Staged& out, juce::String& error) {
    if (!value.isArray()) { error = "Project file is missing its track list."; return false; }
    for (int i = 0; i < value.size(); ++i) {
        const auto& item = value[i];
        if (!item.isObject()) { error = "Malformed track entry " + juce::String(i) + "."; return false; }
        auto* object = item.getDynamicObject();
        TrackData data;
        if (!readString(object->getProperty("id"), data.id) || data.id.isEmpty()) {
            error = "Track " + juce::String(i) + " has an invalid ID.";
            return false;
        }
        for (const auto& existing : out.tracks)
            if (existing.id == data.id) {
                error = "Project file declares track ID " + data.id + " more than once.";
                return false;
            }
        if (!readString(object->getProperty("name"), data.name)) {
            error = "Track " + data.id + " has an invalid name.";
            return false;
        }
        if (!readInt(object->getProperty("height"), data.height) || data.height <= 0) {
            error = "Track " + data.id + " has an invalid height.";
            return false;
        }
        if (!readColour(object->getProperty("colour"), data.colour)) {
            error = "Track " + data.id + " has an invalid colour.";
            return false;
        }
        const auto instances = object->getProperty("instances");
        if (!instances.isArray()) { error = "Track " + data.id + " is missing its instance list."; return false; }
        for (int n = 0; n < instances.size(); ++n) {
            const auto& instanceVar = instances[n];
            if (!instanceVar.isObject()) { error = "Track " + data.id + " has a malformed instance."; return false; }
            auto* instanceObject = instanceVar.getDynamicObject();
            InstanceData instance;
            if (instanceObject->hasProperty("id") && !readString(instanceObject->getProperty("id"), instance.id)) {
                error = "Track " + data.id + " has an instance with an invalid ID.";
                return false;
            }
            if (!instance.id.isEmpty())
                for (const auto& track : out.tracks)
                    for (const auto& existing : track.instances)
                        if (existing.id == instance.id) {
                            error = "Project file declares instance ID " + instance.id + " more than once.";
                            return false;
                        }
            if (!readInt(instanceObject->getProperty("clipId"), instance.clipId) || instance.clipId < 0) {
                error = "Track " + data.id + " has an instance with an invalid clip reference.";
                return false;
            }
            if (!readInt(instanceObject->getProperty("channelId"), instance.channelId) || instance.channelId < 0) {
                error = "Track " + data.id + " has an instance with an invalid channel reference.";
                return false;
            }
            if (!readNumber(instanceObject->getProperty("startBeats"), instance.startBeats) ||
                instance.startBeats < 0.0 || !std::isfinite(instance.startBeats)) {
                error = "Track " + data.id + " has an instance with an invalid start position.";
                return false;
            }
            if (!readNumber(instanceObject->getProperty("durationBeats"), instance.durationBeats) ||
                instance.durationBeats <= 0.0 || !std::isfinite(instance.startBeats + instance.durationBeats)) {
                error = "Track " + data.id + " has an instance with an invalid duration.";
                return false;
            }
            if (!readBool(instanceObject->getProperty("muted"), instance.muted)) {
                error = "Track " + data.id + " has an instance with invalid flags.";
                return false;
            }
            data.instances.push_back(std::move(instance));
        }
        out.tracks.push_back(std::move(data));
    }
    return true;
}

bool stageTransport(const juce::DynamicObject* object, TransportData& out, juce::String& error) {
    if (!readNumber(object->getProperty("tempo"), out.tempo) || out.tempo < 20.0 || out.tempo > 300.0) {
        error = "Project file has a tempo out of range 20..300.";
        return false;
    }
    if (!readInt(object->getProperty("numerator"), out.numerator) || out.numerator < 1 || out.numerator > 32) {
        error = "Project file has an invalid time signature numerator.";
        return false;
    }
    if (!readInt(object->getProperty("denominator"), out.denominator) ||
        (out.denominator != 2 && out.denominator != 4 && out.denominator != 8 && out.denominator != 16)) {
        error = "Project file has an invalid time signature denominator.";
        return false;
    }
    const auto loopVar = object->getProperty("loop");
    if (!loopVar.isObject()) { error = "Project file is missing its loop region."; return false; }
    auto* loopObject = loopVar.getDynamicObject();
    if (!readBool(loopObject->getProperty("exists"), out.loop.exists) ||
        !readBool(loopObject->getProperty("enabled"), out.loop.enabled)) {
        error = "Project file has an invalid loop region.";
        return false;
    }
    if (out.loop.exists) {
        if (!readNumber(loopObject->getProperty("startBeats"), out.loop.startBeats) ||
            !readNumber(loopObject->getProperty("endBeats"), out.loop.endBeats) ||
            !TransportState::validLoopRegion(out.loop.startBeats, out.loop.endBeats)) {
            error = "Project file has an invalid loop span.";
            return false;
        }
    } else {
        out.loop = LoopRegion{};
    }
    if (!readBool(object->getProperty("metronome"), out.metronome)) {
        error = "Project file has an invalid metronome flag.";
        return false;
    }
    return true;
}

} // namespace

bool stage(const juce::String& json, Staged& out, juce::String& error) {
    out = Staged{};
    auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject()) {
        error = "Not a valid VibeDAW project file.";
        return false;
    }
    auto* root = parsed.getDynamicObject();
    const auto versionVar = root->getProperty("formatVersion");
    int version = 0;
    if (!readInt(versionVar, version)) {
        error = "Project file is missing a valid format version.";
        return false;
    }
    if (version != currentVersion) {
        error = "Unsupported project format version " + juce::String(version) +
                " (this build supports version " + juce::String(currentVersion) + ").";
        return false;
    }
    out.version = version;
    const auto masterVar = root->getProperty("master");
    if (!masterVar.isObject()) {
        error = "Project file is missing its master settings.";
        return false;
    }
    auto* masterObject = masterVar.getDynamicObject();
    if (!readNumber(masterObject->getProperty("gain"), out.masterGain) || out.masterGain < 0.0f ||
        out.masterGain > 2.0f) {
        error = "Project file has a master gain out of range 0..2.";
        return false;
    }
    if (!readBool(masterObject->getProperty("muted"), out.masterMuted)) {
        error = "Project file has invalid master flags.";
        return false;
    }
    if (!stageChannels(root->getProperty("channels"), out, error)) return false;
    if (!stageClips(root->getProperty("clips"), out, error)) return false;
    if (!stageTracks(root->getProperty("tracks"), out, error)) return false;
    const auto transportVar = root->getProperty("transport");
    if (!transportVar.isObject()) {
        error = "Project file is missing its transport settings.";
        return false;
    }
    return stageTransport(transportVar.getDynamicObject(), out.transport, error);
}

bool writeToFile(const Project& project, const juce::File& file, juce::String& error) {
    error.clear();
    if (file.getFullPathName().isEmpty()) {
        error = "No project file was chosen.";
        return false;
    }
    const juce::TemporaryFile temp(file);
    if (!temp.getFile().replaceWithText(serialize(project))) {
        error = "Could not write to " + file.getFullPathName() + ".";
        return false;
    }
    if (!temp.overwriteTargetFileWithTemporary()) {
        error = "Could not replace " + file.getFullPathName() + ".";
        return false;
    }
    return true;
}

} // namespace ProjectDocument
} // namespace vibedaw