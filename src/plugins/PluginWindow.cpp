#include "PluginWindow.h"
#include "PluginHost.h"
#include "core/AudioBoundary.h"
#include "utils/Logger.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

PluginWindow::PluginWindow(PluginHost* host, const juce::String& title)
    : DocumentWindow(title, juce::Colours::darkgrey, DocumentWindow::closeButton, true),
      pluginHost(host)
{
    LOG_INFO("PluginWindow: Creating window for: " + title);
    if (host) host->registerWindow(this);
    
    if (host != nullptr && host->hasEditor()) {
        auto editor = host->createEditor();
        if (editor != nullptr) {
            setContentOwned(editor.release(), true);
        }
    }
    if (getContentComponent() == nullptr) {
        auto warning = std::make_unique<juce::Label>();
        warning->setText("This plugin could not create an editor.",
                         juce::dontSendNotification);
        warning->setSize(420, 80);
        setContentOwned(warning.release(), true);
    }
    
    setResizable(true, true);
    setUsingNativeTitleBar(true);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
    
    LOG_INFO("PluginWindow: Window created and visible");
}

PluginWindow::~PluginWindow() {
    AudioQuiescence::Edit edit;
    if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(getContentComponent()))
        editor->getAudioProcessor()->editorBeingDeleted(editor);
    clearContentComponent();
    if (pluginHost) pluginHost->unregisterWindow(this);
    LOG_INFO("PluginWindow: Destroyed");
}

void PluginWindow::closeButtonPressed() {
    AudioQuiescence::Edit edit;
    LOG_INFO("PluginWindow: Close button pressed");
    delete this;
}

} // namespace vibedaw
