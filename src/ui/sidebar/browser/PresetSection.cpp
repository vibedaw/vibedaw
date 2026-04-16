#include "PresetSection.h"
#include "utils/Logger.h"

namespace vibedaw {

namespace {
    bool isPresetFile(const juce::File& file) {
        auto extension = file.getFileExtension().toLowerCase();
        static const juce::StringArray presetExtensions = {
            ".vstpreset", ".fxp", ".fxb", ".preset", ".patch"
        };
        return presetExtensions.contains(extension);
    }
}

PresetSection::PresetSection()
    : BrowserSection("Presets")
{
    treeView_ = std::make_unique<juce::TreeView>();
    treeView_->setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff252525));
    treeView_->setColour(juce::TreeView::linesColourId, juce::Colour(0xff333333));
    treeView_->setDefaultOpenness(false);
    treeView_->setMultiSelectEnabled(false);
    addAndMakeVisible(*treeView_);
    
    setContentHeight(100);
}

PresetSection::~PresetSection() {
    treeView_->setRootItem(nullptr);
}

void PresetSection::refresh() {
    buildTree();
}

void PresetSection::setRootDirectory(const juce::File& directory) {
    rootDirectory_ = directory;
    buildTree();
}

void PresetSection::paintContent(juce::Graphics& g, juce::Rectangle<int> bounds) {
    juce::ignoreUnused(g, bounds);
}

void PresetSection::resizedContent(juce::Rectangle<int> bounds) {
    if (treeView_) {
        treeView_->setBounds(bounds);
    }
}

void PresetSection::buildTree() {
    treeView_->setRootItem(nullptr);
    
    if (rootDirectory_.exists()) {
        rootItem_ = std::make_unique<PresetFileTreeItem>(rootDirectory_, true);
        rootItem_->setListener(presetListener_);
        treeView_->setRootItem(rootItem_.get());
    }
}

PresetFileTreeItem::PresetFileTreeItem(const juce::File& file, bool isRoot)
    : file_(file), isRoot_(isRoot)
{
    if (file_.isDirectory()) {
        populateChildren();
    }
}

bool PresetFileTreeItem::mightContainSubItems() {
    return file_.isDirectory();
}

juce::String PresetFileTreeItem::getUniqueName() const {
    return file_.getFullPathName();
}

void PresetFileTreeItem::paintItem(juce::Graphics& g, int width, int height) {
    auto bounds = juce::Rectangle<int>(0, 0, width, height);
    
    if (file_.isDirectory()) {
        g.fillAll(juce::Colour(0xff252525));
    } else {
        g.fillAll(juce::Colour(0xff2a2a2a));
    }
    
    g.setColour(juce::Colours::white);
    g.setFont(11.0f);
    
    int indent = 4;
    juce::String displayText = isRoot_ ? file_.getFullPathName() : file_.getFileName();
    g.drawText(displayText, indent, 0, width - indent - 4, height, juce::Justification::centredLeft, true);
    
    g.setColour(juce::Colour(0xff333333));
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
}

void PresetFileTreeItem::itemClicked(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    
    if (!file_.isDirectory() && listener_) {
        listener_->presetSelected(file_);
    }
}

void PresetFileTreeItem::itemDoubleClicked(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    
    if (!file_.isDirectory() && listener_) {
        listener_->presetDoubleClicked(file_);
    }
}

juce::var PresetFileTreeItem::getDragSourceDescription() {
    if (!file_.isDirectory() && isPresetFile(file_)) {
        return "preset://" + file_.getFullPathName();
    }
    return {};
}

void PresetFileTreeItem::populateChildren() {
    clearSubItems();
    
    auto files = file_.findChildFiles(juce::File::findFilesAndDirectories, false);
    files.sort();
    
    for (const auto& child : files) {
        if (child.isDirectory() || isPresetFile(child)) {
            auto* childItem = new PresetFileTreeItem(child);
            childItem->setListener(listener_);
            addSubItem(childItem);
        }
    }
}

} // namespace vibedaw
