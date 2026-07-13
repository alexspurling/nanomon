#ifndef NANOMON_CPUGRAPH_H
#define NANOMON_CPUGRAPH_H

#pragma once

#include <functional>
#include <nanogui/canvas.h>
#include <nanogui/shader.h>
#include <nanogui/widget.h>
#include <nanogui/theme.h>

#include "CpuHistory.h"
#include "LineGraph.h"

using namespace nanogui;

class CpuGraph : public Canvas {
public:
    using MouseOverCallback = std::function<void(float x, float y)>;

    CpuGraph(Widget *parent);

    void perform_layout(NVGcontext *ctx) override;
    void draw(NVGcontext *ctx) override;
    void draw_contents() override;

    void set_paused(const bool paused) { m_paused = paused; }
    bool paused() const { return m_paused; }

    void set_mouse_over_callback(const MouseOverCallback &callback) {
        m_mouse_over_callback = callback;
    }

    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    bool scroll_event(const Vector2i &p, const Vector2f &rel) override;

    CoreSample interpolate_sample_at(int core_id, float x) const;

    float compute_core_y(int core_id, float sample_x, int sample_window_size) const;

    void draw_grid(size_t num_points, float x_offset);

private:
    ref<Shader> m_shader;
    ref<Shader> m_grid_shader;
    CpuHistory m_cpu_history;
    std::vector<LineGraph> m_line_graphs;
    bool m_paused = false;
    bool m_dragging = false;
    bool m_was_paused_before_drag = true;
    MouseOverCallback m_mouse_over_callback;

    // This is the number of frames we wait before taking another sample
    int m_sample_interval;
    double m_game_time = 0.0;
};

#endif //NANOMON_CPUGRAPH_H
