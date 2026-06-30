#ifndef NANOMON_SinGraph_H
#define NANOMON_SinGraph_H

#pragma once

#include <functional>
#include <nanogui/canvas.h>
#include <nanogui/shader.h>
#include <nanogui/widget.h>

using namespace nanogui;

class SinGraph : public Canvas {
public:
    using MouseOverCallback = std::function<void(float x, float y)>;

    SinGraph(Widget *parent);

    void perform_layout(NVGcontext *ctx) override;

    void draw_grid(int num_points);

    void draw_contents() override;

    void set_zoom(float zoom) { m_zoom = zoom; }
    float zoom() const { return m_zoom; }

    void set_paused(const bool paused) { m_paused = paused; }
    bool paused() const { return m_paused; }

    void set_mouse_over_callback(const MouseOverCallback &callback) {
        m_mouse_over_callback = callback;
    }

    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    static float sample_at(float x);

    static float compute_y(float sample_x);

private:
    ref<Shader> m_shader;
    ref<Shader> m_grid_shader;
    std::vector<float> m_graph_data;
    float m_zoom = 1.0f;
    float m_x_offset = 0.0f;
    bool m_paused = false;
    MouseOverCallback m_mouse_over_callback;

    // This is the number of frames we wait before taking another sample
    int m_sample_interval;
    int m_frame_count;
};

#endif //NANOMON_SinGraph_H
