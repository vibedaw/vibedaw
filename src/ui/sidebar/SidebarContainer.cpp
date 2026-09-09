#include "SidebarContainer.h"
#include "SidebarTab.h"
#include <algorithm>

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

SidebarTab* SidebarContainer::getTab(int index) const {
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        return tabs_[index].get();
    }
    return nullptr;
}

bool SidebarContainer::hasCollapsedSidebars() const {
    for (auto* sidebar : sidebars_) {
        if (sidebar->isCollapsed()) return true;
    }
    return false;
}

int SidebarContainer::expandedTotalWidth() const {
    int total = 0;
    for (auto* sidebar : sidebars_) {
        if (sidebar->isExpanded()) {
            total += sidebar->getSidebarWidth();
        }
    }
    return total;
}

int SidebarContainer::railWidth() const {
    return hasCollapsedSidebars() ? Sidebar::collapseTabWidth : 0;
}

int SidebarContainer::getTotalWidth() const {
    return expandedTotalWidth() + railWidth();
}

int SidebarContainer::displayedSidebarWidth(Sidebar* sidebar, int budget, int expandedTotal) const {
    if (!sidebar->isExpanded()) return 0;
    if (expandedTotal <= 0 || expandedTotal <= budget) {
        return sidebar->getSidebarWidth();
    }
    return static_cast<int>((static_cast<long long>(sidebar->getSidebarWidth()) * budget) / expandedTotal);
}

int SidebarContainer::getDisplayedWidth() const {
    int rail = railWidth();
    int expandedTotal = expandedTotalWidth();
    int budget = expandedTotal;
    if (constrainedAvailable_ >= 0) {
        budget = std::min(expandedTotal, std::max(constrainedAvailable_ - rail, 0));
    }
    int displayed = rail;
    for (auto* sidebar : sidebars_) {
        displayed += displayedSidebarWidth(sidebar, budget, expandedTotal);
    }
    return displayed;
}

void SidebarContainer::constrainTo(int availableWidth) {
    constrainedAvailable_ = availableWidth;
    updateLayout();
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
        tabs_.clear();
        setSize(0, getHeight());
        return;
    }

    updateTabs();

    setSize(getDisplayedWidth(), getHeight());

    int rail = railWidth();
    auto contentBounds = getLocalBounds();
    if (side_ == Side::Left) {
        contentBounds.removeFromLeft(rail);
    } else {
        contentBounds.removeFromRight(rail);
    }

    int expandedTotal = expandedTotalWidth();
    int budget = expandedTotal;
    if (constrainedAvailable_ >= 0) {
        budget = std::min(expandedTotal, std::max(constrainedAvailable_ - rail, 0));
    }
    int currentX = 0;

    for (auto* sidebar : sidebars_) {
        if (sidebar->isExpanded()) {
            int width = displayedSidebarWidth(sidebar, budget, expandedTotal);
            sidebar->setVisible(true);
            sidebar->setBounds(contentBounds.getX() + currentX, 0, width, getHeight());
            currentX += width;
        } else {
            sidebar->setVisible(false);
        }
    }

    int tabX = (side_ == Side::Left) ? 0 : getWidth() - Sidebar::collapseTabWidth;
    int tabY = 0;
    for (auto& tab : tabs_) {
        tab->setVisible(true);
        tab->setBounds(tabX, tabY, Sidebar::collapseTabWidth, Sidebar::collapseTabWidth);
        tabY += Sidebar::collapseTabWidth;
    }
}

void SidebarContainer::updateTabs() {
    std::vector<Sidebar*> collapsed;
    for (auto* sidebar : sidebars_) {
        if (sidebar->isCollapsed()) {
            collapsed.push_back(sidebar);
        }
    }

    std::vector<std::unique_ptr<SidebarTab>> rebound;
    rebound.reserve(collapsed.size());
    for (auto* sidebar : collapsed) {
        SidebarTab* reused = nullptr;
        for (auto& tab : tabs_) {
            if (tab && &tab->sidebar() == sidebar) {
                reused = tab.release();
                break;
            }
        }
        if (reused != nullptr) {
            rebound.emplace_back(reused);
        } else {
            rebound.push_back(std::make_unique<SidebarTab>(*sidebar));
        }
    }

    tabs_ = std::move(rebound);
    for (auto& tab : tabs_) {
        addAndMakeVisible(tab.get());
    }
}

} // namespace vibedaw
