#pragma once


#include <nanogui/canvas.h>
#include <nanogui/shader.h>

#include "CpuHistory.h"
#include "ViewWindow.h"
#include "VertexGenerator.h"

using namespace nanogui;

class CpuGraph : public Canvas {
public:
    explicit CpuGraph(Widget *parent);

    void perform_layout(NVGcontext *ctx) override;
    void draw(NVGcontext *ctx) override;
    void draw_contents() override;

    void set_paused(const bool paused) { m_paused = paused; }
    [[nodiscard]] bool paused() const { return m_paused; }
    void nudge_start(double delta);
    void nudge_end(double delta);

    /** Access per-core stats from the last vertex generation pass. */
    [[nodiscard]] const GraphStats& stats(const int core_id) const {
        return m_view_window.get_stats();
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
    ViewWindow m_view_window{MAX_VERTICES};
    std::vector<VertexGenerator> m_vertex_generators;

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