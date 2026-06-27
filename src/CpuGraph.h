#ifndef NANOMON_CPUGRAPH_H
#define NANOMON_CPUGRAPH_H

#pragma once

#include <nanogui/canvas.h>
#include <nanogui/shader.h>
#include <nanogui/widget.h>

#include "CpuHistory.h"

using namespace nanogui;

class CpuGraph : public Canvas {
public:
    CpuGraph(Widget *parent);

    void perform_layout(NVGcontext *ctx) override;
    void draw_contents() override;

private:
    ref<Shader> m_shader;
    CpuHistory m_cpu_history;
    std::vector<std::vector<float>> m_graph_data;
};

#endif //NANOMON_CPUGRAPH_H
