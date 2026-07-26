#pragma once

#include <memory>
#include <nanogui/canvas.h>
#include <nanogui/shader.h>

#include "DataSource.h"
#include "StatsWidget.h"
#include "VertexGenerator.h"
#include "ViewWindow.h"

using namespace nanogui;

class Graph : public Canvas {
public:
    Graph(Widget *parent, std::unique_ptr<DataSource> data_source, StatsWidget *stats_widget);

    void perform_layout(NVGcontext *ctx) override;
    void draw(NVGcontext *ctx) override;
    void draw_contents() override;

    void set_paused(bool paused) { m_paused = paused; }
    [[nodiscard]] bool paused() const { return m_paused; }
    void nudge_start(double delta);
    void nudge_end(double delta);

    [[nodiscard]] const GraphStats& stats() const { return m_view_window.get_stats(); }

    bool mouse_button_event(const Vector2i &p, int button, bool down,
                            int modifiers) override;
    bool mouse_drag_event(const Vector2i &p, const Vector2i &rel,
                          int button, int modifiers) override;
    bool scroll_event(const Vector2i &p, const Vector2f &rel) override;

    void update();
    void create_context_menu();

protected:

    void draw_grid(const std::vector<float> &vertices);
    static void render_line_strip(Shader *shader,
                           const std::vector<float> &verts,
                           float x_offset,
                           const Vector3f &colour);

    ref<Shader> m_shader;
    ref<Shader> m_grid_shader;
    std::unique_ptr<DataSource> m_data_source;
    ViewWindow m_view_window{MAX_VERTICES};
    std::vector<VertexGenerator> m_vertex_generators;
    ref<StatsWidget> m_stats_widget;

    static const Vector3f GRID_COLOUR;

    bool m_paused = false;
    bool m_dragging = false;
    bool m_was_paused_before_drag = false;

    int m_sample_interval;
    int m_frame_count = 0;

    static constexpr int MAX_VERTICES = 50;
    static constexpr int SAMPLE_FREQUENCY = 1;
    static constexpr int SAMPLE_WINDOW_SIZE = 2;
    Popup *m_context_menu = nullptr;
};