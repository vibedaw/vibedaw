#include "MainContent.h"
#include "plugins/PluginHost.h"
#include "core/Constants.h"
#include "utils/Logger.h"
#include "sidebar/Sidebar.h"
#include "sidebar/SidebarContainer.h"
#include "sidebar/channel/ChannelRackSidebar.h"
#include "sidebar/browser/BrowserSidebar.h"
#include "sidebar/clips/ClipsSidebar.h"

namespace vibedaw {

MainContent::MainContent(juce::MidiKeyboardState& keyboardState, MidiManager& manager, Project& proj)
    : midiManager_(manager),
      project_(proj), transportState_(proj.getTransportState()), pluginButton_(proj)
{
    setOpaque(true);
    
    project_.addListener(this);
    project_.getClipPool().addListener(this);
    
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
    addAndMakeVisible(*rightSidebarContainer_);
    
    auto* clipsSidebar = createClipsSidebar(project_);
    auto* clipsContent = dynamic_cast<ClipsContent*>(clipsSidebar->getContent());
    if (clipsContent) {
        clipsContent->setClipsListener(this);
    }
    rightSidebarContainer_->addSidebar(clipsSidebar);
    
    panelContainer_ = std::make_unique<PanelContainer>();
    addAndMakeVisible(*panelContainer_);
    
    timelinePanel_ = new TimelinePanel(project_);
    timelinePanel_->onEditSource = [this](ClipId id) { clipOpened(id, project_.getClipPool().getClip(id)); };
    panelContainer_->addPanel(timelinePanel_);
    
    mixerPanel_ = new MixerPanel(project_);
    panelContainer_->addPanel(mixerPanel_);
    
    pianoPanel_ = new PianoPanel(keyboardState, &midiManager_);
    panelContainer_->addPanel(pianoPanel_);
    
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
    
    LOG_INFO("MainContent: Created with transport bar and sidebars");
}

MainContent::~MainContent() {
    stopTimer();
    while (!openClipEditors_.empty()) openClipEditors_.back()->closeButtonPressed();
    project_.getClipPool().removeListener(this);
    project_.removeListener(this);
    LOG_INFO("MainContent: Destroyed");
}

void MainContent::timerCallback() {
    transportState_.pollRenderPosition();
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

    int available = bounds.getWidth();
    leftSidebarContainer_->constrainTo(available);
    int leftWidth = leftSidebarContainer_->getDisplayedWidth();
    rightSidebarContainer_->constrainTo(available - leftWidth);
    int rightWidth = rightSidebarContainer_->getDisplayedWidth();

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
    auto* channel = project_.getChannelList().getChannelById(project_.getActiveChannelId());
    const auto reason = PluginButton::unavailableReason(channel);
    if (reason.isEmpty()) channel->getPlugin()->openEditorWindow();
    else juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Plugin editor unavailable", reason);
}

void MainContent::updateStatusLabel() {
    juce::String status;
    
    int activeIndex = project_.getActiveChannel();
    auto& channels = project_.getChannelList();
    
    if (activeIndex >= 0 && activeIndex < channels.getNumChannels()) {
        auto* channel = channels.getChannel(activeIndex);
        if (channel && channel->hasPlugin()) {
            status = "Ch" + juce::String(activeIndex + 1) + ": " + channel->getPlugin()->getPluginName();
        } else {
            status = "Ch" + juce::String(activeIndex + 1) + ": No plugin";
        }
    } else {
        status = "No channel selected";
    }
    
    status += " | MIDI: " + (midiManager_.isConnected() ? midiManager_.getCurrentDeviceName() : "Not connected");
    status += " | Plugin editors: initial testing (unguarded restarts)";
    
    statusLabel_.setText(status, juce::dontSendNotification);
}

void MainContent::pluginSelectedForLoad(const juce::String& pluginPath) {
    LOG_INFO("MainContent: Loading plugin from browser: " + pluginPath);
    
    if (project_.loadPlugin(pluginPath)) {
        updateStatusLabel();
    } else {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Plugin not loaded",
            "Loading failed. The audio boundary supports at most 128 channels and mono/stereo plugins.");
    }
}

void MainContent::sampleSelected(const juce::File& file) {
    LOG_INFO("MainContent: Sample selected: " + file.getFullPathName());
}

void MainContent::presetSelected(const juce::File& file) {
    LOG_INFO("MainContent: Preset selected: " + file.getFullPathName());
}

void MainContent::activeChannelChanged(int newActiveIndex) {
    juce::ignoreUnused(newActiveIndex);
    updateStatusLabel();
}

void MainContent::clipCreated(ClipId clipId, Clip* clip) {
    clipSelected(clipId, clip);
}

void MainContent::clipOpened(ClipId clipId, Clip* clip) {
    clip = project_.getClipPool().getClip(clipId);
    if (!clip) return;
    clipSelected(clipId, clip);
    for (auto* window : openClipEditors_) {
        if (window->getClipId() == clipId) {
            window->setMinimised(false);
            window->setVisible(true);
            window->toFront(true);
            return;
        }
    }
    if (clip && clip->getType() == Clip::Type::Midi) {
        auto* midiClip = dynamic_cast<MidiClip*>(clip);
        if (midiClip) {
            auto* window = new ClipEditorWindow(midiClip, clipId, &midiManager_);
            window->setListener(this);
            openClipEditors_.push_back(window);
        }
    }
}

void MainContent::clipSelected(ClipId clipId, Clip*) {
    if (timelinePanel_) timelinePanel_->setSelectedClip(clipId);
}

void MainContent::clipWillBeRemoved(ClipId id) {
    // Close while the source is still alive so grids can detach their listeners.
    for (int i = static_cast<int>(openClipEditors_.size()) - 1; i >= 0; --i)
        if (openClipEditors_[i]->getClipId() == id) openClipEditors_[i]->closeButtonPressed();
}

void MainContent::clipEditorClosed(ClipEditorWindow* window) {
    auto it = std::find(openClipEditors_.begin(), openClipEditors_.end(), window);
    if (it != openClipEditors_.end()) {
        openClipEditors_.erase(it);
    }
}

} // namespace vibedaw
