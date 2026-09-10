#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace vibedaw {

// Async project-file selection; no modal loops. Offline tests install the
// interceptor instead of creating a native file browser; an unset interceptor
// uses the real FileChooser (watcher/manual only). An empty juce::File in the
// callback means the selection was cancelled.
using FileDialogHandler = std::function<void(bool forSaving, juce::Component* target,
                                             std::function<void(const juce::File&)>)>;

inline FileDialogHandler& fileDialogInterceptor() {
    static FileDialogHandler interceptor;
    return interceptor;
}

} // namespace vibedaw