#ifndef NANOMON_SinGraph_H
#define NANOMON_SinGraph_H

#pragma once

#include <functional>
#include <nanogui/canvas.h>
#include <nanogui/shader.h>
#include <nanogui/widget.h>

using namespace nanogui;

// Maximum number of points to keep in the graph (oldest points are trimmed)
static constexpr size_t GRAPH_DATA_MAX_POINTS = 30;

class SinGraph : public Canvas {
public:
    using MouseOverCallback = std::function<void(float x, float y)>;

    SinGraph(Widget *parent);

    void perform_layout(NVGcontext *ctx) override;

    void draw_grid(int num_points, float x_offset);

    void draw(NVGcontext *ctx) override;

    void draw_contents() override;

    void set_zoom(float zoom) { m_zoom = zoom; }

    float zoom() const { return m_zoom; }

    void set_start_x(float x) { m_start_x = x; }

    float start_x() const { return m_start_x; }

    void set_end_x(float x) { m_end_x = x; }

    float end_x() const { return m_end_x; }

    float get_speed() {
        return m_scroll_speed;
    }

    void set_speed(float speed) { m_scroll_speed = speed; }

    void initialise_x_values();

    void set_paused(const bool paused) { m_paused = paused; }
    bool paused() const { return m_paused; }

    void set_mouse_over_callback(const MouseOverCallback &callback) {
        m_mouse_over_callback = callback;
    }

    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    bool scroll_event(const Vector2i &p, const Vector2f &rel) override;

    static double sample_at(double x);

    static double compute_y(double sample_x);

private:
    ref<Shader> m_shader;
    ref<Shader> m_grid_shader;
    std::vector<float> m_graph_data;
    float m_zoom = 1.0f;
    float m_start_x = 0.0f;
    float m_end_x = 10.0f;
    double m_x_offset = 0.0f;
    bool m_paused = true;
    double m_last_frame_time = 0;
    double m_game_time = 0;
    double m_scroll_speed = 1;
    MouseOverCallback m_mouse_over_callback;

    // This is the number of frames we wait before taking another sample
    double m_sample_interval;
    int m_frame_count = 0;
    double last_sample_time = 0;
};

#endif //NANOMON_SinGraph_H
