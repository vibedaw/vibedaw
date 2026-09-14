#pragma once

#include <juce_core/juce_core.h>
#include "plugins/builtin/InternalPluginFormat.h"

namespace vibedaw {

enum class DragSourceType { Plugin, Sample, Clip, Unknown };

struct DragDropInfo {
    DragSourceType type = DragSourceType::Unknown;
    juce::String path;
    int clipId = -1;

    // Message-thread gesture state: copies share the object across target exits/windows.
    static bool clipCancelled(const juce::var& gesture) {
        if (auto* object = gesture.getDynamicObject()) return bool(object->getProperty("cancelled"));
        return false;
    }
    static void cancelClip(const juce::var& gesture) {
        if (auto* object = gesture.getDynamicObject()) object->setProperty("cancelled", true);
    }

    static juce::var clip(int id) {
        auto* payload = new juce::DynamicObject();
        payload->setProperty("type", "vibedaw.clip");
        payload->setProperty("clipId", id);
        return juce::var(payload);
    }

    static juce::var plugin(const juce::String& path) {
        auto* payload = new juce::DynamicObject();
        payload->setProperty("type", "vibedaw.plugin");
        payload->setProperty("path", path);
        return juce::var(payload);
    }

    static DragDropInfo fromDragDescription(const juce::var& description) {
        // Never interpret an unknown object or a bare string ID/path as a plugin.
        if (auto* payload = description.getDynamicObject()) {
            const auto type = payload->getProperty("type");
            const auto path = payload->getProperty("path");
            const auto id = payload->getProperty("clipId");
            if (type.isString() && type.toString() == "vibedaw.clip" && id.isInt() && int(id) >= 0)
                return { DragSourceType::Clip, {}, int(id) };
            if (type.isString() && type.toString() == "vibedaw.plugin" &&
                path.isString() && path.toString().trim().isNotEmpty() &&
                (juce::File::isAbsolutePath(path.toString()) ||
                 InternalPluginFormat::claimsIdentifier(path.toString())))
                return { DragSourceType::Plugin, path.toString() };
        }
        // Preserve the shipped sample-file assignment gesture (not playback).
        if (description.isString() && description.toString().startsWith("sample://")) {
            auto path = description.toString().substring(9);
            if (juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile())
                return { DragSourceType::Sample, path };
        }
        return {};
    }
};

} // namespace vibedaw
