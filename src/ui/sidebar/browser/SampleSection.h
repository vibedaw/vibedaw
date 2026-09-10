#pragma once

#include "BrowserSection.h"
#include "BrowserTreeItem.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace vibedaw {

class SampleFileTreeItem;

class SampleSection : public BrowserSection {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void sampleSelected(const juce::File& file) = 0;
        virtual void sampleDoubleClicked(const juce::File& file) = 0;
    };
    
    SampleSection();
    ~SampleSection() override;
    
    void setSampleListener(Listener* listener) { sampleListener_ = listener; }
    
    void refresh();
    void setRootDirectory(const juce::File& directory);
    
    void paintContent(juce::Graphics& g, juce::Rectangle<int> bounds) override;
    void resizedContent(juce::Rectangle<int> bounds) override;
    void applyFilter(const juce::String& filter) override;

private:
    Listener* sampleListener_ = nullptr;
    juce::String filterText_;
    
    std::unique_ptr<juce::TreeView> treeView_;
    std::unique_ptr<SampleFileTreeItem> rootItem_;
    juce::File rootDirectory_;
    
    void buildTree();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleSection)
};

class SampleFileTreeItem : public BrowserTreeItem {
public:
    SampleFileTreeItem(const juce::File& file, bool isRoot = false, const juce::String& filter = {});
    ~SampleFileTreeItem() override = default;
    
    bool mightContainSubItems() override;
    juce::String getUniqueName() const override;
    void paintItem(juce::Graphics& g, int width, int height) override;
    void itemClicked(const juce::MouseEvent& e) override;
    void itemDoubleClicked(const juce::MouseEvent& e) override;
    
    juce::var getDragSourceDescription() override;
    
    void setListener(SampleSection::Listener* listener) { listener_ = listener; }
    
    const juce::File& getFile() const { return file_; }
    bool isFile() const { return !file_.isDirectory(); }
    
private:
    juce::File file_;
    bool isRoot_;
    juce::String filter_;
    SampleSection::Listener* listener_ = nullptr;

    void populateChildren();
    static bool subtreeMatches(const juce::File& directory, const juce::String& filter);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleFileTreeItem)
};

} // namespace vibedaw
