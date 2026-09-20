#include "SidebarContainer.h"
#include "ui/Theme.h"
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
    transitions_.erase(std::remove_if(transitions_.begin(), transitions_.end(),
        [sidebar](const auto& transition) { return transition.sidebar == sidebar; }), transitions_.end());
    sidebar->setAlpha(1.0f);
    removeChildComponent(sidebar);

    auto it = std::find(sidebars_.begin(), sidebars_.end(), sidebar);
    if (it != sidebars_.end()) {
        sidebars_.erase(it);
    }

    layoutChanged();
}

void SidebarContainer::clearSidebars() {
    transitions_.clear();
    for (auto* sidebar : sidebars_) {
        sidebar->setListener(nullptr);
        sidebar->setAlpha(1.0f);
        removeChildComponent(sidebar);
    }
    sidebars_.clear();
    tabs_.clear();
    layoutChanged();
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
        total += presentationWidth(sidebar);
    }
    return total;
}

int SidebarContainer::railWidth() const {
    float fraction = 0.0f;
    for (auto* sidebar : sidebars_)
        fraction = std::max(fraction, 1.0f - widthFraction(sidebar));
    return juce::roundToInt(Sidebar::collapseTabWidth * fraction);
}

int SidebarContainer::getTotalWidth() const {
    return expandedTotalWidth() + railWidth();
}

int SidebarContainer::displayedSidebarWidth(Sidebar* sidebar, int budget, int expandedTotal) const {
    const int width = presentationWidth(sidebar);
    if (expandedTotal <= 0 || expandedTotal <= budget) {
        return width;
    }
    return static_cast<int>((static_cast<long long>(width) * budget) / expandedTotal);
}

const SidebarContainer::Transition* SidebarContainer::transitionFor(const Sidebar* sidebar) const {
    for (const auto& transition : transitions_)
        if (transition.sidebar == sidebar) return &transition;
    return nullptr;
}

float SidebarContainer::widthFraction(const Sidebar* sidebar) const {
    if (const auto* transition = transitionFor(sidebar)) return transition->width;
    return sidebar->isExpanded() ? 1.0f : 0.0f;
}

int SidebarContainer::presentationWidth(const Sidebar* sidebar) const {
    return juce::roundToInt(sidebar->getSidebarWidth() * widthFraction(sidebar));
}

bool SidebarContainer::showsTab(const Sidebar* sidebar) const {
    if (const auto* transition = transitionFor(sidebar)) return transition->showTab;
    return sidebar->isCollapsed();
}

void SidebarContainer::setAnimationsEnabled(bool enabled) {
    animationsEnabled_ = enabled;
    if (!enabled && !transitions_.empty()) {
        transitions_.clear();
        layoutChanged();
    }
}

void SidebarContainer::advanceAnimation(double now) {
    if (transitions_.empty()) return;
    const int previousRail = railWidth();
    bool geometryChanged = false, tabsChanged = false;
    std::vector<Sidebar*> closed;
    for (auto& transition : transitions_) {
        const int previousWidth = presentationWidth(transition.sidebar);
        const bool previousTab = transition.showTab;
        const double elapsed = now - transition.started;
        const auto progress = [elapsed](double start, double duration) {
            const float t = static_cast<float>(juce::jlimit(0.0, 1.0,
                (elapsed - start) / duration));
            return t * t * (3.0f - 2.0f * t);
        };
        if (transition.expanding) {
            transition.width = transition.initialWidth + (1.0f - transition.initialWidth)
                * progress(0.0, reflowDurationMs);
            transition.alpha = transition.initialAlpha + (1.0f - transition.initialAlpha)
                * progress(reflowDurationMs, fadeDurationMs);
        } else {
            transition.alpha = transition.initialAlpha * (1.0f - progress(0.0, fadeDurationMs));
            transition.width = transition.initialWidth * (1.0f - progress(fadeDurationMs, reflowDurationMs));
            transition.showTab = transition.showTab || elapsed >= fadeDurationMs;
            if (elapsed >= fadeDurationMs + reflowDurationMs) closed.push_back(transition.sidebar);
        }
        geometryChanged |= previousWidth != presentationWidth(transition.sidebar);
        tabsChanged |= previousTab != transition.showTab;
    }
    transitions_.erase(std::remove_if(transitions_.begin(), transitions_.end(),
        [now](const auto& transition) { return now - transition.started >= fadeDurationMs + reflowDurationMs; }),
        transitions_.end());
    if (geometryChanged || previousRail != railWidth()) layoutChanged();
    else if (tabsChanged) updateLayout();
    else updatePresentation();
    // Pulse only once the full restore button is visible, not while its rail is growing.
    for (auto& tab : tabs_)
        if (std::find(closed.begin(), closed.end(), &tab->sidebar()) != closed.end()) tab->flash();
}

void SidebarContainer::layoutChanged() {
    // Let the workspace apply its width budget before laying out children, once.
    layoutDirty_ = true;
    if (containerListener_) containerListener_->sidebarContainerChanged(this);
    if (layoutDirty_) updateLayout();
}

void SidebarContainer::updatePresentation() {
    for (auto* sidebar : sidebars_) {
        const auto* transition = transitionFor(sidebar);
        const float alpha = transition ? transition->alpha : 1.0f;
        sidebar->setAlpha(alpha);
        sidebar->setVisible(presentationWidth(sidebar) > 0 && sidebar->getWidth() > 0 && alpha > 0.0f);
    }
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
    if (constrainedAvailable_ == availableWidth && !layoutDirty_) return;
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
    g.fillAll(theme::windowBackground);
}

void SidebarContainer::resized() {
    updateLayout();
}

void SidebarContainer::sidebarToggled(Sidebar* sidebar, bool expanded) {
    if (animationsEnabled_) {
        // The model already holds the target state. Start from the previous presentation,
        // including partial progress when a shortcut reverses an in-flight transition.
        const auto* previous = transitionFor(sidebar);
        const float width = previous ? previous->width : (expanded ? 0.0f : 1.0f);
        const float alpha = previous ? previous->alpha : (expanded ? 0.0f : 1.0f);
        const bool showTab = !expanded && previous && previous->showTab;
        transitions_.erase(std::remove_if(transitions_.begin(), transitions_.end(),
            [sidebar](const auto& transition) { return transition.sidebar == sidebar; }), transitions_.end());
        transitions_.push_back({ sidebar, expanded, juce::Time::getMillisecondCounterHiRes(),
                                 width, alpha, width, alpha, showTab });
    }
    layoutChanged();
}

void SidebarContainer::sidebarResized(Sidebar* sidebar, int newWidth) {
    juce::ignoreUnused(sidebar, newWidth);
    layoutChanged();
}

void SidebarContainer::updateLayout() {
    if (updatingLayout_) return;
    const juce::ScopedValueSetter<bool> guard(updatingLayout_, true);
    layoutDirty_ = false;
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
        const int width = displayedSidebarWidth(sidebar, budget, expandedTotal);
        sidebar->setBounds(contentBounds.getX() + currentX, 0, width, getHeight());
        currentX += width;
    }
    updatePresentation();

    int tabX = (side_ == Side::Left) ? 0 : getWidth() - Sidebar::collapseTabWidth;
    int tabY = 0;
    for (auto& tab : tabs_) {
        // A full-size button must not paint or receive clicks over an adjacent panel
        // while its rail is still growing.
        tab->setVisible(rail == Sidebar::collapseTabWidth);
        tab->setBounds(tabX, tabY, Sidebar::collapseTabWidth, Sidebar::collapseTabWidth);
        tabY += Sidebar::collapseTabWidth;
    }
}

void SidebarContainer::updateTabs() {
    size_t index = 0;
    bool unchanged = true;
    for (auto* sidebar : sidebars_) {
        if (!showsTab(sidebar)) continue;
        if (index >= tabs_.size() || &tabs_[index]->sidebar() != sidebar) unchanged = false;
        ++index;
    }
    if (unchanged && index == tabs_.size()) return;

    std::vector<Sidebar*> collapsed;
    for (auto* sidebar : sidebars_) {
        if (showsTab(sidebar)) {
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
        addChildComponent(tab.get());
    }
}

} // namespace vibedaw
