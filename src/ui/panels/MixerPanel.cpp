#include "MixerPanel.h"
#include "project/Project.h"

namespace vibedaw {

class MixerPanel::MeterTimer : public juce::Timer {
public:
    MeterTimer(MixerPanel& owner) : owner_(owner) {}
    
    void timerCallback() override {
        owner_.updateMetersFromProject();
    }
    
private:
    MixerPanel& owner_;
};

MixerPanel::MixerPanel()
    : Panel("Mixer")
{
    setPreferredHeight(200);
    setExpandedHeight(200);
    setMinHeight(100);
    setMaxHeight(400);
    
    masterStrip = std::make_unique<MasterStrip>();
    masterStrip->onVolumeChanged = [this](float v) {
        if (onMasterVolumeChanged) {
            onMasterVolumeChanged(v);
        }
    };
    addAndMakeVisible(*masterStrip);
    
    setupStrips();
    
    meterTimer = std::make_unique<MeterTimer>(*this);
    meterTimer->startTimerHz(30);
}

MixerPanel::~MixerPanel() {
    if (meterTimer) {
        meterTimer->stopTimer();
    }
}

void MixerPanel::setupStrips() {
    channelStrips.clear();
    sendStrips.clear();
    
    for (int i = 0; i < numChannels; ++i) {
        auto strip = std::make_unique<MixerStrip>(i);
        
        strip->setTrackColour(juce::Colour(0xff888888));
        strip->setTrackName("Ch " + juce::String(i + 1));
        
        strip->onVolumeChanged = [this, i](float v) {
            if (onChannelVolumeChanged) {
                onChannelVolumeChanged(i, v);
            }
        };
        
        strip->onPanChanged = [this, i](float p) {
            if (onChannelPanChanged) {
                onChannelPanChanged(i, p);
            }
        };
        
        strip->onMuteToggled = [this, i](bool m) {
            if (onChannelMuteChanged) {
                onChannelMuteChanged(i, m);
            }
        };
        
        strip->onSoloToggled = [this, i](bool s) {
            if (onChannelSoloChanged) {
                onChannelSoloChanged(i, s);
            }
        };
        
        strip->onSendLevelChanged = [this, i](int sendIdx, float level) {
            if (onSendLevelChanged) {
                onSendLevelChanged(i, sendIdx, level);
            }
        };
        
        strip->onStripSelected = [this, i]() {
            selectChannel(i);
        };
        
        addAndMakeVisible(*strip);
        channelStrips.push_back(std::move(strip));
    }
    
    for (int i = 0; i < numSends; ++i) {
        auto strip = std::make_unique<MixerStrip>(100 + i);
        
        strip->setTrackColour(juce::Colour(0xff666666));
        strip->setTrackName("Send " + juce::String(i + 1));
        strip->setStripType(MixerStrip::StripType::Send);
        strip->setShowSends(false);
        
        addAndMakeVisible(*strip);
        sendStrips.push_back(std::move(strip));
    }
}

void MixerPanel::paint(juce::Graphics& g) {
    Panel::paint(g);
    
    auto bounds = getLocalBounds();
    bounds.removeFromTop(getTitleBarHeight());
    
    g.fillAll(juce::Colour(0xff1a1a1a));
    
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(bounds);
}

void MixerPanel::resized() {
    Panel::resized();
    layoutStrips();
}

void MixerPanel::layoutStrips() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(getTitleBarHeight());
    
    int contentWidth = bounds.getWidth();
    int contentHeight = bounds.getHeight();
    
    int stripWidth = 50;
    int masterWidth = 50;
    int spacing = 1;
    int sectionSeparator = 4;
    
    int masterX = 0;
    
    int channelsStartX = masterWidth + sectionSeparator;
    int totalChannelsWidth = static_cast<int>(channelStrips.size()) * (stripWidth + spacing);
    
    int sendsStartX = channelsStartX + totalChannelsWidth + sectionSeparator;
    
    masterStrip->setBounds(masterX, bounds.getY(), masterWidth, contentHeight);
    
    int x = channelsStartX;
    for (auto& strip : channelStrips) {
        strip->setBounds(x, bounds.getY(), stripWidth, contentHeight);
        x += stripWidth + spacing;
    }
    
    x = sendsStartX;
    for (auto& strip : sendStrips) {
        strip->setBounds(x, bounds.getY(), stripWidth, contentHeight);
        x += stripWidth + spacing;
    }
}

void MixerPanel::setProject(Project* proj) {
    project = proj;
}

void MixerPanel::setNumChannels(int num) {
    numChannels = juce::jlimit(1, 64, num);
    setupStrips();
    resized();
}

void MixerPanel::setNumSends(int num) {
    numSends = juce::jlimit(0, 8, num);
    setupStrips();
    resized();
}

MixerStrip* MixerPanel::getChannelStrip(int index) {
    if (index >= 0 && index < static_cast<int>(channelStrips.size())) {
        return channelStrips[index].get();
    }
    return nullptr;
}

MixerStrip* MixerPanel::getSendStrip(int index) {
    if (index >= 0 && index < static_cast<int>(sendStrips.size())) {
        return sendStrips[index].get();
    }
    return nullptr;
}

MasterStrip* MixerPanel::getMasterStrip() {
    return masterStrip.get();
}

void MixerPanel::selectChannel(int index) {
    if (selectedChannel >= 0 && selectedChannel < static_cast<int>(channelStrips.size())) {
        channelStrips[selectedChannel]->setSelected(false);
    }
    
    selectedChannel = index;
    
    if (selectedChannel >= 0 && selectedChannel < static_cast<int>(channelStrips.size())) {
        channelStrips[selectedChannel]->setSelected(true);
    }
}

void MixerPanel::updateChannelLevels(int index, float left, float right) {
    if (auto* strip = getChannelStrip(index)) {
        strip->setLevels(left, right);
    }
}

void MixerPanel::updateMasterLevels(float left, float right) {
    if (masterStrip) {
        masterStrip->setLevels(left, right);
    }
}

void MixerPanel::updateMetersFromProject() {
    if (!project) return;
    
    auto& channels = project->getChannelList();
    if (channels.getNumChannels() > 0) {
        auto* channel = channels.getChannel(0);
        if (channel) {
            float leftLevel = channel->getLeftLevel();
            float rightLevel = channel->getRightLevel();
            updateMasterLevels(leftLevel, rightLevel);
        }
    }
}

} // namespace vibedaw
