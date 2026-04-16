#include "MainContent.h"
#include "plugins/PluginWindow.h"
#include "plugins/PluginHost.h"
#include "core/Constants.h"
#include "utils/Logger.h"

namespace vibedaw {

MainContent::MainContent(juce::MidiKeyboardState& keyboardState, MidiManager& manager, Project& proj)
    : midiManager_(manager),
      project_(proj)
{
    transport_ = std::make_unique<TransportComponent>();
    addAndMakeVisible(*transport_);
    
    panelContainer_ = std::make_unique<PanelContainer>();
    addAndMakeVisible(*panelContainer_);
    
    timelinePanel_ = new TimelinePanel(project_);
    panelContainer_->addPanel(timelinePanel_);
    
    mixerPanel_ = new MixerPanel();
    panelContainer_->addPanel(mixerPanel_);
    
    pianoPanel_ = new PianoPanel(keyboardState, &midiManager_);
    panelContainer_->addPanel(pianoPanel_);
    
    pluginButton_.setButtonText("Open Plugin");
    pluginButton_.onClick = [this]() {
        openPluginWindow();
    };
    addAndMakeVisible(pluginButton_);
    
    midiLabel_.setText("MIDI Input:", juce::dontSendNotification);
    addAndMakeVisible(midiLabel_);
    
    midiDeviceCombo_.addItem("None", 1);
    auto devices = midiManager_.getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i) {
        midiDeviceCombo_.addItem(devices[i], i + 2);
    }
    midiDeviceCombo_.setSelectedId(1, juce::dontSendNotification);
    midiDeviceCombo_.onChange = [this, devices]() {
        int selectedId = midiDeviceCombo_.getSelectedId();
        if (selectedId == 1) {
            midiManager_.disconnect();
        } else {
            int deviceIndex = selectedId - 2;
            if (deviceIndex >= 0 && deviceIndex < devices.size()) {
                midiManager_.connectToDevice(devices[deviceIndex]);
                project_.getSettings().midiInputDevice = devices[deviceIndex];
            }
        }
        updateStatusLabel();
    };
    addAndMakeVisible(midiDeviceCombo_);
    
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    updateStatusLabel();
    addAndMakeVisible(statusLabel_);
    
    setWantsKeyboardFocus(true);
    
    LOG_INFO("MainContent: Created with panel system");
}

MainContent::~MainContent() {
    LOG_INFO("MainContent: Destroyed");
}

void MainContent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void MainContent::resized() {
    auto bounds = getLocalBounds();
    
    auto topArea = bounds.removeFromTop(topBarHeight).reduced(10, 5);
    transport_->setBounds(topArea.removeFromLeft(250));
    
    midiLabel_.setBounds(topArea.removeFromLeft(80).withTrimmedTop(5));
    midiDeviceCombo_.setBounds(topArea.removeFromLeft(200));
    
    pluginButton_.setBounds(topArea.removeFromRight(120));
    
    statusLabel_.setBounds(bounds.removeFromBottom(30).reduced(10, 0));
    
    panelContainer_->setBounds(bounds);
}

bool MainContent::keyPressed(const juce::KeyPress& key) {
    return handleKeyPress(key);
}

bool MainContent::handleKeyPress(const juce::KeyPress& key) {
    if ((key.getKeyCode() == 'P' || key.getKeyCode() == 'p') && key.getModifiers().isCtrlDown()) {
        pianoPanel_->toggleCollapsed();
        return true;
    }
    if ((key.getKeyCode() == 'M' || key.getKeyCode() == 'm') && key.getModifiers().isCtrlDown()) {
        mixerPanel_->toggleCollapsed();
        return true;
    }
    return false;
}

void MainContent::openPluginWindow() {
    auto* track = project_.getMasterTrack();
    if (track && track->getPlugin()) {
        auto* pluginHost = track->getPlugin();
        if (pluginHost->hasEditor()) {
            new PluginWindow(pluginHost, pluginHost->getPluginName());
        } else {
            LOG_WARN("MainContent: Plugin has no editor");
        }
    } else {
        LOG_WARN("MainContent: No plugin loaded");
    }
}

void MainContent::updateStatusLabel() {
    juce::String status;
    
    auto* track = project_.getMasterTrack();
    if (track && track->hasPlugin()) {
        status = "Plugin: " + track->getPlugin()->getPluginName();
    } else {
        status = "Plugin: None";
    }
    
    status += " | MIDI: " + (midiManager_.isConnected() ? midiManager_.getCurrentDeviceName() : "Not connected");
    
    statusLabel_.setText(status, juce::dontSendNotification);
}

} // namespace vibedaw
