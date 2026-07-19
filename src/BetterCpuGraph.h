#pragma once


#include <nanogui/canvas.h>
#include <nanogui/shader.h>

#include "CpuHistory.h"
#include "BetterLineGraph.h"

// Forward declare CpuGraph so we can reference it in this .h file without creating a circular dependency
class CpuGraph;

using namespace nanogui;

/**
 * A CPU usage time-series widget built on BetterLineGraph.
 *
 * BetterCpuGraph samples CpuHistory at a fixed interval and renders
 * one line strip per CPU core.  Each BetterLineGraph holds a movable,
 * zoomable window over the history data and generates vertex data on
 * demand — only the visible portion is sent to the GPU.
 *
 * Auto-scrolling keeps the window anchored to the latest sample;
 * dragging or zooming temporarily suspends auto-scroll.
 */
class BetterCpuGraph : public Canvas {
public:
    explicit BetterCpuGraph(Widget *parent);

    void perform_layout(NVGcontext *ctx) override;
    void draw(NVGcontext *ctx) override;
    void draw_contents() override;

    void set_paused(const bool paused) { m_paused = paused; }
    [[nodiscard]] bool paused() const { return m_paused; }
    void nudge_start(double delta);
    void nudge_end(double delta);

    void set_sibling(CpuGraph *sibling) { m_sibling = sibling; }

    void set_forwarding(const bool forwarding) { m_forwarding = forwarding; }

    /** Access per-core stats from the last vertex generation pass. */
    [[nodiscard]] const LineGraphStats& stats(const int core_id) const {
        return m_line_graphs[core_id].last_stats();
    }

    bool mouse_button_event(const Vector2i &p, int button, bool down,
                            int modifiers) override;
    bool mouse_drag_event(const Vector2i &p, const Vector2i &rel,
                          int button, int modifiers) override;
    bool scroll_event(const Vector2i &p, const Vector2f &rel) override;

private:
    void draw_grid(const std::vector<float> &vertices);

    ref<Shader> m_shader;
    ref<Shader> m_grid_shader;
    CpuHistory m_cpu_history;
    std::vector<BetterLineGraph> m_line_graphs;

    CpuGraph *m_sibling = nullptr;
    bool m_forwarding = false;

    bool m_paused = false;
    bool m_dragging = false;
    bool m_was_paused_before_drag = false;

    int m_sample_interval;
    int m_frame_count = 0;

    /// Visible window width in sample-index units.
    double m_window_width = 30.0;

    static constexpr int MAX_VERTICES = 50;
    static constexpr int SAMPLE_FREQUENCY = 1;
    static constexpr int SAMPLE_WINDOW_SIZE = 2;
};