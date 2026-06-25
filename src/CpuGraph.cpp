#include "CpuGraph.h"
#include <iostream>
#include <nanogui/opengl.h>

CpuGraph::CpuGraph(nanogui::Widget *parent, const nanogui::Color &bg)
    : nanogui::Widget(parent), m_background_color(bg) {}

void CpuGraph::set_background_color(const nanogui::Color &background_color) {
    m_background_color = background_color;
}

void CpuGraph::perform_layout(NVGcontext *ctx) {
    if (parent()) {
        set_position({0, 0});
        set_size(parent()->size());
    }
    nanogui::Widget::perform_layout(ctx);
}

void CpuGraph::draw(NVGcontext *ctx) {
    nanogui::Widget::draw(ctx);

    std::cout << "CpuGraph pos: ("
              << m_pos.x() << ", " << m_pos.y()
              << ") size: ("
              << m_size.x() << ", " << m_size.y()
              << ") color: ("
              << m_background_color.r() << ", "
              << m_background_color.g() << ", "
              << m_background_color.b() << ", "
              << m_background_color.a()
              << ")" << std::endl;

    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, m_background_color);
    nvgFill(ctx);
}

nanogui::Vector2i CpuGraph::preferred_size_impl(NVGcontext *ctx) const {
    if (parent())
        return parent()->size();
    return nanogui::Widget::preferred_size(ctx);
}