#pragma once

#include "ViewWindow.h"
#include "nanogui/widget.h"
using namespace nanogui;

class Graph;

class StatsWidget: public Widget {
public:
    explicit StatsWidget(Widget *parent);

    void set_graph(Graph *graph) const;

    void draw(NVGcontext *ctx);

    void set_stats(const GraphStats& stats) { m_stats = stats; }

    void update_labels() const;

private:
    Label *m_total_samples_label  = nullptr;
    Label *m_excess_samples_label = nullptr;
    Label *m_step_label           = nullptr;
    Label *m_vertex_count_label    = nullptr;
    Label *m_data_width_label     = nullptr;
    Label *m_scroll_offset_label  = nullptr;
    Label *m_start_label = nullptr;
    Label *m_end_label   = nullptr;
    Button *m_start_dec  = nullptr;
    Button *m_start_inc  = nullptr;
    Button *m_end_dec    = nullptr;
    Button *m_end_inc    = nullptr;
    Button *m_pause_button = nullptr;

    GraphStats m_stats{};

};
