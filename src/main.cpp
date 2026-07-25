#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <nanogui/button.h>
#include <nanogui/vscrollpanel.h>
#include <nanogui/shader.h>
#include <nanogui/renderpass.h>
#include <iostream>
#include <memory>
#include <sstream>

#include "Graph.h"
#include "CpuDataSource.h"
#include "MemoryDataSource.h"
#include "DiskDataSource.h"
#include "simple_vert_shader.h"
#include "simple_frag_shader.h"

using namespace nanogui;

class ExampleApplication : public Screen {
public:
    ExampleApplication() : Screen(Vector2i(1024, 768), "NanoMON") {
        inc_ref();

        this->set_layout(new GroupLayout());

        // ---- data sources ----
        m_cpu_source    = std::make_unique<CpuDataSource>();
        m_mem_source    = std::make_unique<MemoryDataSource>();
        m_disk_source   = std::make_unique<DiskDataSource>();

        // ---- tab bar ----
        Widget *tab_group = new Widget(this);
        tab_group->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));

        Button *btn_cpu = new Button(tab_group, "CPU");
        btn_cpu->set_flags(Button::RadioButton);
        btn_cpu->set_pushed(true);
        btn_cpu->set_callback([this] { switch_source(m_cpu_source.get()); });

        Button *btn_mem = new Button(tab_group, "Memory");
        btn_mem->set_flags(Button::RadioButton);
        btn_mem->set_callback([this] { switch_source(m_mem_source.get()); });

        Button *btn_disk = new Button(tab_group, "Disk");
        btn_disk->set_flags(Button::RadioButton);
        btn_disk->set_callback([this] { switch_source(m_disk_source.get()); });

        // ---- graph ----
        Widget *graph_container = new Widget(this);
        graph_container->set_fixed_height(250);
        m_graph = new Graph(graph_container, m_cpu_source.get());

        // ---- stats labels ----
        m_total_samples_label  = new Label(this, "", "sans-bold");
        m_excess_samples_label = new Label(this, "", "sans-bold");
        m_step_label           = new Label(this, "", "sans-bold");
        m_vertex_count_label   = new Label(this, "", "sans-bold");
        m_data_width_label     = new Label(this, "", "sans-bold");
        m_scroll_offset_label  = new Label(this, "", "sans-bold");

        {
            Widget *start_row = new Widget(this);
            start_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
            m_start_dec = new Button(start_row, "-");
            m_start_label = new Label(start_row, "", "sans-bold");
            m_start_label->set_fixed_width(100);
            m_start_inc = new Button(start_row, "+");
            m_start_dec->set_callback([this] { m_graph->nudge_start(-1.0); });
            m_start_inc->set_callback([this] { m_graph->nudge_start(1.0); });
        }

        {
            Widget *end_row = new Widget(this);
            end_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
            m_end_dec = new Button(end_row, "-");
            m_end_label = new Label(end_row, "", "sans-bold");
            m_end_label->set_fixed_width(100);
            m_end_inc = new Button(end_row, "+");
            m_end_dec->set_callback([this] { m_graph->nudge_end(-1.0); });
            m_end_inc->set_callback([this] { m_graph->nudge_end(1.0); });
        }

        // Pause button
        Widget *pause_row = new Widget(this);
        pause_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        Button *pause_button = new Button(pause_row, "Pause");
        pause_button->set_flags(Button::ToggleButton);
        pause_button->set_pushed(false);
        pause_button->set_change_callback([this, pause_button](bool pushed) {
            m_graph->set_paused(pushed);
            pause_button->set_caption(pushed ? "Resume" : "Pause");
        });

        VScrollPanel *panel = new VScrollPanel(this);
        panel->set_fixed_height(100);
        Widget *content = new Widget(panel);
        content->set_layout(new GroupLayout());
        for (int i = 0; i < 10; ++i) {
            new Label(content, "Label " + std::to_string(i), "sans-bold");
        }

        perform_layout();

        // ---- render pass + shader ----
        m_render_pass = new RenderPass({ this });
        m_render_pass->set_clear_color(0, Color(0.3f, 0.3f, 0.32f, 1.f));

        m_shader = new Shader(m_render_pass, "a_simple_shader", simple_vert, simple_frag);

        const uint32_t indices[3*2] = { 0, 1, 2, 2, 3, 0 };
        const float positions[3*4] = {
            -1.f, -1.f, 0.f,
             1.f, -1.f, 0.f,
             1.f,  1.f, 0.f,
            -1.f,  1.f, 0.f
        };
        m_shader->set_buffer("indices", VariableType::UInt32, {3*2}, indices);
        m_shader->set_buffer("position", VariableType::Float32, {4, 3}, positions);
        m_shader->set_uniform("intensity", .5f);
    }

    void switch_source(DataSource *source) {
        m_graph->set_data_source(source);
    }

    bool resize_event(const Vector2i &size) override {
        perform_layout();
        return Screen::resize_event(size);
    }

    bool keyboard_event(int key, int scancode, int action, int modifiers) override {
        if (Screen::keyboard_event(key, scancode, action, modifiers))
            return true;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            set_visible(false);
            return true;
        }
        return false;
    }

    void draw(NVGcontext *ctx) override {
        if (m_graph) {
            const auto &s = m_graph->stats();
            std::ostringstream oss;

            oss << "total_samples: " << s.total_samples;
            m_total_samples_label->set_caption(oss.str());

            oss.str(""); oss << "excess_samples: " << s.excess_samples;
            m_excess_samples_label->set_caption(oss.str());

            oss.str(""); oss << "step: " << s.step;
            m_step_label->set_caption(oss.str());

            oss.str(""); oss << "vertex count: " << s.vertex_count;
            m_vertex_count_label->set_caption(oss.str());

            oss.str(""); oss << "data_width: " << s.data_width;
            m_data_width_label->set_caption(oss.str());

            oss.str(""); oss << "start: " << s.start;
            m_start_label->set_caption(oss.str());

            oss.str(""); oss << "end: " << s.end;
            m_end_label->set_caption(oss.str());

            oss.str(""); oss << "scroll_offset: " << s.scroll_offset;
            m_scroll_offset_label->set_caption(oss.str());
        }

        Screen::draw(ctx);
    }

    void draw_contents() override {
        Matrix4f mvp = Matrix4f::scale(Vector3f(
                           (float) m_size.y() / (float) m_size.x() * 0.25f, 0.25f, 0.25f)) *
                       Matrix4f::rotate(Vector3f(0, 0, 1), (float) glfwGetTime());

        m_shader->set_uniform("mvp", mvp);

        m_render_pass->resize(framebuffer_size());
        m_render_pass->begin();

        m_shader->begin();
        m_shader->draw_array(Shader::PrimitiveType::Triangle, 0, 6, true);
        m_shader->end();

        m_render_pass->end();

        if (m_frame_index % 60 == 59) {
            char caption[128];
            snprintf(caption, 128, "NanoMON (%.2f FPS)", 1.f / m_frame_timer.value());
            set_caption(caption);
        }
    }

private:
    std::unique_ptr<CpuDataSource>    m_cpu_source;
    std::unique_ptr<MemoryDataSource> m_mem_source;
    std::unique_ptr<DiskDataSource>   m_disk_source;

    Graph *m_graph = nullptr;

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

    ref<Shader> m_shader;
    ref<RenderPass> m_render_pass;
};

int main(int /* argc */, char ** /* argv */) {
    try {
        nanogui::init();

        {
            ref<ExampleApplication> app = new ExampleApplication();
            app->dec_ref();
            app->set_visible(true);
            nanogui::run(RunMode::VSync);
        }

        nanogui::shutdown();
    } catch (const std::exception &e) {
        std::cerr << "Caught a fatal error: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Caught an unknown error!" << std::endl;
    }

    return 0;
}