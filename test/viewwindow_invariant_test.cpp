#include "ViewWindow.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// A dependency-free regression test for the ViewWindow bounds invariant:
//
//   after auto-scroll runs, the window must never advance past the newest
//   sample, i.e. excess_samples (num_samples - view_end) stays >= 1.
//
// It drives the window through the exact frame loop Graph::update() uses, then
// sweeps zoom and pan across the whole reachable state space. This is the class
// of bug behind the "graph scrolls past the right edge when zooming" report; if
// a change to the zoom/pan/clamp maths reintroduces it, this test fails.

namespace {

constexpr int SAMPLE_INTERVAL = 60;

int g_failures = 0;

// One driver that mirrors Graph::update(): sample + advance on the interval,
// auto-scroll every frame, both frozen while paused.
struct Driver {
    ViewWindow w{50};
    int num_samples = 0;
    int frame = 0;
    bool paused = false;

    Driver() {
        w.set_export_on_violation(false);
        w.set_recording_enabled(false); // the sweep drives millions of inputs; never saved
    }

    void tick() {
        if (frame % SAMPLE_INTERVAL == 0) {
            if (!paused) num_samples++;
            w.set_num_samples(num_samples);
            if (!paused) w.add_sample();
        }
        if (!paused)
            w.update_scroll(static_cast<double>(frame % SAMPLE_INTERVAL) / SAMPLE_INTERVAL);
        frame++;
    }

    void warm_up(const int samples) {
        for (int i = 0; i < SAMPLE_INTERVAL * samples; ++i) tick();
    }

    // Only meaningful once the window is live (warm-up has filled it)
    [[nodiscard]] bool live() const { return num_samples > 120; }
    [[nodiscard]] int excess() const { return w.get_stats().excess_samples; }
};

void check(const bool ok, const std::string &ctx, const Driver &d) {
    if (ok) return;
    if (g_failures < 20)
        std::printf("FAIL [%s]: excess=%d num_samples=%d start=%d end=%d step=%d pan=%.4f auto=%.4f\n",
                    ctx.c_str(), d.excess(), d.num_samples, d.w.view_start(), d.w.view_end(),
                    d.w.calculate_step(), d.w.get_stats().pan_offset,
                    d.w.get_stats().auto_scroll_offset);
    ++g_failures;
}

// Zoom to a target width, apply an optional sub-step pan, then advance through
// several full steps of auto-scroll, asserting the invariant the whole way.
void scenario(const int target_width, const double mouse_ratio, const double pan_frac) {
    Driver d;
    d.warm_up(150);

    int guard = 0;
    while (d.w.view_width() < target_width && guard++ < 400) d.w.zoom(1.1, mouse_ratio);
    while (d.w.view_width() > target_width && guard++ < 400) d.w.zoom(1.0 / 1.1, mouse_ratio);
    check(!d.live() || d.excess() >= 1,
          "after zoom w=" + std::to_string(target_width), d);

    if (pan_frac != 0.0) {
        d.w.pan(pan_frac);
        check(!d.live() || d.excess() >= 1, "after pan", d);
    }

    for (int i = 0; i < SAMPLE_INTERVAL * 3; ++i) {
        d.tick();
        check(!d.live() || d.excess() >= 1,
              "auto-scroll w=" + std::to_string(target_width), d);
    }
}

} // namespace

int main() {
    // Sweep widths spanning step = 1..6, every mouse anchor, with and without a
    // fractional pan (the fold trigger).
    for (int width = 8; width <= 320; ++width)
        for (int ri = 0; ri <= 20; ++ri) {
            const double ratio = ri / 20.0;
            scenario(width, ratio, 0.0);
            scenario(width, ratio, ratio);       // pan by a sub-step fraction
            scenario(width, ratio, -ratio);
        }

    if (g_failures == 0) {
        std::puts("viewwindow_invariant_test: PASS");
        return 0;
    }
    std::printf("viewwindow_invariant_test: FAIL (%d violations)\n", g_failures);
    return 1;
}
