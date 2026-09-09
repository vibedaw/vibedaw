#pragma once

#include <cmath>
#include <algorithm>

namespace vibedaw {

struct TimelineGeometry {
    static double beatAt(double x, double scroll, double scale) { return (x + scroll) / scale; }
    static double xAt(double beat, double scroll, double scale) { return beat * scale - scroll; }
    // Nearest sixteenth note (quarter of a quarter-note beat); Alt bypasses snap.
    static double startAt(double x, double scroll, double scale, double grab, bool bypass) {
        auto beat = beatAt(x, scroll, scale) - grab;
        if (!std::isfinite(beat)) return 0.0;
        return std::max(0.0, bypass ? beat : std::round(beat * 4.0) / 4.0);
    }
    // -1 rejects; count denotes only unused space BELOW all lanes.
    static int laneAt(int x, int y, int width, int height, int ruler, int scroll, int laneHeight, int count) {
        if (x < 0 || x >= width || y < ruler || y >= height) return -1;
        return std::min(count, (y - ruler + scroll) / laneHeight);
    }
    static int edgeDelta(int position, int low, int high) {
        if (position < low || position >= high) return 0;
        if (position < low + 24) return -12;
        if (position >= high - 24) return 12;
        return 0;
    }
};

} // namespace vibedaw
