#pragma once

#include <nanogui/canvas.h>
#include <nanogui/shader.h>

#include "CpuHistory.h"
#include "ViewWindow.h"

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

    /** Access stats from the last vertex generation pass. */
    virtual const GraphStats& stats(int core_id) const {
        return m_view_window.get_stats();
    }

    bool mouse_button_event(const Vector2i &p, int button, bool down,
                            int modifiers) override;
    bool mouse_drag_event(const Vector2i &p, const Vector2i &rel,
                          int button, int modifiers) override;
    bool scroll_event(const Vector2i &p, const Vector2f &rel) override;

protected:
    /** Subclass hook: draw the graph content (grid + line strips). */
    virtual void draw_graph_content() = 0;

    /** Draw vertical grid lines at the x positions of the given vertices. */
    void draw_grid(const std::vector<float> &vertices);

    /** Upload and draw a single line strip. */
    void render_line_strip(Shader *shader,
                           const std::vector<float> &verts,
                           float x_offset,
                           const Vector3f &colour);

    ref<Shader> m_shader;
    ref<Shader> m_grid_shader;
    CpuHistory m_cpu_history;
    ViewWindow m_view_window{MAX_VERTICES};
    // Colour palette (shared by subclasses)
    static const Vector3f CORE_COLOURS[8];
    static const size_t NUM_COLOURS;
    static const Vector3f GRID_COLOUR;

    bool m_paused = false;
    bool m_dragging = false;
    bool m_was_paused_before_drag = false;

    int m_sample_interval;
    int m_frame_count = 0;

    static constexpr int MAX_VERTICES = 50;
    static constexpr int SAMPLE_FREQUENCY = 1;
};