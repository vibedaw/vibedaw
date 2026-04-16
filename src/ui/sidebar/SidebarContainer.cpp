#include "SidebarContainer.h"
#include "SidebarTab.h"

namespace vibedaw {

SidebarContainer::SidebarContainer(Side side)
    : side_(side)
{
    setOpaque(true);
    setInterceptsMouseClicks(true, true);
}

SidebarContainer::~SidebarContainer() = default;

void SidebarContainer::addSidebar(Sidebar* sidebar) {
    if (sidebar == nullptr) return;
    
    sidebar->setListener(this);
    sidebars_.push_back(sidebar);
    addAndMakeVisible(sidebar);
    
    updateLayout();
}

void SidebarContainer::insertSidebar(int index, Sidebar* sidebar) {
    if (sidebar == nullptr) return;
    if (index < 0 || index > static_cast<int>(sidebars_.size())) {
        addSidebar(sidebar);
        return;
    }
    
    sidebar->setListener(this);
    sidebars_.insert(sidebars_.begin() + index, sidebar);
    addAndMakeVisible(sidebar, index);
    
    updateLayout();
}

void SidebarContainer::removeSidebar(Sidebar* sidebar) {
    if (sidebar == nullptr) return;
    
    sidebar->setListener(nullptr);
    removeChildComponent(sidebar);
    
    auto it = std::find(sidebars_.begin(), sidebars_.end(), sidebar);
    if (it != sidebars_.end()) {
        sidebars_.erase(it);
    }
    
    updateLayout();
}

void SidebarContainer::clearSidebars() {
    for (auto* sidebar : sidebars_) {
        sidebar->setListener(nullptr);
        removeChildComponent(sidebar);
    }
    sidebars_.clear();
    tabs_.clear();
    updateLayout();
}

int SidebarContainer::getSidebarCount() const {
    return static_cast<int>(sidebars_.size());
}

Sidebar* SidebarContainer::getSidebar(int index) const {
    if (index >= 0 && index < static_cast<int>(sidebars_.size())) {
        return sidebars_[index];
    }
    return nullptr;
}

Sidebar* SidebarContainer::getSidebarById(int id) const {
    for (auto* sidebar : sidebars_) {
        if (sidebar->getId() == id) {
            return sidebar;
        }
    }
    return nullptr;
}

int SidebarContainer::getTotalWidth() const {
    int total = 0;
    for (auto* sidebar : sidebars_) {
        if (sidebar->isExpanded()) {
            total += sidebar->getSidebarWidth();
        } else {
            total += Sidebar::collapseTabWidth;
        }
    }
    return total;
}

int SidebarContainer::getExpandedCount() const {
    int count = 0;
    for (auto* sidebar : sidebars_) {
        if (sidebar->isExpanded()) {
            count++;
        }
    }
    return count;
}

bool SidebarContainer::hasExpandedSidebars() const {
    return getExpandedCount() > 0;
}

void SidebarContainer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void SidebarContainer::resized() {
    updateLayout();
}

void SidebarContainer::sidebarToggled(Sidebar* sidebar, bool expanded) {
    juce::ignoreUnused(sidebar, expanded);
    updateLayout();
    
    if (containerListener_) {
        containerListener_->sidebarContainerChanged(this);
    }
}

void SidebarContainer::sidebarResized(Sidebar* sidebar, int newWidth) {
    juce::ignoreUnused(sidebar, newWidth);
    updateLayout();
    
    if (containerListener_) {
        containerListener_->sidebarContainerChanged(this);
    }
}

void SidebarContainer::updateLayout() {
    if (sidebars_.empty()) {
        setSize(0, getHeight());
        return;
    }
    
    updateTabs();
    
    int totalWidth = getTotalWidth();
    setSize(totalWidth, getHeight());
    
    auto bounds = getLocalBounds();
    int currentX = 0;
    
    for (auto* sidebar : sidebars_) {
        if (sidebar->isExpanded()) {
            sidebar->setVisible(true);
            sidebar->setBounds(currentX, 0, sidebar->getSidebarWidth(), getHeight());
            currentX += sidebar->getSidebarWidth();
        } else {
            sidebar->setVisible(false);
            currentX += Sidebar::collapseTabWidth;
        }
    }
    
    int tabIndex = 0;
    int tabX = 0;
    
    for (size_t i = 0; i < sidebars_.size(); ++i) {
        if (sidebars_[i]->isCollapsed()) {
            if (tabIndex < static_cast<int>(tabs_.size())) {
                auto* tab = tabs_[tabIndex].get();
                tab->setVisible(true);
                
                if (side_ == Side::Left) {
                    tab->setBounds(tabX, tabIndex * Sidebar::collapseTabWidth, 
                                   Sidebar::collapseTabWidth, Sidebar::collapseTabWidth);
                } else {
                    tab->setBounds(tabX + sidebars_[i]->getSidebarWidth() - Sidebar::collapseTabWidth, 
                                   tabIndex * Sidebar::collapseTabWidth,
                                   Sidebar::collapseTabWidth, Sidebar::collapseTabWidth);
                }
            }
            tabIndex++;
        }
        tabX += sidebars_[i]->isExpanded() ? sidebars_[i]->getSidebarWidth() : Sidebar::collapseTabWidth;
    }
    
    for (int i = tabIndex; i < static_cast<int>(tabs_.size()); ++i) {
        tabs_[i]->setVisible(false);
    }
}

void SidebarContainer::updateTabs() {
    int collapsedCount = 0;
    for (auto* sidebar : sidebars_) {
        if (sidebar->isCollapsed()) {
            collapsedCount++;
        }
    }
    
    while (static_cast<int>(tabs_.size()) < collapsedCount) {
        Sidebar* collapsedSidebar = nullptr;
        int collapsedIndex = 0;
        for (auto* s : sidebars_) {
            if (s->isCollapsed()) {
                if (collapsedIndex == static_cast<int>(tabs_.size())) {
                    collapsedSidebar = s;
                    break;
                }
                collapsedIndex++;
            }
        }
        
        if (collapsedSidebar) {
            auto tab = std::make_unique<SidebarTab>(*collapsedSidebar);
            addAndMakeVisible(tab.get());
            tabs_.push_back(std::move(tab));
        } else {
            break;
        }
    }
    
    while (static_cast<int>(tabs_.size()) > collapsedCount) {
        tabs_.pop_back();
    }
}

} // namespace vibedaw
