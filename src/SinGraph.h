#ifndef NANOMON_SinGraph_H
#define NANOMON_SinGraph_H

#pragma once

#include <functional>
#include <nanogui/canvas.h>
#include <nanogui/shader.h>
#include <nanogui/widget.h>

#include "LineGraph.h"

using namespace nanogui;

class SinGraph : public Canvas {
public:
    using MouseOverCallback = std::function<void(float x, float y)>;

    SinGraph(Widget *parent);

    void perform_layout(NVGcontext *ctx) override;

    void draw_grid(int num_points, double smooth_scrolling_x_offset);

    void draw(NVGcontext *ctx) override;

    void draw_contents() override;

    void set_start_x(float x) { m_line_graph.set_start_x(x); }

    float start_x() const { return static_cast<float>(m_line_graph.start_x()); }

    void set_end_x(float x) { m_line_graph.set_end_x(x); }

    float end_x() const { return static_cast<float>(m_line_graph.end_x()); }

    float get_speed() const {
        return static_cast<float>(m_line_graph.scroll_speed());
    }

    void set_speed(float speed) { m_line_graph.set_scroll_speed(speed); }

    void set_paused(const bool paused) { m_paused = paused; }
    bool paused() const { return m_paused; }

    void set_mouse_over_callback(const MouseOverCallback &callback) {
        m_mouse_over_callback = callback;
    }

    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    bool scroll_event(const Vector2i &p, const Vector2f &rel) override;

private:
    static double sin(double x);

    ref<Shader> m_shader;
    ref<Shader> m_grid_shader;
    LineGraph m_line_graph;
    bool m_paused = true;
    bool m_dragging = false;
    bool m_was_paused_before_drag = true;
    double m_drag_offset = 0.0;
    double m_last_frame_time = 0;
    double m_game_time = 0;
    MouseOverCallback m_mouse_over_callback;
};

#endif //NANOMON_SinGraph_H
