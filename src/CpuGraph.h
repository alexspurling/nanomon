#ifndef NANOMON_CPUGRAPH_H
#define NANOMON_CPUGRAPH_H

#pragma once

#include <nanogui/canvas.h>
#include <nanogui/shader.h>
#include <nanogui/widget.h>

using namespace nanogui;

class CpuGraph : public Canvas {
public:
    CpuGraph(Widget *parent);

    void draw_contents() override;

private:
    ref<Shader> m_shader;
};

#endif //NANOMON_CPUGRAPH_H
