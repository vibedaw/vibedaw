#include "MainContent.h"
#include "plugins/PluginWindow.h"
#include "plugins/PluginHost.h"
#include "core/Constants.h"
#include "utils/Logger.h"
#include "sidebar/Sidebar.h"
#include "sidebar/SidebarContainer.h"
#include "sidebar/channel/ChannelRackSidebar.h"
#include "sidebar/browser/BrowserSidebar.h"

namespace vibedaw {

MainContent::MainContent(juce::MidiKeyboardState& keyboardState, MidiManager& manager, Project& proj)
    : midiManager_(manager),
      project_(proj)
{
    setOpaque(true);
    
    transport_ = std::make_unique<TransportComponent>(transportState_);
    addAndMakeVisible(*transport_);
    
    leftSidebarContainer_ = std::make_unique<SidebarContainer>(Sidebar::Side::Left);
    leftSidebarContainer_->setContainerListener(this);
    addAndMakeVisible(*leftSidebarContainer_);
    
    auto* browserSidebar = createBrowserSidebar(pluginScanner_);
    browserSidebar->getContent();
    auto* browserContent = dynamic_cast<BrowserSidebar*>(browserSidebar->getContent());
    if (browserContent) {
        browserContent->setListener(this);
    }
    leftSidebarContainer_->addSidebar(browserSidebar);
    
    auto* channelRack = createChannelRackSidebar(project_);
    leftSidebarContainer_->addSidebar(channelRack);
    
    pluginScanner_.scanDefaultDirectories();
    
    rightSidebarContainer_ = std::make_unique<SidebarContainer>(Sidebar::Side::Right);
    rightSidebarContainer_->setContainerListener(this);
    addChildComponent(*rightSidebarContainer_);
    
    panelContainer_ = std::make_unique<PanelContainer>();
    addAndMakeVisible(*panelContainer_);
    
    timelinePanel_ = new TimelinePanel(project_);
    panelContainer_->addPanel(timelinePanel_);
    
    mixerPanel_ = new MixerPanel();
    panelContainer_->addPanel(mixerPanel_);
    
    pianoPanel_ = new PianoPanel(keyboardState, &midiManager_);
    panelContainer_->addPanel(pianoPanel_);
    
    pluginButton_.setButtonText("Plugin");
    pluginButton_.onClick = [this]() {
        openPluginWindow();
    };
    addAndMakeVisible(pluginButton_);
    
    midiLabel_.setText("MIDI:", juce::dontSendNotification);
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
    
    startTimerHz(30);
    lastUpdateTime_ = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    
    LOG_INFO("MainContent: Created with transport bar and sidebars");
}

MainContent::~MainContent() {
    stopTimer();
    LOG_INFO("MainContent: Destroyed");
}

void MainContent::timerCallback() {
    if (transportState_.isPlaying()) {
        double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double elapsed = now - lastUpdateTime_;
        lastUpdateTime_ = now;
        
        double newPos = transportState_.getPosition() + elapsed;
        transportState_.setPosition(newPos);
    } else {
        lastUpdateTime_ = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    }
}

void MainContent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void MainContent::resized() {
    updateLayout();
}

void MainContent::updateLayout() {
    auto bounds = getLocalBounds();
    
    transport_->setBounds(bounds.removeFromTop(transportBarHeight));
    
    auto statusBarBounds = bounds.removeFromBottom(statusBarHeight);
    
    int leftWidth = leftSidebarContainer_->getTotalWidth();
    int rightWidth = rightSidebarContainer_->getTotalWidth();
    
    if (leftWidth > 0) {
        leftSidebarContainer_->setBounds(bounds.removeFromLeft(leftWidth));
    } else {
        leftSidebarContainer_->setBounds(0, transportBarHeight, 0, bounds.getHeight());
    }
    
    if (rightWidth > 0) {
        rightSidebarContainer_->setBounds(bounds.removeFromRight(rightWidth));
    } else {
        rightSidebarContainer_->setBounds(getWidth(), transportBarHeight, 0, bounds.getHeight());
    }
    
    panelContainer_->setBounds(bounds);
    
    auto statusBar = statusBarBounds.reduced(10, 2);
    midiLabel_.setBounds(statusBar.removeFromLeft(40));
    midiDeviceCombo_.setBounds(statusBar.removeFromLeft(150));
    pluginButton_.setBounds(statusBar.removeFromRight(70));
    statusLabel_.setBounds(statusBar);
}

void MainContent::sidebarContainerChanged(SidebarContainer* container) {
    juce::ignoreUnused(container);
    updateLayout();
}

bool MainContent::keyPressed(const juce::KeyPress& key) {
    return handleKeyPress(key);
}

bool MainContent::handleKeyPress(const juce::KeyPress& key) {
    if (key.getKeyCode() == ' ' && !key.getModifiers().isCtrlDown()) {
        transportState_.togglePlay();
        return true;
    }
    
    if (key.getKeyCode() == juce::KeyPress::returnKey && !key.getModifiers().isAltDown()) {
        transportState_.reset();
        return true;
    }
    
    auto* focused = panelContainer_->getFocusedPanel();
    double now = juce::Time::getMillisecondCounterHiRes();
    
    if ((key.getKeyCode() == 'P' || key.getKeyCode() == 'p') && key.getModifiers().isCtrlDown()) {
        handlePanelFocusHotkey(2, now);
        return true;
    }
    if ((key.getKeyCode() == 'M' || key.getKeyCode() == 'm') && key.getModifiers().isCtrlDown()) {
        handlePanelFocusHotkey(1, now);
        return true;
    }
    if ((key.getKeyCode() == 'T' || key.getKeyCode() == 't') && key.getModifiers().isCtrlDown()) {
        handlePanelFocusHotkey(0, now);
        return true;
    }
    
    if ((key.getKeyCode() == 'B' || key.getKeyCode() == 'b') && key.getModifiers().isCtrlDown()) {
        if (leftSidebarContainer_->getSidebarCount() > 0) {
            auto* sidebar = leftSidebarContainer_->getSidebar(0);
            if (sidebar) {
                sidebar->toggle();
                updateLayout();
            }
        }
        return true;
    }
    
    if (key.getModifiers().isShiftDown()) {
        if (key.getKeyCode() == '-' || key.getKeyCode() == juce::KeyPress::numberPadSubtract) {
            if (focused) {
                if (focused->getWindowState() == PanelWindowState::Maximized) {
                    focused->restore();
                } else if (focused->getWindowState() == PanelWindowState::Restored) {
                    focused->minimize();
                }
            }
            return true;
        }
        if (key.getKeyCode() == '+' || key.getKeyCode() == '=' || key.getKeyCode() == juce::KeyPress::numberPadAdd) {
            if (focused) {
                if (focused->getWindowState() == PanelWindowState::Minimized) {
                    focused->restore();
                } else if (focused->getWindowState() == PanelWindowState::Restored) {
                    focused->maximize();
                } else if (focused->getWindowState() == PanelWindowState::Maximized) {
                    focused->restore();
                }
            }
            return true;
        }
    }
    
    if (key.getKeyCode() == '-' || key.getKeyCode() == juce::KeyPress::numberPadSubtract) {
        panelContainer_->resizeFocusedPanel(-30);
        return true;
    }
    if (key.getKeyCode() == '+' || key.getKeyCode() == '=' || key.getKeyCode() == juce::KeyPress::numberPadAdd) {
        panelContainer_->resizeFocusedPanel(30);
        return true;
    }
    return false;
}

void MainContent::handlePanelFocusHotkey(int panelIndex, double currentTime) {
    bool isDoubleTap = (lastFocusedPanelIndex_ == panelIndex && 
                        (currentTime - lastPanelFocusTime_) < doubleTapIntervalMs_);
    
    auto* panel = panelContainer_->getPanel(panelIndex);
    
    if (isDoubleTap && panel) {
        if (panel->getWindowState() == PanelWindowState::Minimized) {
            panel->restore();
        } else {
            panel->minimize();
        }
        lastFocusedPanelIndex_ = -1;
        lastPanelFocusTime_ = 0;
    } else {
        panelContainer_->focusPanelByIndex(panelIndex);
        if (panel && panel->getWindowState() == PanelWindowState::Minimized) {
            panel->restore();
        }
        lastPanelFocusTime_ = currentTime;
        lastFocusedPanelIndex_ = panelIndex;
    }
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

void MainContent::pluginSelectedForLoad(const juce::String& pluginPath) {
    LOG_INFO("MainContent: Loading plugin from browser: " + pluginPath);
    
    if (project_.loadPlugin(pluginPath)) {
        updateStatusLabel();
    }
}

void MainContent::sampleSelected(const juce::File& file) {
    LOG_INFO("MainContent: Sample selected: " + file.getFullPathName());
}

void MainContent::presetSelected(const juce::File& file) {
    LOG_INFO("MainContent: Preset selected: " + file.getFullPathName());
}

} // namespace vibedaw
