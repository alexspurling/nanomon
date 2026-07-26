#include "StatsWidget.h"

#include "Graph.h"
#include "nanogui/button.h"
#include "nanogui/label.h"
#include "nanogui/layout.h"

// This include is incorrectly marked as unused - we need it to define the cast operator from Color to NVGColor
#include <nanogui/opengl.h>
#include <sstream>

#include "nanovg.h"
using namespace nanogui;

StatsWidget::StatsWidget(Widget *parent) : Widget(parent) {

    set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Minimum, 0, 10));

    // ---- column 1: sample/label stats ----
    Widget *col1 = new Widget(this);
    col1->set_layout(new GroupLayout(10, 0, 3, 0));
    m_total_samples_label  = new Label(col1, "samples: -----", "sans-bold");
    m_excess_samples_label = new Label(col1, "vertices: -----", "sans-bold");
    m_step_label           = new Label(col1, "step: -----", "sans-bold");
    m_vertex_count_label   = new Label(col1, "vertices: -----", "sans-bold");

    // Pause button
    Widget *pause_row = new Widget(col1);
    pause_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    m_pause_button = new Button(pause_row, "Pause");
    m_pause_button->set_flags(Button::ToggleButton);
    m_pause_button->set_pushed(false);

    // ---- column 2: view/scroll stats + controls ----
    Widget *col2 = new Widget(this);
    col2->set_layout(new GroupLayout(10, 0, 3, 0));
    m_data_width_label     = new Label(col2, "data_width: -----", "sans-bold");
    m_scroll_offset_label  = new Label(col2, "scroll_offset: -----", "sans-bold");

    {
        Widget *start_row = new Widget(col2);
        start_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        m_start_dec = new Button(start_row, "-");
        m_start_dec->set_size(Vector2i(5,5));
        m_start_label = new Label(start_row, "start: -----", "sans-bold");
        m_start_label->set_fixed_width(75);
        m_start_inc = new Button(start_row, "+");
    }

    {
        Widget *end_row = new Widget(col2);
        end_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        m_end_dec = new Button(end_row, "-");
        m_end_label = new Label(end_row, "end: -----", "sans-bold");
        m_end_label->set_fixed_width(75);
        m_end_inc = new Button(end_row, "+");
    }
}

void StatsWidget::set_graph(Graph *graph) const {
    m_start_dec->set_callback([graph] { graph->nudge_start(-1.0); });
    m_start_inc->set_callback([graph] { graph->nudge_start(1.0); });
    m_end_dec->set_callback([graph] { graph->nudge_end(-1.0); });
    m_end_inc->set_callback([graph] { graph->nudge_end(1.0); });

    m_pause_button->set_caption(graph->paused() ? "Resume" : "Pause");
    m_pause_button->set_pushed(graph->paused());
    m_pause_button->set_change_callback([this, graph](bool pushed) {
        graph->set_paused(pushed);
        m_pause_button->set_caption(pushed ? "Resume" : "Pause");
    });
}

void StatsWidget::draw(NVGcontext* ctx) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, m_pos.x(),m_pos.y(), m_size.x(), m_size.y(), 10);
    nvgFillColor(ctx, m_theme->m_window_fill_focused);
    nvgFill(ctx);

    Widget::draw(ctx);
}

void StatsWidget::update_labels() const {
    std::ostringstream oss;

    oss << "samples: " << m_stats.total_samples;
    m_total_samples_label->set_caption(oss.str());

    oss.str(""); oss << "excess: " << m_stats.excess_samples;
    m_excess_samples_label->set_caption(oss.str());

    oss.str(""); oss << "step: " << m_stats.step;
    m_step_label->set_caption(oss.str());

    oss.str(""); oss << "vertices: " << m_stats.vertex_count;
    m_vertex_count_label->set_caption(oss.str());

    oss.str(""); oss << "data_width: " << m_stats.data_width;
    m_data_width_label->set_caption(oss.str());

    oss.str(""); oss << "start: " << m_stats.start;
    m_start_label->set_caption(oss.str());

    oss.str(""); oss << "end: " << m_stats.end;
    m_end_label->set_caption(oss.str());

    oss.str(""); oss << "scroll_offset: " << m_stats.scroll_offset;
    m_scroll_offset_label->set_caption(oss.str());
}