#ifndef NANOMON_CPUGRAPH_H
#define NANOMON_CPUGRAPH_H

#pragma once

#include <nanogui/widget.h>

class CpuGraph : public nanogui::Widget {
public:
    CpuGraph(nanogui::Widget *parent, const nanogui::Color &bg);

    /// Sets the background color
    void set_background_color(const nanogui::Color &background_color);

    void perform_layout(NVGcontext *ctx) override;
    void draw(NVGcontext *ctx) override;

protected:
    nanogui::Vector2i preferred_size_impl(NVGcontext *ctx) const override;

private:
    nanogui::Color m_background_color;
};

#endif //NANOMON_CPUGRAPH_H
