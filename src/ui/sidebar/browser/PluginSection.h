#pragma once

#include "BrowserSection.h"
#include "plugins/PluginScanner.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class PluginTreeItem;

class PluginSection : public BrowserSection {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void pluginSelected(const juce::String& pluginPath) = 0;
        virtual void pluginDoubleClicked(const juce::String& pluginPath) = 0;
    };
    
    PluginSection(PluginScanner& scanner);
    ~PluginSection() override;
    
    void setPluginListener(Listener* listener) { pluginListener_ = listener; }
    
    void refreshPlugins();
    
    void paintContent(juce::Graphics& g, juce::Rectangle<int> bounds) override;
    void resizedContent(juce::Rectangle<int> bounds) override;
    
private:
    PluginScanner& scanner_;
    Listener* pluginListener_ = nullptr;
    
    std::unique_ptr<juce::TreeView> treeView_;
    std::unique_ptr<PluginTreeItem> rootItem_;
    
    void buildTree();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSection)
};

class PluginTreeItem : public juce::TreeViewItem {
public:
    PluginTreeItem(const juce::String& name, const juce::String& path = {}, bool isPlugin = false);
    ~PluginTreeItem() override = default;
    
    bool mightContainSubItems() override;
    juce::String getUniqueName() const override;
    void paintItem(juce::Graphics& g, int width, int height) override;
    void itemClicked(const juce::MouseEvent& e) override;
    void itemDoubleClicked(const juce::MouseEvent& e) override;
    
    juce::var getDragSourceDescription() override;
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) override { return false; }
    
    void setListener(PluginSection::Listener* listener) { listener_ = listener; }
    
    const juce::String& getPath() const { return path_; }
    bool isPlugin() const { return isPlugin_; }
    
private:
    juce::String name_;
    juce::String path_;
    bool isPlugin_;
    PluginSection::Listener* listener_ = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTreeItem)
};

} // namespace vibedaw
