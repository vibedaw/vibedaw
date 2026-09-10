#pragma once

#include "Theme.h"

namespace vibedaw {

class DawLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    DawLookAndFeel() {
        setColourScheme({theme::windowBackground, theme::deepWell, theme::raised,
                         theme::border, theme::textDefault, theme::control,
                         theme::textBright, theme::highlightBackground, theme::textBright});
        setColour(juce::ResizableWindow::backgroundColourId, theme::windowBackground);
        setColour(juce::Label::textColourId, theme::textDefault);
        setColour(juce::TextButton::buttonColourId, theme::control);
        setColour(juce::TextButton::buttonOnColourId, theme::highlightBackground);
        setColour(juce::TextButton::textColourOffId, theme::textDefault);
        setColour(juce::TextButton::textColourOnId, theme::accent);
        setColour(juce::ComboBox::backgroundColourId, theme::control);
        setColour(juce::ComboBox::outlineColourId, theme::border);
        setColour(juce::ComboBox::textColourId, theme::textBright);
        setColour(juce::ComboBox::arrowColourId, theme::textSecondary);
        setColour(juce::ComboBox::focusedOutlineColourId, theme::accent);
        setColour(juce::TextEditor::backgroundColourId, theme::deepWell);
        setColour(juce::TextEditor::textColourId, theme::textBright);
        setColour(juce::TextEditor::outlineColourId, theme::border);
        setColour(juce::TextEditor::focusedOutlineColourId, theme::accent.withAlpha(0.7f));
        setColour(juce::TextEditor::highlightColourId, theme::highlightBackground);
        setColour(juce::TextEditor::highlightedTextColourId, theme::textBright);
        setColour(juce::CaretComponent::caretColourId, theme::accent);
        setColour(juce::ScrollBar::backgroundColourId, theme::deepWell);
        setColour(juce::ScrollBar::thumbColourId, theme::borderStrong);
        setColour(juce::PopupMenu::backgroundColourId, theme::raised);
        setColour(juce::PopupMenu::textColourId, theme::textBright);
        setColour(juce::PopupMenu::headerTextColourId, theme::textSecondary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::selectedSurface);
        setColour(juce::PopupMenu::highlightedTextColourId, theme::accent);
        setColour(juce::AlertWindow::backgroundColourId, theme::raised);
        setColour(juce::AlertWindow::textColourId, theme::textBright);
        setColour(juce::AlertWindow::outlineColourId, theme::borderStrong);
        setColour(juce::TooltipWindow::backgroundColourId, theme::control);
        setColour(juce::TooltipWindow::textColourId, theme::textBright);
        setColour(juce::TooltipWindow::outlineColourId, theme::borderStrong);
        setColour(juce::TreeView::backgroundColourId, theme::browserBackground);
        setColour(juce::TreeView::linesColourId, theme::border);
        setColour(juce::TreeView::selectedItemBackgroundColourId, theme::selectedSurface);
        setColour(juce::Slider::backgroundColourId, theme::deepWell);
        setColour(juce::Slider::trackColourId, theme::accent);
        setColour(juce::Slider::thumbColourId, theme::textBright);
        setColour(juce::Slider::textBoxTextColourId, theme::textDefault);
        setColour(juce::Slider::textBoxBackgroundColourId, theme::deepWell);
        setColour(juce::Slider::textBoxOutlineColourId, theme::border);
    }

    juce::Button* createDocumentWindowButton(int type) override {
        class WindowButton final : public juce::Button {
        public:
            explicit WindowButton(int buttonType)
                : Button(buttonType == juce::DocumentWindow::closeButton ? "Close"
                         : buttonType == juce::DocumentWindow::minimiseButton ? "Minimize" : "Maximize or restore"),
                  type(buttonType) {
                setTooltip(getName());
                setMouseClickGrabsKeyboardFocus(false);
            }

            void paintButton(juce::Graphics& g, bool over, bool down) override {
                const bool close = type == juce::DocumentWindow::closeButton;
                const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
                if (isEnabled() && (over || down)) {
                    const auto fill = close ? theme::dangerDim : theme::controlHover;
                    theme::drawSurface(g, bounds, down ? fill.darker(0.2f) : fill,
                                       theme::controlRadius, close ? theme::danger.withAlpha(0.55f) : theme::border);
                }
                g.setColour((over && isEnabled() ? theme::textBright : theme::textDefault)
                                .withMultipliedAlpha(isEnabled() ? 1.0f : 0.45f));
                const float x = bounds.getCentreX() - 5.0f;
                const float y = bounds.getCentreY() - 5.0f;
                if (close) {
                    g.drawLine(x, y, x + 10.0f, y + 10.0f, 1.3f);
                    g.drawLine(x + 10.0f, y, x, y + 10.0f, 1.3f);
                } else if (type == juce::DocumentWindow::minimiseButton) {
                    g.drawLine(x, y + 7.0f, x + 10.0f, y + 7.0f, 1.3f);
                } else if (getToggleState()) {
                    juce::Path back;
                    back.startNewSubPath(x + 3.0f, y + 2.0f);
                    back.lineTo(x + 3.0f, y);
                    back.lineTo(x + 10.0f, y);
                    back.lineTo(x + 10.0f, y + 7.0f);
                    back.lineTo(x + 8.0f, y + 7.0f);
                    g.strokePath(back, juce::PathStrokeType(1.2f));
                    g.drawRect(x, y + 3.0f, 7.0f, 7.0f, 1.2f);
                } else {
                    g.drawRect(x, y, 10.0f, 10.0f, 1.2f);
                }
            }

        private:
            const int type;
        };
        return new WindowButton(type);
    }

    void positionDocumentWindowButtons(juce::DocumentWindow&, int x, int y, int width, int height,
                                        juce::Button* minimise, juce::Button* maximise,
                                        juce::Button* close, bool onLeft) override {
        const int count = (minimise != nullptr) + (maximise != nullptr) + (close != nullptr);
        if (count == 0) return;
        const int buttonWidth = juce::jmin(36, juce::jmax(0, width - 8) / count);
        int nextX = onLeft ? x + 4 : x + width - 4 - buttonWidth;
        if (onLeft) std::swap(minimise, maximise);
        for (auto* button : {close, maximise, minimise}) {
            if (button == nullptr) continue;
            button->setBounds(nextX, y + 2, buttonWidth, juce::jmax(0, height - 4));
            nextX += onLeft ? buttonWidth : -buttonWidth;
        }
    }

    void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g,
                                    int width, int height, int titleX, int titleWidth,
                                    const juce::Image* icon, bool onLeft) override {
        if (width <= 0 || height <= 0) return;
        g.setGradientFill(juce::ColourGradient(theme::raised, 0.0f, 0.0f,
                                              theme::windowBackground, 0.0f, static_cast<float>(height), false));
        g.fillRect(0, 0, width, height);
        g.setColour(theme::hairline);
        g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

        juce::Graphics::ScopedSaveState save(g);
        auto title = juce::Rectangle<int>(titleX + 4, 0, juce::jmax(0, titleWidth - 8), height);
        g.reduceClipRegion(title);
        auto mark = title.removeFromLeft(18).withSizeKeepingCentre(16, 16).toFloat();
        const bool active = window.isActiveWindow();
        if (icon != nullptr && icon->isValid()) {
            g.setOpacity(active ? 1.0f : 0.55f);
            g.drawImageWithin(*icon, static_cast<int>(mark.getX()), static_cast<int>(mark.getY()),
                              16, 16, juce::RectanglePlacement::centred);
        } else {
            g.setColour(theme::accent.withAlpha(active ? 0.9f : 0.4f));
            g.drawRoundedRectangle(mark.reduced(0.5f), 3.0f, 1.2f);
            juce::Path wave;
            wave.startNewSubPath(mark.getX() + 3.0f, mark.getCentreY());
            wave.lineTo(mark.getX() + 5.0f, mark.getCentreY());
            wave.lineTo(mark.getX() + 6.5f, mark.getY() + 4.0f);
            wave.lineTo(mark.getX() + 9.0f, mark.getBottom() - 4.0f);
            wave.lineTo(mark.getX() + 10.5f, mark.getCentreY());
            wave.lineTo(mark.getRight() - 3.0f, mark.getCentreY());
            g.strokePath(wave, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved));
        }
        title.removeFromLeft(8);
        g.setColour(active ? theme::textBright : theme::textSecondary);
        g.setFont(juce::Font(13.0f));
        g.drawText(window.getName(), title, onLeft ? juce::Justification::centredLeft : juce::Justification::centred, true);
    }

    void drawResizableWindowBorder(juce::Graphics& g, int width, int height,
                                    const juce::BorderSize<int>&, juce::ResizableWindow& window) override {
        if (window.isUsingNativeTitleBar() || window.isKioskMode()) return;
        // The JUCE resize border stays four pixels wide; only its outside edge is visible.
        g.setColour(window.isActiveWindow() ? theme::borderStrong : theme::border);
        g.drawRect(0, 0, width, height);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int height) override {
        return juce::Font(juce::jlimit(10.0f, 13.0f, height * 0.48f));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& colour, bool over, bool down) override {
        const bool action = colour == theme::actionGreen;
        auto fill = down ? colour.darker(0.18f) : over ? colour.brighter(0.12f) : colour;
        auto outline = action || button.getToggleState() ? theme::accent.withAlpha(0.6f) : theme::border;
        if (button.hasKeyboardFocus(true)) outline = theme::accent;
        if (!button.isEnabled()) {
            fill = fill.interpolatedWith(theme::panelBackground, 0.55f);
            outline = theme::border.withAlpha(0.5f);
        }
        theme::drawSurface(g, button.getLocalBounds().toFloat().reduced(0.5f),
                           fill, theme::controlRadius, outline);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override {
        auto colour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                : juce::TextButton::textColourOffId);
        if (button.findColour(juce::TextButton::buttonColourId) == theme::actionGreen)
            colour = theme::accent.brighter(0.15f);
        g.setColour(colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.4f));
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 1),
                         juce::Justification::centred, 1);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(12.0f); }

    void drawComboBox(juce::Graphics& g, int width, int height, bool down,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override {
        auto fill = box.findColour(juce::ComboBox::backgroundColourId);
        if (down) fill = fill.darker(0.15f);
        theme::drawSurface(g, {0.5f, 0.5f, width - 1.0f, height - 1.0f}, fill,
                           theme::controlRadius, box.hasKeyboardFocus(true) ? theme::accent : theme::border);
        const auto x = buttonX + buttonW * 0.5f;
        const auto y = buttonY + buttonH * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath(x - 3.5f, y - 1.5f);
        chevron.lineTo(x, y + 2.0f);
        chevron.lineTo(x + 3.5f, y - 1.5f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId).withMultipliedAlpha(box.isEnabled() ? 1.0f : 0.4f));
        g.strokePath(chevron, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override {
        g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), theme::controlRadius);
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override {
        if (!editor.isEnabled()) return;
        g.setColour(editor.findColour(editor.hasKeyboardFocus(true) ? juce::TextEditor::focusedOutlineColourId
                                                                    : juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle(0.5f, 0.5f, width - 1.0f, height - 1.0f, theme::controlRadius, 1.0f);
    }

    int getDefaultScrollbarWidth() override { return 10; }

    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height,
                       bool vertical, int start, int size, bool over, bool down) override {
        g.setColour(theme::deepWell);
        g.fillRect(x, y, width, height);
        if (size <= 0) return;
        auto thumb = vertical ? juce::Rectangle<float>(static_cast<float>(x + 2), static_cast<float>(start),
                                                       static_cast<float>(juce::jmax(1, width - 4)), static_cast<float>(size))
                              : juce::Rectangle<float>(static_cast<float>(start), static_cast<float>(y + 2),
                                                       static_cast<float>(size), static_cast<float>(juce::jmax(1, height - 4)));
        g.setColour(down ? theme::textSecondary : over ? theme::tickMark : bar.findColour(juce::ScrollBar::thumbColourId));
        g.fillRoundedRectangle(thumb.reduced(0.5f), 3.0f);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override {
        g.fillAll(theme::raised);
        g.setColour(theme::borderStrong);
        g.drawRect(0, 0, width, height);
    }
};

} // namespace vibedaw
