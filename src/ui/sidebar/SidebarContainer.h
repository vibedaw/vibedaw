#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Sidebar.h"
#include <vector>
#include <memory>

namespace vibedaw {

class SidebarTab;

class SidebarContainerListener {
public:
    virtual ~SidebarContainerListener() = default;
    virtual void sidebarContainerChanged(class SidebarContainer* container) = 0;
};

class SidebarContainer : public juce::Component, public Sidebar::Listener {
public:
    using Side = Sidebar::Side;

    explicit SidebarContainer(Side side);
    ~SidebarContainer() override;

    void addSidebar(Sidebar* sidebar);
    void insertSidebar(int index, Sidebar* sidebar);
    void removeSidebar(Sidebar* sidebar);
    void clearSidebars();

    int getSidebarCount() const;
    Sidebar* getSidebar(int index) const;
    Sidebar* getSidebarById(int id) const;

    int getTotalWidth() const;
    int getDisplayedWidth() const;
    void constrainTo(int availableWidth);
    int getExpandedCount() const;
    bool hasExpandedSidebars() const;
    bool hasCollapsedSidebars() const;
    int getTabCount() const { return static_cast<int>(tabs_.size()); }
    SidebarTab* getTab(int index) const;

    int getCollapseTabWidth() const { return Sidebar::collapseTabWidth; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void sidebarToggled(Sidebar* sidebar, bool expanded) override;
    void sidebarResized(Sidebar* sidebar, int newWidth) override;

    void setContainerListener(SidebarContainerListener* listener) { containerListener_ = listener; }

private:
    Side side_;
    std::vector<Sidebar*> sidebars_;
    std::vector<std::unique_ptr<SidebarTab>> tabs_;
    SidebarContainerListener* containerListener_ = nullptr;
    int constrainedAvailable_ = -1;

    int railWidth() const;
    int expandedTotalWidth() const;
    int displayedSidebarWidth(Sidebar* sidebar, int budget, int expandedTotal) const;
    void updateLayout();
    void updateTabs();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidebarContainer)
};

} // namespace vibedaw
