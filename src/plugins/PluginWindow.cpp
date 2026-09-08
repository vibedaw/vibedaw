#include "PluginWindow.h"
#include "PluginHost.h"
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
    } else {
        // Also make the restriction visible to any direct window-construction caller.
        auto warning = std::make_unique<juce::Label>();
        warning->setText("Plugin editors are disabled until VST3 restart callbacks can be safely guarded.",
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
    clearContentComponent();
    if (pluginHost) pluginHost->unregisterWindow(this);
    LOG_INFO("PluginWindow: Destroyed");
}

void PluginWindow::closeButtonPressed() {
    LOG_INFO("PluginWindow: Close button pressed");
    delete this;
}

} // namespace vibedaw
