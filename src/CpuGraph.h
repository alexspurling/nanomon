#ifndef NANOMON_CPUGRAPH_H
#define NANOMON_CPUGRAPH_H

#pragma once

#include <functional>
#include <nanogui/canvas.h>
#include <nanogui/shader.h>
#include <nanogui/widget.h>

#include "CpuHistory.h"

using namespace nanogui;

class CpuGraph : public Canvas {
public:
    using MouseOverCallback = std::function<void(float x, float y)>;

    CpuGraph(Widget *parent);

    void perform_layout(NVGcontext *ctx) override;
    void draw_contents() override;

    void set_zoom(float zoom) { m_zoom = zoom; }
    float zoom() const { return m_zoom; }

    void set_mouse_over_callback(const MouseOverCallback &callback) {
        m_mouse_over_callback = callback;
    }

    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    const CoreSample interpolate_sample_at(int core_id, float x) const;

    float compute_core_y(int core_id, float sample_x, int sample_window_size) const;

private:
    ref<Shader> m_shader;
    CpuHistory m_cpu_history;
    std::vector<std::vector<float>> m_graph_data;
    Timestamp m_last_sample_time;
    float m_zoom = 1.0f;
    MouseOverCallback m_mouse_over_callback;

    // This is the number of frames we wait before taking another sample
    int m_sample_interval;
};

#endif //NANOMON_CPUGRAPH_H
