#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrowserSection.h"
#include "PluginSection.h"
#include "SampleSection.h"
#include "PresetSection.h"
#include "plugins/PluginScanner.h"
#include <memory>
#include <vector>

namespace vibedaw {
class Sidebar;
}

namespace vibedaw {

class BrowserSidebar : public juce::Component,
                       public BrowserSection::Listener,
                       public PluginSection::Listener,
                       public SampleSection::Listener,
                       public PresetSection::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void pluginSelectedForLoad(const juce::String& pluginPath) = 0;
        virtual void sampleSelected(const juce::File& file) = 0;
        virtual void presetSelected(const juce::File& file) = 0;
    };
    
    explicit BrowserSidebar(PluginScanner& scanner);
    ~BrowserSidebar() override;
    
    void setListener(Listener* listener) { listener_ = listener; }
    
    void scanPlugins();
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void sectionExpanded(BrowserSection* section) override;
    void sectionCollapsed(BrowserSection* section) override;
    
    void pluginSelected(const juce::String& pluginPath) override;
    void pluginDoubleClicked(const juce::String& pluginPath) override;
    
    void sampleSelected(const juce::File& file) override;
    void sampleDoubleClicked(const juce::File& file) override;
    
    void presetSelected(const juce::File& file) override;
    void presetDoubleClicked(const juce::File& file) override;
    
private:
    PluginScanner& scanner_;
    Listener* listener_ = nullptr;

    juce::TextEditor searchBox_;
    std::vector<BrowserSection*> sections_;
    
    std::unique_ptr<PluginSection> pluginSection_;
    std::unique_ptr<SampleSection> sampleSection_;
    std::unique_ptr<PresetSection> presetSection_;
    
    void updateLayout();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserSidebar)
};

Sidebar* createBrowserSidebar(PluginScanner& scanner);

} // namespace vibedaw
