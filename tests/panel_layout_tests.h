#pragma once

#include "ui/panels/PianoPanel.h"
#include "ui/panels/TimelinePanel.h"
#include "ui/PianoComponent.h"

// Include after CHECK; invoke with JUCE initialized and the test HOME installed.
// These tests create no native peers and do not dispatch animation timers.
namespace panel_layout_test {

struct BoundsListener : juce::ComponentListener {
    explicit BoundsListener(juce::Component& value) : component(value) {
        component.addComponentListener(this);
    }
    ~BoundsListener() override { component.removeComponentListener(this); }
    void componentMovedOrResized(juce::Component&, bool, bool wasResized) override {
        ++changes;
        if (wasResized) ++resizes;
    }
    juce::Component& component;
    int changes = 0, resizes = 0;
};

} // namespace panel_layout_test

inline void panelLayoutTests() {
    using namespace vibedaw;
    using panel_layout_test::BoundsListener;
    {
        Panel panel("Default content");
        panel.setContentComponent(std::make_unique<juce::Component>());
        auto* content = panel.getContentComponent();
        panel.setSize(800, 150);
        CHECK(content->getBounds() == juce::Rectangle<int>(4, 24, 792, 122));
        panel.setCollapsed(true, false);
        CHECK(!content->isVisible());
        panel.setCollapsed(false, false);
        CHECK(content->isVisible());
        CHECK(content->getBounds() == juce::Rectangle<int>(4, 24, 792, 122));
    }
    {
        juce::MidiKeyboardState keyboard;
        PianoPanel panel(keyboard, nullptr);
        panel.setSize(800, 150);
        auto* piano = dynamic_cast<PianoComponent*>(panel.getContentComponent());
        CHECK(piano != nullptr);
        BoundsListener listener(*piano);
        const auto expectedBounds = [&] {
            auto bounds = panel.getLocalBounds();
            bounds.removeFromTop(panel.getTitleBarHeight());
            bounds = bounds.reduced(4, 0).withTrimmedBottom(4);
            bounds.removeFromLeft(150);
            const int keyboardWidth = static_cast<int>(piano->getTotalKeyboardWidth());
            const int inset = juce::jmax(10, (bounds.getWidth() - keyboardWidth) / 2);
            return bounds.withTrimmedLeft(inset).withTrimmedRight(inset);
        };
        CHECK(piano->getBounds() == expectedBounds());
        for (int width : {780, 760, 740, 760, 780, 800}) {
            listener.changes = listener.resizes = 0;
            panel.setSize(width, 150);
            CHECK(listener.changes == 1 && listener.resizes == 1);
            CHECK(piano->getBounds() == expectedBounds());
            listener.changes = listener.resizes = 0;
            panel.resized();
            CHECK(listener.changes == 0);
        }
        panel.setSize(800, 180);
        CHECK(listener.resizes == 1);
        CHECK(piano->getBounds() == expectedBounds());

        listener.changes = listener.resizes = 0;
        panel.setCollapsed(true, false);
        panel.setSize(700, panel.getTitleBarHeight());
        CHECK(!piano->isVisible() && listener.changes == 0);
        panel.setSize(700, 180);
        CHECK(!piano->isVisible() && listener.changes == 0);
        panel.setCollapsed(false, false);
        CHECK(piano->isVisible() && listener.resizes == 1);
        CHECK(piano->getBounds() == expectedBounds());

        listener.changes = listener.resizes = 0;
        panel.setTitleBarHeight(30); // Also uses the hook without PianoPanel::resized().
        CHECK(listener.resizes == 1 && piano->getBounds() == expectedBounds());
        panel.setDisplayMode(DisplayMode::Floating);
        listener.changes = listener.resizes = 0;
        panel.setSize(720, 180);
        CHECK(listener.resizes == 1 && piano->getBounds() == expectedBounds());
        panel.setDisplayMode(DisplayMode::Flex);
    }
    {
        Project project;
        for (int i = 0; i < 8; ++i) project.getTrackList().addTrack();
        TimelinePanel panel(project);
        panel.setSize(800, 360);
        TimelineContent* content = nullptr;
        TrackHeaderList* headers = nullptr;
        for (auto* child : panel.getChildren()) {
            if (auto* value = dynamic_cast<TimelineContent*>(child)) content = value;
            if (auto* value = dynamic_cast<TrackHeaderList*>(child)) headers = value;
        }
        CHECK(content != nullptr && headers != nullptr);
        TimelineLane* lane = nullptr;
        for (auto* child : content->getChildren())
            if (auto* value = dynamic_cast<TimelineLane*>(child)) { lane = value; break; }
        CHECK(lane != nullptr);
        BoundsListener listener(*lane);
        panel.setSize(780, 360);
        CHECK(listener.changes == 1 && listener.resizes == 1);
        CHECK(lane->getWidth() == content->getWidth());
        panel.resized();
        CHECK(listener.changes == 1);

        content->onAutoScroll(80, 35);
        CHECK(lane->getY() == TimeRuler::rulerHeight - 35);
        CHECK(headers->getChildComponent(0)->getY() == lane->getY());
        const auto laidOut = lane->getBounds();

        // Equal setBounds calls do not notify listeners. Displace the lane so an
        // unwanted updateLayout would restore it and become observable.
        lane->setTopLeftPosition(7, laidOut.getY() + 3);
        const auto displaced = lane->getBounds();
        listener.changes = listener.resizes = 0;
        content->setScrollOffset(35, 80.0);
        panel.resized(); // Includes panel-level scroll synchronization.
        CHECK(listener.changes == 0 && lane->getBounds() == displaced);

        content->setScrollOffset(35, 80.5); // A horizontal-only change must not be skipped.
        CHECK(listener.changes == 1 && lane->getBounds() == laidOut);
        content->setScrollOffset(36, 80.5); // Nor a vertical-only change.
        CHECK(listener.changes == 2 && lane->getY() == laidOut.getY() - 1);
        content->setScrollOffset(36, 80.5);
        CHECK(listener.changes == 2);

        content->setPixelsPerBeat(75.0);
        content->setSize(content->getWidth() + 20, content->getHeight());
        CHECK(lane->getWidth() == content->getWidth());
        CHECK(lane->getY() == TimeRuler::rulerHeight - 36);
    }
}
