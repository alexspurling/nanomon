#pragma once

#include "CpuGraph.h"
#include "VertexGenerator.h"

class MultiCpuGraph : public CpuGraph {
public:
    explicit MultiCpuGraph(Widget *parent);

protected:
    void draw_graph_content() override;

    std::vector<VertexGenerator> m_vertex_generators;

    static constexpr int SAMPLE_WINDOW_SIZE = 2;
};