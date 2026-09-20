#include "MainContent.h"
#include "ui/Theme.h"
#include "MainWindow.h"
#include "plugins/PluginHost.h"
#include "core/Constants.h"
#include "utils/Logger.h"
#include "components/FileDialog.h"
#include "components/TextPrompt.h"
#include "components/RecordingControls.h"
#include "editor/PianoRollGeometry.h"
#include "sidebar/Sidebar.h"
#include "sidebar/SidebarContainer.h"
#include "sidebar/channel/ChannelRackSidebar.h"
#include "sidebar/browser/BrowserSidebar.h"
#include "sidebar/clips/ClipsSidebar.h"
#include <fstream>

namespace vibedaw {

MainContent::MainContent(juce::MidiKeyboardState& keyboardState, MidiManager& manager, Project& proj, AudioEngine& engine)
    : midiManager_(manager),
      project_(proj), transportState_(proj.getTransportState()), pluginButton_(proj),
      engine_(&engine)
{
    setOpaque(true);
    
    project_.addListener(this);
    project_.getClipPool().addListener(this);
    
    transport_ = std::make_unique<TransportComponent>(transportState_);
    addAndMakeVisible(*transport_);
    transport_->setRecordAction([this] {
        const juce::Component::SafePointer<MainContent> safe(this);
        auto& recorder = project_.getMidiRecorder();
        RecordingControls::ScopedRecorderCommand command(recorder);
        if (!recorder.isSessionActive()) { openRecordingSetup(); return; }
        juce::String error;
        const bool arm = !recorder.isRecording();
        const bool success = recorder.setRecording(arm, error);
        if (!safe) return;
        recordingError_ = error;
        if (success) {
            if (arm) recorder.getPlaybackTransport().setPlaying(true);
        } else if (recordingError_.isEmpty()) recordingError_ = "Recording state could not be changed.";
        if (!safe) return;
        timerCallback();
    }, [this] { openRecordingSetup(); });
    transport_->setPlaybackActions([this] { togglePlayback(); }, [this] {
        const juce::Component::SafePointer<MainContent> safe(this);
        auto& recorder = project_.getMidiRecorder();
        RecordingControls::ScopedRecorderCommand command(recorder);
        recordingError_.clear();
        if (recorder.isSessionActive()) recorder.stop();
        else transportState_.stop();
        if (!safe) return;
        timerCallback();
    });
    
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

    fileButton_.setTooltip("Project file: New, Open, Save, Save As");
    fileButton_.onClick = [this]() { showProjectMenu(); };
    addAndMakeVisible(fileButton_);

    midiLabel_.setText("MIDI:", juce::dontSendNotification);
    midiLabel_.setFont(juce::Font(11.0f, juce::Font::bold));
    midiLabel_.setColour(juce::Label::textColourId, theme::textSecondary);
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
        refreshSystemStats();
    };
    addAndMakeVisible(midiDeviceCombo_);
    
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    statusLabel_.setFont(juce::Font(11.0f));
    statusLabel_.setColour(juce::Label::textColourId, theme::textSecondary);
    updateStatusLabel();
    addAndMakeVisible(statusLabel_);

    // Live MIDI device indicator: filled dot when a device is open.
    midiDot_.setText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")), juce::dontSendNotification);
    midiDot_.setJustificationType(juce::Justification::centred);
    midiDot_.setColour(juce::Label::textColourId, midiManager_.isConnected() ? theme::accent : theme::textFaint);
    addAndMakeVisible(midiDot_);

    cpuLabel_.setJustificationType(juce::Justification::centredRight);
    cpuLabel_.setFont(juce::Font(11.0f));
    cpuLabel_.setColour(juce::Label::textColourId, theme::textSecondary);
    addAndMakeVisible(cpuLabel_);

    ramLabel_.setJustificationType(juce::Justification::centredRight);
    ramLabel_.setFont(juce::Font(11.0f));
    ramLabel_.setColour(juce::Label::textColourId, theme::textSecondary);
    addAndMakeVisible(ramLabel_);
    refreshSystemStats();
    
    setWantsKeyboardFocus(true);
    
    startTimerHz(30);
    
    LOG_INFO("MainContent: Created with transport bar and sidebars");
}

MainContent::~MainContent() {
    stopTimer();
    if (recordingPopover_) {
        recordingPopover_->setVisible(false);
        recordingPopover_->dismiss();
    }
    closeAllClipEditors();
    project_.getClipPool().removeListener(this);
    project_.removeListener(this);
    LOG_INFO("MainContent: Destroyed");
}

void MainContent::timerCallback() {
    const juce::Component::SafePointer<MainContent> safe(this);
    auto& recorder = project_.getMidiRecorder();
    RecordingControls::ScopedRecorderCommand command(recorder);
    recorder.poll();
    if (!safe) return;
    transportState_.pollRenderPosition();
    if (!safe) return;
    if (recorder.isSessionActive()) recorder.getPlaybackTransport().pollRenderPosition();
    if (!safe) return;
    transport_->setRecorderState(recorder.isSessionActive(), recorder.isRecording(),
                                recorder.isSessionActive() && recorder.getPlaybackTransport().isPlaying());
    updateStatusLabel();
    // CPU/RAM readouts are 1 Hz class, piggybacking on the 30 Hz poll timer.
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastSystemPollTime_ >= 1000.0) {
        lastSystemPollTime_ = now;
        refreshSystemStats();
    }
}

void MainContent::openRecordingSetup() {
    if (recordingPopover_) return;
    recordingError_.clear();
    auto controls = std::make_unique<RecordingControls>(project_, false, InvalidClipId, true);
    controls->setLifetimeOwner(this);
    recordingPopover_ = &juce::CallOutBox::launchAsynchronously(std::move(controls),
                                                               transport_->getScreenBounds(), nullptr);
}

void MainContent::togglePlayback() {
    auto& recorder = project_.getMidiRecorder();
    RecordingControls::ScopedRecorderCommand command(recorder);
    if (!recorder.isSessionActive()) transportState_.togglePlay();
    else if (recorder.getPlaybackTransport().isPlaying()) recorder.stop();
    else recorder.getPlaybackTransport().setPlaying(true);
}

void MainContent::refreshSystemStats() {
    if (engine_ != nullptr) {
        const double cpu = engine_->getCpuUsage();
        if (cpu >= 0.0) {
            cpuLabel_.setText("CPU " + juce::String(juce::roundToInt(cpu * 100.0)) + "%",
                              juce::dontSendNotification);
        } else {
            cpuLabel_.setText("CPU --%", juce::dontSendNotification);
        }
    } else {
        cpuLabel_.setVisible(false);
    }

#if defined(__linux__)
    std::ifstream statm("/proc/self/statm");
    long total = 0, resident = 0;
    if (statm.is_open() && (statm >> total >> resident) && resident > 0) {
        const double megabytes = static_cast<double>(resident) *
            static_cast<double>(juce::SystemStats::getPageSize()) / (1024.0 * 1024.0);
        ramLabel_.setText("RAM " + juce::String(megabytes, 0) + " MB", juce::dontSendNotification);
        ramLabel_.setVisible(true);
    } else {
        ramLabel_.setVisible(false);
    }
#else
    ramLabel_.setVisible(false);
#endif

    midiDot_.setColour(juce::Label::textColourId, midiManager_.isConnected() ? theme::accent : theme::textFaint);
}

void MainContent::paint(juce::Graphics& g) {
    g.fillAll(theme::windowBackground);
    auto bounds = getLocalBounds().reduced(workspaceGap);
    theme::drawSurface(g, bounds.removeFromBottom(statusBarHeight).toFloat(),
                       theme::raised, theme::panelRadius);
}

void MainContent::resized() {
    updateLayout();
}

void MainContent::updateLayout() {
    auto bounds = getLocalBounds().reduced(workspaceGap);

    transport_->setBounds(bounds.removeFromTop(transportBarHeight));
    bounds.removeFromTop(workspaceGap);

    auto statusBarBounds = bounds.removeFromBottom(statusBarHeight);
    bounds.removeFromBottom(workspaceGap);

    const int leftGap = leftSidebarContainer_->getTotalWidth() > 0 ? workspaceGap : 0;
    const int rightGap = rightSidebarContainer_->getTotalWidth() > 0 ? workspaceGap : 0;
    const int available = juce::jmax(0, bounds.getWidth() - leftGap - rightGap);
    // Constrain displayed widths, never the remembered sidebar preferences.
    const int sidebarBudget = juce::jmax(0, available - juce::jmin(240, available / 3));
    const int sidebarTotal = leftSidebarContainer_->getTotalWidth() + rightSidebarContainer_->getTotalWidth();
    const int leftBudget = sidebarTotal > 0
        ? static_cast<int>(static_cast<double>(sidebarBudget) * leftSidebarContainer_->getTotalWidth() / sidebarTotal)
        : 0;
    leftSidebarContainer_->constrainTo(leftBudget);
    int leftWidth = leftSidebarContainer_->getDisplayedWidth();
    rightSidebarContainer_->constrainTo(juce::jmax(0, sidebarBudget - leftWidth));
    int rightWidth = rightSidebarContainer_->getDisplayedWidth();

    if (leftWidth > 0) {
        leftSidebarContainer_->setBounds(bounds.removeFromLeft(leftWidth));
        bounds.removeFromLeft(leftGap);
    } else {
        leftSidebarContainer_->setBounds(bounds.getX(), bounds.getY(), 0, bounds.getHeight());
    }

    if (rightWidth > 0) {
        rightSidebarContainer_->setBounds(bounds.removeFromRight(rightWidth));
        bounds.removeFromRight(rightGap);
    } else {
        rightSidebarContainer_->setBounds(bounds.getRight(), bounds.getY(), 0, bounds.getHeight());
    }

    panelContainer_->setBounds(bounds);
    
    auto statusBar = statusBarBounds.reduced(8, 4);
    pluginButton_.setBounds(statusBar.removeFromRight(70));
    statusBar.removeFromRight(4);
    fileButton_.setBounds(statusBar.removeFromRight(48));
    statusBar.removeFromRight(6);
    ramLabel_.setBounds(statusBar.removeFromRight(getWidth() >= 850 ? 80 : 0));
    cpuLabel_.setBounds(statusBar.removeFromRight(getWidth() >= 700 ? 60 : 0));
    midiLabel_.setBounds(statusBar.removeFromLeft(36));
    midiDeviceCombo_.setBounds(statusBar.removeFromLeft(juce::jmin(150, juce::jmax(0, statusBar.getWidth() / 2))));
    midiDot_.setBounds(statusBar.removeFromLeft(16));
    statusBar.removeFromLeft(4);
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
        togglePlayback();
        return true;
    }
    
    if (key.getKeyCode() == juce::KeyPress::returnKey && !key.getModifiers().isAltDown()) {
        if (!project_.getMidiRecorder().isSessionActive()) transportState_.reset();
        return true;
    }

    if (key.getModifiers().isCtrlDown()) {
        const auto code = key.getKeyCode();
        if (code == 'N' || code == 'n') { actionNewProject(); return true; }
        if (code == 'O' || code == 'o') { actionOpenProject(); return true; }
        if (code == 'S' || code == 's') {
            if (key.getModifiers().isShiftDown()) actionSaveProjectAs();
            else actionSaveProject();
            return true;
        }
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
    auto& recorder = project_.getMidiRecorder();
    if (recordingError_.isNotEmpty() || recorder.isSessionActive() || recorder.hasPendingContent()) {
        auto* channel = project_.getChannelList().getChannelById(recorder.getChannelId());
        auto* clip = project_.getClipPool().getClip(recorder.getTargetClipId());
        status = recorder.getStatus();
        if (recordingError_.isNotEmpty()) status += " | " + recordingError_;
        if (recorder.isSessionActive())
            status = (channel ? channel->getName() : "Missing instrument") + " / "
                + (clip ? clip->getName() : "New MIDI clip") + ": " + status;
        statusLabel_.setText(status, juce::dontSendNotification);
        statusLabel_.setTooltip(status);
        return;
    }
    
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
    if (recorder.getStatus().isNotEmpty()) status += " | Recorder: " + recorder.getStatus();
    status += " | Plugin editors: initial testing (unguarded restarts)";
    
    statusLabel_.setText(status, juce::dontSendNotification);
    statusLabel_.setTooltip(status);
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
            // Arrangement -> source-local beat for the playhead: follow the
            // placement containing the transport position (earliest on overlap).
            auto provider = [this, clipId](double arrangementBeat, double& local) -> bool {
                auto* source = project_.getClipPool().getClip(clipId);
                if (!source) return false;
                bool found = false;
                double bestStart = 0.0;
                for (const auto& track : project_.getTrackList().getTracks()) {
                    for (const auto& instance : track->getClipInstances()) {
                        if (!instance || instance->getClipId() != clipId || !instance->isValid())
                            continue;
                        double candidate = 0.0;
                        if (sourceLocalBeat(arrangementBeat, instance->getStartTime(),
                                            instance->getDuration(), source->getDuration(), candidate)) {
                            if (!found || instance->getStartTime() < bestStart) {
                                found = true;
                                bestStart = instance->getStartTime();
                                local = candidate;
                            }
                        }
                    }
                }
                return found;
            };
            auto* window = new ClipEditorWindow(midiClip, clipId, &midiManager_,
                                                &transportState_, std::move(provider), true, &project_);
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
    std::vector<juce::Component::SafePointer<ClipEditorWindow>> closing;
    for (auto* window : openClipEditors_)
        if (window->getClipId() == id) closing.emplace_back(window);
    // Closing an owner can synchronously close other editors too.
    for (const auto& window : closing)
        if (window) window->closeButtonPressed();
}

void MainContent::clipEditorClosed(ClipEditorWindow* window) {
    auto it = std::find(openClipEditors_.begin(), openClipEditors_.end(), window);
    if (it != openClipEditors_.end()) {
        openClipEditors_.erase(it);
    }
}

void MainContent::projectDocumentChanged() {
    if (auto* window = findParentComponentOfClass<MainWindow>()) window->updateProjectTitle();
}

void MainContent::closeAllClipEditors() {
    while (!openClipEditors_.empty()) openClipEditors_.back()->closeButtonPressed();
}

void MainContent::showError(const juce::String& text) {
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Project", text);
}

void MainContent::confirmDiscardThen(std::function<void()> action) {
    if (!project_.isDirty()) {
        action();
        return;
    }
    confirmAsync("Unsaved changes", "The current project has unsaved changes. Discard them and continue?",
        "Discard", this, [safe = juce::Component::SafePointer<MainContent>(this), action = std::move(action)]() {
            if (safe != nullptr) action();
        });
}

void MainContent::showProjectMenu() {
    juce::PopupMenu menu;
    menu.addItem(1, "New (Ctrl+N)", true, false);
    menu.addItem(2, "Open... (Ctrl+O)", true, false);
    menu.addSeparator();
    menu.addItem(3, "Save (Ctrl+S)", true, false);
    menu.addItem(4, "Save As... (Ctrl+Shift+S)", true, false);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&fileButton_),
        [safe = juce::Component::SafePointer<MainContent>(this)](int result) {
            if (safe != nullptr) safe->runFileAction(result);
        });
}

void MainContent::runFileAction(int actionId) {
    switch (actionId) {
        case 1: actionNewProject(); break;
        case 2: actionOpenProject(); break;
        case 3: actionSaveProject(); break;
        case 4: actionSaveProjectAs(); break;
        default: break;
    }
}

void MainContent::actionNewProject() {
    confirmDiscardThen([safe = juce::Component::SafePointer<MainContent>(this)]() {
        if (safe == nullptr) return;
        safe->closeAllClipEditors();
        safe->project_.newProject();
        safe->updateStatusLabel();
    });
}

void MainContent::actionOpenProject() {
    chooseProjectFile(false, [safe = juce::Component::SafePointer<MainContent>(this)](const juce::File& file) {
        if (safe == nullptr || file.getFullPathName().isEmpty()) return; // Cancelled selection.
        juce::String error;
        // Parse and validate before any discard prompt: a failed load must
        // leave the current session completely untouched.
        if (!safe->project_.prepareLoad(file, error)) {
            safe->showError("The project could not be opened.\n\n" + error);
            return;
        }
        safe->confirmDiscardThen([safe]() {
            if (safe == nullptr) return;
            safe->closeAllClipEditors();
            safe->project_.commitLoad();
            safe->updateStatusLabel();
        });
    });
}

void MainContent::actionSaveProject() {
    if (project_.getProjectFile().getFullPathName().isEmpty()) {
        actionSaveProjectAs();
        return;
    }
    juce::String error;
    if (!project_.saveProject(error)) showError("The project could not be saved.\n\n" + error);
}

void MainContent::actionSaveProjectAs() {
    chooseProjectFile(true, [safe = juce::Component::SafePointer<MainContent>(this)](const juce::File& file) {
        if (safe == nullptr || file.getFullPathName().isEmpty()) return; // Cancelled selection.
        juce::String error;
        if (!safe->project_.saveProjectAs(file, error))
            safe->showError("The project could not be saved.\n\n" + error);
    });
}

void MainContent::chooseProjectFile(bool forSaving, std::function<void(const juce::File&)> onChosen) {
    if (auto& intercept = fileDialogInterceptor(); intercept) {
        intercept(forSaving, this, std::move(onChosen));
        return;
    }
    if (fileDialogActive_) return; // One pending chooser per thread; a second request is ignored.
    const auto extension = juce::String(Constants::PROJECT_FILE_EXTENSION);
    fileDialog_ = std::make_unique<juce::FileChooser>(
        forSaving ? "Save project" : "Open project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*" + extension, true);
    const int flags = juce::FileBrowserComponent::canSelectFiles |
        (forSaving ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode);
    fileDialogActive_ = true;
    fileDialog_->launchAsync(flags,
        [safe = juce::Component::SafePointer<MainContent>(this), onChosen = std::move(onChosen), extension, forSaving]
        (const juce::FileChooser& chooser) {
            if (safe != nullptr) safe->fileDialogActive_ = false;
            auto file = chooser.getResult();
            if (forSaving && file.getFullPathName().isNotEmpty() && !file.hasFileExtension(extension))
                file = file.withFileExtension(extension);
            onChosen(file);
        });
}

} // namespace vibedaw
