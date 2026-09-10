#include "SampleSection.h"
#include "ui/Theme.h"
#include "utils/Logger.h"

namespace vibedaw {

namespace {
    bool isAudioFile(const juce::File& file) {
        auto extension = file.getFileExtension().toLowerCase();
        static const juce::StringArray audioExtensions = {
            ".wav", ".aiff", ".aif", ".aifc", ".flac", ".mp3", ".ogg", ".m4a", ".wma"
        };
        return audioExtensions.contains(extension);
    }
}

SampleSection::SampleSection()
    : BrowserSection("Samples")
{
    treeView_ = std::make_unique<juce::TreeView>();
    treeView_->setColour(juce::TreeView::backgroundColourId, theme::raised);
    treeView_->setColour(juce::TreeView::linesColourId, theme::hairline);
    treeView_->setDefaultOpenness(false);
    treeView_->setMultiSelectEnabled(false);
    addAndMakeVisible(*treeView_);
    
    setContentHeight(150);
    
    setRootDirectory(juce::File::getSpecialLocation(juce::File::userMusicDirectory));
}

SampleSection::~SampleSection() {
    treeView_->setRootItem(nullptr);
}

void SampleSection::refresh() {
    buildTree();
}

void SampleSection::setRootDirectory(const juce::File& directory) {
    rootDirectory_ = directory;
    buildTree();
}

void SampleSection::paintContent(juce::Graphics& g, juce::Rectangle<int> bounds) {
    juce::ignoreUnused(g, bounds);
}

void SampleSection::resizedContent(juce::Rectangle<int> bounds) {
    if (treeView_) {
        treeView_->setBounds(bounds);
    }
}

void SampleSection::buildTree() {
    treeView_->setRootItem(nullptr);

    if (rootDirectory_.exists()) {
        rootItem_ = std::make_unique<SampleFileTreeItem>(rootDirectory_, true, filterText_);
        rootItem_->setListener(sampleListener_);
        treeView_->setRootItem(rootItem_.get());
    }
}

void SampleSection::applyFilter(const juce::String& filter) {
    if (filterText_ == filter) return;
    filterText_ = filter;
    buildTree();
}

SampleFileTreeItem::SampleFileTreeItem(const juce::File& file, bool isRoot, const juce::String& filter)
    : file_(file), isRoot_(isRoot), filter_(filter)
{
    if (file_.isDirectory()) {
        populateChildren();
    }
}

bool SampleFileTreeItem::mightContainSubItems() {
    return file_.isDirectory();
}

juce::String SampleFileTreeItem::getUniqueName() const {
    return file_.getFullPathName();
}

void SampleFileTreeItem::paintItem(juce::Graphics& g, int width, int height) {
    auto bounds = juce::Rectangle<int>(0, 0, width, height);
    
    if (file_.isDirectory()) {
        g.fillAll(theme::raised);
    } else {
        g.fillAll(theme::control);
    }
    
    g.setColour(theme::white);
    g.setFont(11.0f);
    
    int indent = 4;
    juce::String displayText = isRoot_ ? file_.getFullPathName() : file_.getFileName();
    g.drawText(displayText, indent, 0, width - indent - 4, height, juce::Justification::centredLeft, true);
    
    g.setColour(theme::hairline);
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
}

void SampleFileTreeItem::itemClicked(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    
    if (!file_.isDirectory() && listener_) {
        listener_->sampleSelected(file_);
    }
}

void SampleFileTreeItem::itemDoubleClicked(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    
    if (!file_.isDirectory() && listener_) {
        listener_->sampleDoubleClicked(file_);
    }
}

juce::var SampleFileTreeItem::getDragSourceDescription() {
    if (!file_.isDirectory() && isAudioFile(file_)) {
        return "sample://" + file_.getFullPathName();
    }
    return {};
}

void SampleFileTreeItem::populateChildren() {
    clearSubItems();

    auto files = file_.findChildFiles(juce::File::findFilesAndDirectories, false);
    files.sort();

    for (const auto& child : files) {
        if (child.isDirectory()) {
            if (filter_.isNotEmpty() && !subtreeMatches(child, filter_)) continue;
            auto* childItem = new SampleFileTreeItem(child, false, filter_);
            childItem->setListener(listener_);
            addSubItem(childItem);
        } else if (isAudioFile(child)) {
            if (filter_.isNotEmpty() && !child.getFileName().containsIgnoreCase(filter_)) continue;
            auto* childItem = new SampleFileTreeItem(child, false, filter_);
            childItem->setListener(listener_);
            addSubItem(childItem);
        }
    }
}

bool SampleFileTreeItem::subtreeMatches(const juce::File& directory, const juce::String& filter) {
    if (directory.getFileName().containsIgnoreCase(filter)) return true;
    for (const auto& child : directory.findChildFiles(juce::File::findFilesAndDirectories, false)) {
        if (child.isDirectory()) {
            if (subtreeMatches(child, filter)) return true;
        } else if (isAudioFile(child) && child.getFileName().containsIgnoreCase(filter)) {
            return true;
        }
    }
    return false;
}

} // namespace vibedaw
