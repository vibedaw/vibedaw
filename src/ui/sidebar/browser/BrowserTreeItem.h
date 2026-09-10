#pragma once

#include "ui/Theme.h"

namespace vibedaw {

// Shared painting only: JUCE retains tree selection, disclosure and drag hit areas.
class BrowserTreeItem : public juce::TreeViewItem {
public:
    void paintOpenCloseButton(juce::Graphics& g, const juce::Rectangle<float>& area,
                              juce::Colour, bool mouseOver) override {
        const auto centre = area.getCentre();
        juce::Path chevron;
        if (isOpen()) {
            chevron.startNewSubPath(centre.x - 3.0f, centre.y - 1.5f);
            chevron.lineTo(centre.x, centre.y + 1.5f);
            chevron.lineTo(centre.x + 3.0f, centre.y - 1.5f);
        } else {
            chevron.startNewSubPath(centre.x - 1.5f, centre.y - 3.0f);
            chevron.lineTo(centre.x + 1.5f, centre.y);
            chevron.lineTo(centre.x - 1.5f, centre.y + 3.0f);
        }
        g.setColour(mouseOver ? theme::accent : theme::textSecondary);
        g.strokePath(chevron, juce::PathStrokeType(1.3f));
    }

protected:
    enum class Icon { Folder, Plugin, Sample, Preset, Empty };

    void paintRow(juce::Graphics& g, int width, int height, const juce::String& name, Icon icon) {
        const auto row = juce::Rectangle<float>(1.0f, 1.0f,
            static_cast<float>(juce::jmax(0, width - 3)), static_cast<float>(juce::jmax(0, height - 2)));
        if (isSelected()) {
            theme::drawSurface(g, row, theme::selectedSurface, theme::controlRadius,
                               theme::accent.withAlpha(0.35f));
            g.setColour(theme::accent);
            g.fillRoundedRectangle(2.0f, 4.0f, 2.0f, static_cast<float>(juce::jmax(0, height - 8)), 1.0f);
        }

        const float y = height * 0.5f;
        g.setColour(isSelected() ? theme::accent : theme::textSecondary);
        juce::Path symbol;
        switch (icon) {
            case Icon::Folder:
                symbol.startNewSubPath(7.0f, y - 5.0f);
                symbol.lineTo(11.0f, y - 5.0f);
                symbol.lineTo(13.0f, y - 3.0f);
                symbol.lineTo(19.0f, y - 3.0f);
                symbol.lineTo(19.0f, y + 5.0f);
                symbol.lineTo(7.0f, y + 5.0f);
                symbol.closeSubPath();
                g.strokePath(symbol, juce::PathStrokeType(1.1f));
                break;
            case Icon::Plugin:
                g.drawRoundedRectangle(8.0f, y - 4.0f, 10.0f, 8.0f, 2.0f, 1.1f);
                for (float x : {10.0f, 16.0f}) {
                    g.drawVerticalLine(static_cast<int>(x), y - 7.0f, y - 4.0f);
                    g.drawVerticalLine(static_cast<int>(x), y + 4.0f, y + 7.0f);
                }
                break;
            case Icon::Sample:
                for (int i = 0; i < 5; ++i) {
                    const float halfHeight = i == 2 ? 6.0f : (i % 2 == 0 ? 2.0f : 4.0f);
                    g.fillRoundedRectangle(7.0f + i * 2.5f, y - halfHeight, 1.5f, halfHeight * 2.0f, 0.7f);
                }
                break;
            case Icon::Preset:
                g.drawRoundedRectangle(8.0f, y - 6.0f, 10.0f, 12.0f, 1.5f, 1.1f);
                g.drawHorizontalLine(static_cast<int>(y - 2.0f), 11.0f, 15.0f);
                g.drawHorizontalLine(static_cast<int>(y + 2.0f), 11.0f, 15.0f);
                break;
            case Icon::Empty:
                g.drawEllipse(11.0f, y - 2.0f, 4.0f, 4.0f, 1.0f);
                break;
        }

        g.setColour(icon == Icon::Empty ? theme::textMuted
                                       : (isSelected() ? theme::textBright : theme::textDefault));
        g.setFont(juce::Font(11.5f, icon == Icon::Folder ? juce::Font::bold : juce::Font::plain));
        g.drawText(name, 26, 0, juce::jmax(0, width - 32), height, juce::Justification::centredLeft, true);
    }
};

} // namespace vibedaw
