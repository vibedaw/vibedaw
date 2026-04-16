#include "TrackHeader.h"
#include "project/Track.h"

namespace vibedaw {

class TrackHeader::ToggleButton : public juce::Component {
public:
    ToggleButton(const juce::String& label, bool isMuteButton)
        : label(label)
        , isMute(isMuteButton)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        
        if (isDown) {
            g.setColour(juce::Colour(0xff505050));
        } else if (isOver) {
            g.setColour(juce::Colour(0xff404040));
        } else {
            g.setColour(juce::Colour(0xff353535));
        }
        g.fillRoundedRectangle(bounds, 3.0f);
        
        if (toggled) {
            g.setColour(isMute ? juce::Colour(0xffd94a4a) : juce::Colour(0xff4ad94a));
        } else {
            g.setColour(juce::Colour(0xff888888));
        }
        
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(label, bounds, juce::Justification::centred);
    }
    
    void mouseEnter(const juce::MouseEvent&) override {
        isOver = true;
        repaint();
    }
    
    void mouseExit(const juce::MouseEvent&) override {
        isOver = false;
        repaint();
    }
    
    void mouseDown(const juce::MouseEvent&) override {
        isDown = true;
        repaint();
    }
    
    void mouseUp(const juce::MouseEvent&) override {
        isDown = false;
        toggled = !toggled;
        repaint();
        if (onClick) onClick();
    }
    
    void setToggled(bool t) {
        toggled = t;
        repaint();
    }
    
    bool isToggled() const { return toggled; }
    
    std::function<void()> onClick;
    
private:
    juce::String label;
    bool isMute = false;
    bool isOver = false;
    bool isDown = false;
    bool toggled = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleButton)
};

TrackHeader::~TrackHeader() = default;

TrackHeader::TrackHeader(Track* track, int index)
    : track(track)
    , trackIndex(index)
{
    muteButton = std::make_unique<ToggleButton>("M", true);
    muteButton->onClick = [this] {
        muted = muteButton->isToggled();
        if (onMuteToggled) onMuteToggled(muted);
    };
    addAndMakeVisible(*muteButton);
    
    soloButton = std::make_unique<ToggleButton>("S", false);
    soloButton->onClick = [this] {
        solo = soloButton->isToggled();
        if (onSoloToggled) onSoloToggled(solo);
    };
    addAndMakeVisible(*soloButton);
    
    if (track) {
        trackName = track->getName();
        trackColour = track->getColour();
        muted = track->isMuted();
        solo = track->isSolo();
        muteButton->setToggled(muted);
        soloButton->setToggled(solo);
    }
}

void TrackHeader::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    if (selected) {
        g.fillAll(juce::Colour(0xff3a3a4a));
    } else {
        g.fillAll(juce::Colour(0xff2a2a2a));
    }
    
    g.setColour(trackColour);
    g.fillRect(0, 0, colourStripWidth, getHeight());
    
    g.setColour(juce::Colour(0xff505050));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(bounds.getWidth()));
    
    g.setColour(juce::Colour(0xffcccccc));
    g.setFont(juce::Font(12.0f));
    
    auto textBounds = bounds.withLeft(colourStripWidth + 4).withRight(bounds.getWidth() - 4);
    textBounds.removeFromBottom(buttonSize + 8);
    g.drawText(trackName, textBounds, juce::Justification::topLeft);
}

void TrackHeader::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(colourStripWidth + 4);
    bounds.removeFromRight(4);
    bounds.removeFromBottom(4);
    
    auto buttonArea = bounds.removeFromBottom(buttonSize);
    soloButton->setBounds(buttonArea.removeFromRight(buttonSize + 4).withSizeKeepingCentre(buttonSize, buttonSize));
    muteButton->setBounds(buttonArea.removeFromRight(buttonSize + 4).withSizeKeepingCentre(buttonSize, buttonSize));
}

void TrackHeader::setTrackName(const juce::String& name) {
    trackName = name;
    repaint();
}

void TrackHeader::setTrackColour(const juce::Colour& colour) {
    trackColour = colour;
    repaint();
}

void TrackHeader::setMuted(bool m) {
    muted = m;
    muteButton->setToggled(m);
    repaint();
}

void TrackHeader::setSolo(bool s) {
    solo = s;
    soloButton->setToggled(s);
    repaint();
}

void TrackHeader::setSelected(bool sel) {
    selected = sel;
    repaint();
}

} // namespace vibedaw
