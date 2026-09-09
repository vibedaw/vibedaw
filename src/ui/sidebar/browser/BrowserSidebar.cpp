#include "BrowserSidebar.h"
#include "../Sidebar.h"
#include "utils/Logger.h"

namespace vibedaw {

BrowserSidebar::BrowserSidebar(PluginScanner& scanner)
    : scanner_(scanner)
{
    pluginSection_ = std::make_unique<PluginSection>(scanner_);
    pluginSection_->setListener(this);
    pluginSection_->setPluginListener(this);
    addAndMakeVisible(*pluginSection_);
    sections_.push_back(pluginSection_.get());
    
    sampleSection_ = std::make_unique<SampleSection>();
    sampleSection_->setListener(this);
    sampleSection_->setSampleListener(this);
    addAndMakeVisible(*sampleSection_);
    sections_.push_back(sampleSection_.get());
    
    presetSection_ = std::make_unique<PresetSection>();
    presetSection_->setListener(this);
    presetSection_->setPresetListener(this);
    addAndMakeVisible(*presetSection_);
    sections_.push_back(presetSection_.get());
}

BrowserSidebar::~BrowserSidebar() = default;

void BrowserSidebar::scanPlugins() {
    scanner_.scanDefaultDirectories();
}

void BrowserSidebar::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff202020));
}

void BrowserSidebar::resized() {
    updateLayout();
}

void BrowserSidebar::updateLayout() {
    auto bounds = getLocalBounds();
    
    for (auto* section : sections_) {
        int height = BrowserSection::titleBarHeight;
        
        if (section->isExpanded()) {
            height += section->getContentHeight();
        }
        
        section->setBounds(bounds.removeFromTop(height));
    }
}

void BrowserSidebar::sectionExpanded(BrowserSection* section) {
    juce::ignoreUnused(section);
    updateLayout();
}

void BrowserSidebar::sectionCollapsed(BrowserSection* section) {
    juce::ignoreUnused(section);
    updateLayout();
}

void BrowserSidebar::pluginSelected(const juce::String& pluginPath) {
    LOG_INFO("BrowserSidebar: Plugin selected: " + pluginPath);
    juce::ignoreUnused(pluginPath);
}

void BrowserSidebar::pluginDoubleClicked(const juce::String& pluginPath) {
    LOG_INFO("BrowserSidebar: Plugin double-clicked: " + pluginPath);
    
    if (listener_) {
        listener_->pluginSelectedForLoad(pluginPath);
    }
}

void BrowserSidebar::sampleSelected(const juce::File& file) {
    LOG_INFO("BrowserSidebar: Sample selected: " + file.getFullPathName());
    if (listener_) {
        listener_->sampleSelected(file);
    }
}

void BrowserSidebar::sampleDoubleClicked(const juce::File& file) {
    LOG_INFO("BrowserSidebar: Sample double-clicked: " + file.getFullPathName());
    if (listener_) {
        listener_->sampleSelected(file);
    }
}

void BrowserSidebar::presetSelected(const juce::File& file) {
    LOG_INFO("BrowserSidebar: Preset selected: " + file.getFullPathName());
    if (listener_) {
        listener_->presetSelected(file);
    }
}

void BrowserSidebar::presetDoubleClicked(const juce::File& file) {
    LOG_INFO("BrowserSidebar: Preset double-clicked: " + file.getFullPathName());
    if (listener_) {
        listener_->presetSelected(file);
    }
}

Sidebar* createBrowserSidebar(PluginScanner& scanner) {
    auto* sidebar = new Sidebar("Browser", Sidebar::Side::Left);
    sidebar->setIconSymbol(juce::String(juce::CharPointer_UTF8("\xe2\x89\xa1")));
    sidebar->setMinWidth(180);
    sidebar->setMaxWidth(400);
    sidebar->setSidebarWidth(220);
    
    auto* content = new BrowserSidebar(scanner);
    sidebar->setContent(content);
    
    return sidebar;
}

} // namespace vibedaw
