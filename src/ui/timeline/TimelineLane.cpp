#include "TimelineLane.h"
#include "project/Track.h"

namespace vibedaw {

TimelineLane::TimelineLane(Track* track, int index)
    : track(track)
    , trackIndex(index)
{
    setOpaque(true);
}

void TimelineLane::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    if (selected) {
        g.fillAll(juce::Colour(0xff2a2a3a));
    } else {
        g.fillAll(juce::Colour(0xff1e1e1e));
    }
    
    g.setColour(juce::Colour(0xff333333));
    for (int x = 0; x < bounds.getWidth(); x += 50) {
        g.drawVerticalLine(x, 0.0f, static_cast<float>(bounds.getHeight()));
    }
    
    g.setColour(juce::Colour(0xff505050));
    g.drawHorizontalLine(bounds.getBottom() - 1, 0.0f, static_cast<float>(bounds.getWidth()));
    
    drawClips(g);
}

void TimelineLane::setPixelsPerSecond(double pps) {
    pixelsPerSecond = pps;
    repaint();
}

void TimelineLane::setScrollOffset(double offset) {
    scrollOffset = offset;
    repaint();
}

void TimelineLane::setSelected(bool sel) {
    selected = sel;
    repaint();
}

void TimelineLane::drawClips(juce::Graphics& g) {
    if (!track) return;
    
    const auto& clips = track->getClips();
    for (const auto& clip : clips) {
        if (!clip) continue;
        
        double clipStart = clip->getStartTime();
        double clipDuration = clip->getDuration();
        
        int x = static_cast<int>((clipStart * pixelsPerSecond) - scrollOffset);
        int width = static_cast<int>(clipDuration * pixelsPerSecond);
        
        if (x + width < 0 || x >= getWidth()) continue;
        
        drawClip(g, clip.get(), x, width);
    }
}

void TimelineLane::drawClip(juce::Graphics& g, const Clip* clip, int x, int width) {
    if (width < 4) width = 4;
    
    auto bounds = getLocalBounds();
    int clipHeight = bounds.getHeight() - 4;
    int clipY = 2;
    
    juce::Rectangle<int> clipBounds(x, clipY, width, clipHeight);
    
    auto clipColour = clip->getColour();
    if (clip->isMuted()) {
        clipColour = clipColour.withAlpha(0.5f);
    }
    
    if (clip->isSelected()) {
        g.setColour(clipColour.brighter(0.3f));
    } else {
        g.setColour(clipColour);
    }
    
    g.fillRoundedRectangle(clipBounds.toFloat(), 4.0f);
    
    g.setColour(clipColour.darker(0.3f));
    g.drawRoundedRectangle(clipBounds.toFloat(), 4.0f, 1.0f);
    
    g.setColour(juce::Colour(0xff000000).withAlpha(0.3f));
    g.setFont(juce::Font(10.0f));
    
    auto textBounds = clipBounds.reduced(4, 2);
    if (textBounds.getWidth() > 20) {
        g.drawText(clip->getName(), textBounds, juce::Justification::topLeft);
    }
    
    switch (clip->getType()) {
        case Clip::Type::Audio: {
            g.setColour(juce::Colour(0xff000000).withAlpha(0.2f));
            for (int i = 0; i < std::min(width - 8, 50); i += 3) {
                float h = static_cast<float>(clipHeight * 0.3f + (std::sin(i * 0.5f) + 1.0f) * clipHeight * 0.2f);
                int lineY = clipY + static_cast<int>((clipHeight - h) / 2);
                g.drawVerticalLine(x + 4 + i, static_cast<float>(lineY), static_cast<float>(lineY + h));
            }
            break;
        }
        case Clip::Type::Midi: {
            g.setColour(juce::Colour(0xff000000).withAlpha(0.2f));
            for (int row = 0; row < 4; ++row) {
                int noteY = clipY + 4 + row * (clipHeight - 8) / 4;
                for (int i = 0; i < std::min(width - 8, 40); i += 8) {
                    int noteWidth = 4 + (i % 12);
                    g.fillRect(x + 4 + i, noteY, noteWidth, 3);
                }
            }
            break;
        }
        case Clip::Type::Pattern: {
            g.setColour(juce::Colour(0xff000000).withAlpha(0.2f));
            int patternCount = width / 20;
            for (int i = 0; i < patternCount; ++i) {
                int patternX = x + 4 + i * 16;
                g.drawVerticalLine(patternX, static_cast<float>(clipY + 4), static_cast<float>(clipY + clipHeight - 4));
            }
            break;
        }
    }
}

} // namespace vibedaw
