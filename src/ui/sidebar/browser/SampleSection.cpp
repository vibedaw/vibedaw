#include "SampleSection.h"
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
    treeView_->setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff252525));
    treeView_->setColour(juce::TreeView::linesColourId, juce::Colour(0xff333333));
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
        rootItem_ = std::make_unique<SampleFileTreeItem>(rootDirectory_, true);
        rootItem_->setListener(sampleListener_);
        treeView_->setRootItem(rootItem_.get());
    }
}

SampleFileTreeItem::SampleFileTreeItem(const juce::File& file, bool isRoot)
    : file_(file), isRoot_(isRoot)
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
        if (child.isDirectory() || isAudioFile(child)) {
            auto* childItem = new SampleFileTreeItem(child);
            childItem->setListener(listener_);
            addSubItem(childItem);
        }
    }
}

} // namespace vibedaw
