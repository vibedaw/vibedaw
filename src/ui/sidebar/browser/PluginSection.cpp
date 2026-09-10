#include "PluginSection.h"
#include "ui/Theme.h"
#include "ui/DragPayload.h"
#include "utils/Logger.h"

namespace vibedaw {

PluginSection::PluginSection(PluginScanner& scanner)
    : BrowserSection("Plugins"),
      scanner_(scanner)
{
    treeView_ = std::make_unique<juce::TreeView>();
    treeView_->setColour(juce::TreeView::backgroundColourId, theme::browserBackground);
    treeView_->setColour(juce::TreeView::linesColourId, theme::textSecondary);
    treeView_->setIndentSize(14);
    treeView_->setDefaultOpenness(true);
    treeView_->setMultiSelectEnabled(false);
    addAndMakeVisible(*treeView_);
    
    setContentHeight(200);
    
    scanner_.setCompleteCallback([this]() {
        juce::MessageManager::callAsync([this]() {
            buildTree();
        });
    });
    
    buildTree();
}

PluginSection::~PluginSection() {
    treeView_->setRootItem(nullptr);
}

void PluginSection::refreshPlugins() {
    scanner_.scanDefaultDirectories();
}

void PluginSection::createChannelFromPlugin(const juce::String& pluginPath) {
    // Same validated path as a double-click/drop; no channel is created on failure.
    if (pluginListener_) pluginListener_->pluginDoubleClicked(pluginPath);
}

void PluginSection::paintContent(juce::Graphics& g, juce::Rectangle<int> bounds) {
    juce::ignoreUnused(g, bounds);
}

void PluginSection::resizedContent(juce::Rectangle<int> bounds) {
    if (treeView_) {
        treeView_->setVisible(isExpanded());
        treeView_->setBounds(bounds);
    }
}

void PluginSection::buildTree() {
    treeView_->setRootItem(nullptr);

    rootItem_ = std::make_unique<PluginTreeItem>("Plugins");
    rootItem_->setListener(pluginListener_);

    int shown = 0;
    for (const auto& plugin : scanner_.getScannedPlugins()) {
        if (filterText_.isNotEmpty() && !plugin.name.containsIgnoreCase(filterText_)) continue;
        auto* pluginItem = new PluginTreeItem(plugin.name, plugin.path, true);
        pluginItem->setListener(pluginListener_);
        pluginItem->setOwnerSection(this);
        rootItem_->addSubItem(pluginItem);
        ++shown;
    }

    if (shown == 0) {
        auto* emptyItem = new PluginTreeItem(filterText_.isEmpty() ? "No plugins found" : "No matches");
        emptyItem->setListener(pluginListener_);
        rootItem_->addSubItem(emptyItem);
    }

    treeView_->setRootItem(rootItem_.get());
}

void PluginSection::applyFilter(const juce::String& filter) {
    if (filterText_ == filter) return;
    filterText_ = filter;
    buildTree();
}

PluginTreeItem::PluginTreeItem(const juce::String& name, const juce::String& path, bool isPlugin)
    : name_(name), path_(path), isPlugin_(isPlugin)
{
}

bool PluginTreeItem::mightContainSubItems() {
    return !isPlugin_;
}

juce::String PluginTreeItem::getUniqueName() const {
    return name_ + "_" + path_;
}

void PluginTreeItem::paintItem(juce::Graphics& g, int width, int height) {
    paintRow(g, width, height, name_, isPlugin_ ? Icon::Plugin
        : (getNumSubItems() > 0 ? Icon::Folder : Icon::Empty));
}

void PluginTreeItem::itemClicked(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        if (isPlugin_) showCreateMenu();
        return;
    }

    if (isPlugin_ && listener_) {
        listener_->pluginSelected(path_);
    }
}

void PluginTreeItem::showCreateMenu() {
    juce::PopupMenu menu;
    // The delayed action re-resolves the owner; a destroyed section is a no-op.
    menu.addItem("Create Instrument Channel", true, false,
        [section = owner_, path = path_] {
            if (section != nullptr) section->createChannelFromPlugin(path);
        });
    juce::PopupMenu::Options options;
    if (owner_ != nullptr) options = options.withTargetComponent(owner_.getComponent());
    menu.showMenuAsync(options);
}

void PluginTreeItem::itemDoubleClicked(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    
    if (isPlugin_ && listener_) {
        listener_->pluginDoubleClicked(path_);
    }
}

juce::var PluginTreeItem::getDragSourceDescription() {
    if (isPlugin_) {
        return DragDropInfo::plugin(path_);
    }
    return {};
}

} // namespace vibedaw
