#include "DawWindow.h"
#include "Theme.h"

#if JUCE_LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

namespace vibedaw {

struct DawWindow::NativeState {
#if JUCE_LINUX
    // A separate connection avoids touching JUCE's event queue or backend internals.
    Display* display = nullptr;
    ::Window window = 0;
    ::Window root = 0;
    Atom state = 0, horizontal = 0, vertical = 0;

    NativeState() {
        const auto name = juce::SystemStats::getEnvironmentVariable("DISPLAY", ":0.0");
        display = XOpenDisplay(name.isEmpty() ? ":0.0" : name.toRawUTF8());
        if (display == nullptr) return;
        state = XInternAtom(display, "_NET_WM_STATE", False);
        horizontal = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        vertical = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    }
    ~NativeState() { if (display != nullptr) XCloseDisplay(display); }
#endif
};

struct DawWindow::ResizeBorder : juce::ResizableBorderComponent {
    explicit ResizeBorder(DawWindow& window) : ResizableBorderComponent(&window, window.getConstrainer()), owner(window) {}
    ~ResizeBorder() override { cancelDrag(); }
    void mouseDown(const juce::MouseEvent& event) override {
        dragging = !owner.clientBoundsLocked();
        if (dragging) ResizableBorderComponent::mouseDown(event);
    }
    void mouseDrag(const juce::MouseEvent& event) override {
        if (dragging && !owner.clientBoundsLocked()) ResizableBorderComponent::mouseDrag(event);
    }
    void mouseUp(const juce::MouseEvent& event) override {
        if (dragging) ResizableBorderComponent::mouseUp(event);
        dragging = false;
    }
    void cancelDrag() {
        if (dragging) {
            dragging = false;
            if (auto* constrainer = owner.getConstrainer()) constrainer->resizeEnd();
        }
    }
    DawWindow& owner;
    bool dragging = false;
};

DawWindow::DawWindow(const juce::String& title, int requiredButtons, bool desktop)
    : DocumentWindow(title, theme::windowBackground, requiredButtons, false) {
    setUsingNativeTitleBar(false);
    setTitleBarHeight(32);
    setTitleBarTextCentred(false);
    setResizable(true, false);
#if JUCE_LINUX
    // Keep JUCE's size limits/capabilities, but guard captured border drags during WM maximize.
    resizeBorder_ = std::make_unique<ResizeBorder>(*this);
    Component::addChildComponent(resizeBorder_.get());
    resizeBorder_->toBack();
    resized();
#endif
    // Create the peer only after the resizable flag and custom chrome are configured.
    if (desktop) addToDesktop(getDesktopWindowStyleFlags());
#if JUCE_LINUX
    if (desktop) startTimerHz(5);
#endif
}

DawWindow::~DawWindow() { stopTimer(); }

int DawWindow::getDesktopWindowStyleFlags() const {
    auto flags = DocumentWindow::getDesktopWindowStyleFlags();
#if JUCE_LINUX
    // JUCE normally omits this flag for client-decorated windows.
    if (isResizable()) flags |= juce::ComponentPeer::windowIsResizable;
#endif
    return flags;
}

std::optional<DawWindow::MaximizedState> DawWindow::readNativeMaximizedState() {
#if JUCE_LINUX
    if (!isOnDesktop() || getPeer() == nullptr) return std::nullopt;
    const auto window = static_cast<::Window>(reinterpret_cast<uintptr_t>(getPeer()->getNativeHandle()));
    if (window == 0) return std::nullopt;
    if (!native_) native_ = std::make_unique<NativeState>();
    auto& n = *native_;
    if (n.display == nullptr) {
        native_.reset(); // Retry a transient connection failure on a subsequent poll/click.
        return std::nullopt;
    }
    if (n.window != window) {
        XWindowAttributes attributes{};
        if (!XGetWindowAttributes(n.display, window, &attributes)) return std::nullopt;
        n.window = window;
        n.root = attributes.root;
        observed_ = {};
        desired_ = false;
        inFlight_.reset();
    }
    Atom actualType = 0;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* data = nullptr;
    const auto result = XGetWindowProperty(n.display, n.window, n.state, 0, 64, False,
                                           XA_ATOM, &actualType, &format, &count, &remaining, &data);
    MaximizedState state;
    const bool valid = result == Success && remaining == 0 &&
                      (actualType == None || (actualType == XA_ATOM && format == 32));
    if (valid && data != nullptr) {
        // Xlib expands format-32 properties into native unsigned longs, including on LP64.
        const auto* atoms = reinterpret_cast<const Atom*>(data);
        for (unsigned long i = 0; i < count; ++i) {
            state.horizontal |= atoms[i] == n.horizontal;
            state.vertical |= atoms[i] == n.vertical;
        }
    }
    if (data != nullptr) XFree(data);
    if (valid) return state;
#endif
    return std::nullopt;
}

bool DawWindow::requestNativeMaximizedState(bool maximized) {
#if JUCE_LINUX
    if (!native_ || native_->display == nullptr || native_->window == 0) return false;
    auto& n = *native_;
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = n.window;
    event.xclient.message_type = n.state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = maximized ? 1 : 0; // EWMH ADD / REMOVE, not fullscreen.
    event.xclient.data.l[1] = static_cast<long>(n.horizontal);
    event.xclient.data.l[2] = static_cast<long>(n.vertical);
    event.xclient.data.l[3] = 1; // Normal application request.
    const auto sent = XSendEvent(n.display, n.root, False,
                                 SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(n.display);
    return sent != 0;
#else
    juce::ignoreUnused(maximized);
    return false;
#endif
}

void DawWindow::maximiseButtonPressed() {
#if JUCE_LINUX
    const double now = juce::Time::getMillisecondCounterHiRes();
    // No peer/state: do not fall back to JUCE's panel-covering fullscreen resize.
    if (!pollNativeState(now)) return;
    desired_ = inFlight_ ? !desired_ : !observed_.matches(true);
    if (!inFlight_) sendMaximizeRequest(now);
    updateWindowControls();
#else
    DocumentWindow::maximiseButtonPressed();
#endif
}

void DawWindow::sendMaximizeRequest(double now, bool force) {
    if (!force && observed_.matches(desired_)) return;
    if (requestNativeMaximizedState(desired_)) {
        inFlight_ = desired_;
        requestStarted_ = now;
    } else {
        desired_ = observed_.matches(true);
    }
}

bool DawWindow::pollNativeState(double now) {
    const auto state = readNativeMaximizedState();
    if (state) {
        observed_ = *state;
        if (inFlight_) {
            if (observed_.matches(*inFlight_)) {
                inFlight_.reset();
                // Serialize rapid clicks: acknowledge the first request before sending its inverse.
                sendMaximizeRequest(now);
            }
        } else {
            desired_ = observed_.matches(true);
        }
    }
    if (inFlight_ && now - requestStarted_ >= 1500.0) {
        const bool superseded = desired_ != *inFlight_;
        inFlight_.reset();
        // Send a queued inverse once even if acknowledgement was lost/delayed. Same-connection
        // EWMH requests remain ordered, so a late maximize cannot discard the user's restore.
        if (superseded) sendMaximizeRequest(now, true);
        else desired_ = observed_.matches(true);
    }
    updateWindowControls();
    return state.has_value();
}

void DawWindow::updateWindowControls() {
#if JUCE_LINUX
    if (auto* button = getMaximiseButton())
        button->setToggleState(inFlight_ ? desired_ : observed_.matches(true), juce::dontSendNotification);
    const bool locked = clientBoundsLocked();
    setDraggable(!locked);
    if (resizeBorder_) {
        if (locked) resizeBorder_->cancelDrag();
        resizeBorder_->setBounds(getLocalBounds());
        resizeBorder_->setBorderThickness(getBorderThickness());
    }
    for (auto* child : getChildren()) {
        if (dynamic_cast<juce::ResizableBorderComponent*>(child) != nullptr ||
            dynamic_cast<juce::ResizableCornerComponent*>(child) != nullptr)
            child->setVisible(child == resizeBorder_.get() && isResizable() && !locked &&
                              !isFullScreen() && !isKioskMode() && !isUsingNativeTitleBar());
    }
#endif
}

void DawWindow::timerCallback() { pollNativeState(juce::Time::getMillisecondCounterHiRes()); }

void DawWindow::resized() {
    DocumentWindow::resized();
    updateWindowControls();
}

void DawWindow::lookAndFeelChanged() {
    DocumentWindow::lookAndFeelChanged();
    updateWindowControls();
}

void DawWindow::mouseDrag(const juce::MouseEvent& event) {
#if JUCE_LINUX
    if (clientBoundsLocked()) return;
#endif
    DocumentWindow::mouseDrag(event);
}

} // namespace vibedaw
