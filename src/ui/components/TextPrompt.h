#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace vibedaw {

// Async message-thread prompts; no modal loops. Callbacks run on the message
// thread before the window is destroyed, so handlers must re-resolve model
// objects by stable ID and treat vanished targets as harmless no-ops.
//
// Offline tests install the interceptors instead of creating native alert
// windows; unset interceptors show the real dialogs (watcher/manual only).
using TextPromptHandler = std::function<void(const juce::String& title, const juce::String& message,
                                             const juce::String& initialText, juce::Component* target,
                                             std::function<void(const juce::String&)>)>;
using ConfirmHandler = std::function<void(const juce::String& title, const juce::String& message,
                                          const juce::String& okText, juce::Component* target,
                                          std::function<void()>)>;
inline TextPromptHandler& textPromptInterceptor() {
    static TextPromptHandler interceptor;
    return interceptor;
}
inline ConfirmHandler& confirmInterceptor() {
    static ConfirmHandler interceptor;
    return interceptor;
}

inline void showTextPrompt(const juce::String& title, const juce::String& message,
                           const juce::String& initialText, juce::Component* target,
                           std::function<void(const juce::String&)> onAccept) {
    if (auto& intercept = textPromptInterceptor(); intercept) {
        intercept(title, message, initialText, target, std::move(onAccept));
        return;
    }
    auto* window = new juce::AlertWindow(title, message, juce::AlertWindow::NoIcon, target);
    window->addTextEditor("text", initialText, {});
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create(
        [window, handler = std::move(onAccept)](int result) {
            if (result != 1 || !handler) return;
            if (auto* editor = window->getTextEditor("text")) handler(editor->getText().trim());
        }), true);
}

inline void confirmAsync(const juce::String& title, const juce::String& message,
                         const juce::String& okText, juce::Component* target,
                         std::function<void()> onConfirmed) {
    if (auto& intercept = confirmInterceptor(); intercept) {
        intercept(title, message, okText, target, std::move(onConfirmed));
        return;
    }
    juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, title, message, okText, "Cancel",
        target, juce::ModalCallbackFunction::create([handler = std::move(onConfirmed)](int result) {
            if (result == 1 && handler) handler();
        }));
}

} // namespace vibedaw
