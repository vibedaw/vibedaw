#pragma once

#include "project/Project.h"
#include "plugins/PluginHost.h"

namespace vibedaw {

// Message-thread binding; never retain a channel/host pointer across model changes.
class PluginButton : public juce::TextButton, private Project::Listener {
public:
    explicit PluginButton(Project& project) : project_(project) {
        setButtonText("Plugin");
        setEnabled(false);
        project_.addListener(this);
        activeChannelChanged(project_.getActiveChannel());
    }
    ~PluginButton() override { project_.removeListener(this); }

    static juce::String unavailableReason(const Channel* channel) {
        if (!channel) return "Select an instrument channel to configure its plugin.";
        const auto target = channel->getName() + ": ";
        if (!channel->hasPlugin() || !channel->getPlugin()->isLoaded())
            return target + "No plugin loaded. Load an instrument from the Browser.";
        if (!channel->getPlugin()->hasEditor())
            return target + channel->getPlugin()->getPluginName() + ": No supported editor.";
        return {};
    }

private:
    void activeChannelChanged(int) override {
        auto* channel = project_.getChannelList().getChannelById(project_.getActiveChannelId());
        auto reason = unavailableReason(channel);
        setEnabled(reason.isEmpty());
        setTooltip(reason.isNotEmpty() ? reason : channel->getName() + ": "
            + channel->getPlugin()->getPluginName() + ". Open editor (initial testing; plugin restarts are not guarded).");
    }
    Project& project_;
};

} // namespace vibedaw
