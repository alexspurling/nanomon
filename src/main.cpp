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
    ExampleApplication() : Screen(Vector2i(1024, 768), "Nanomon") {
        inc_ref();

        // Setting the GroupLayout on the Screen widget is necessary to get the main_container to fill the width of the screen
        set_layout(new GroupLayout(0));
        Widget *main_container = new Widget(this);
        main_container->set_tooltip("Main container");
        main_container->set_layout(new GroupLayout());

        // ---- tab bar ----
        Widget *tab_group = new Widget(main_container);
        tab_group->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));

        Button *btn_cpu = new Button(tab_group, "CPU");
        btn_cpu->set_flags(Button::RadioButton);
        btn_cpu->set_pushed(true);
        Button *btn_mem = new Button(tab_group, "Memory");
        btn_mem->set_flags(Button::RadioButton);
        Button *btn_disk = new Button(tab_group, "Disk");
        btn_disk->set_flags(Button::RadioButton);

        // ---- graphs ----
        Widget *graph_container = new Widget(main_container);
        graph_container->set_fixed_height(250);

        m_cpu_graph = new Graph(graph_container, std::make_unique<CpuDataSource>());
        m_memory_graph = new Graph(graph_container, std::make_unique<MemoryDataSource>());
        m_disk_graph = new Graph(graph_container, std::make_unique<DiskDataSource>());

        // Only make the CPU graph visible for now
        m_cpu_graph->set_visible(true);
        m_memory_graph->set_visible(false);
        m_disk_graph->set_visible(false);
        m_current_graph = m_cpu_graph;

        // ---- stats labels ----
        // It's important to set an initial caption because nanogui uses this to determine the size of the labels. If
        // we were to set the initial caption to "", then the initial width would be 0
        m_total_samples_label  = new Label(main_container, "total_samples: --", "sans-bold");
        m_excess_samples_label = new Label(main_container, "excess_samples: --", "sans-bold");
        m_step_label           = new Label(main_container, "step: --", "sans-bold");
        m_vertex_count_label   = new Label(main_container, "vertex_count: --", "sans-bold");
        m_data_width_label     = new Label(main_container, "data_width: --", "sans-bold");
        m_scroll_offset_label  = new Label(main_container, "scroll_offset: --", "sans-bold");

        {
            Widget *start_row = new Widget(main_container);
            start_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
            m_start_dec = new Button(start_row, "-");
            m_start_label = new Label(start_row, "start: --", "sans-bold");
            m_start_label->set_fixed_width(100);
            m_start_inc = new Button(start_row, "+");
            m_start_dec->set_callback([this] {
                m_current_graph->nudge_start(-1.0);
            });
            m_start_inc->set_callback([this] {
                m_current_graph->nudge_start(1.0);
            });
        }

        {
            Widget *end_row = new Widget(main_container);
            end_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
            m_end_dec = new Button(end_row, "-");
            m_end_label = new Label(end_row, "end: --", "sans-bold");
            m_end_label->set_fixed_width(100);
            m_end_inc = new Button(end_row, "+");
            m_end_dec->set_callback([this] { m_current_graph->nudge_end(-1.0); });
            m_end_inc->set_callback([this] { m_current_graph->nudge_end(1.0); });
        }

        // Pause button
        Widget *pause_row = new Widget(main_container);
        pause_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        Button *pause_button = new Button(pause_row, "Pause");
        pause_button->set_flags(Button::ToggleButton);
        pause_button->set_pushed(false);
        pause_button->set_change_callback([this, pause_button](bool pushed) {
            m_current_graph->set_paused(pushed);
            pause_button->set_caption(pushed ? "Resume" : "Pause");
        });

        btn_cpu->set_callback([this, pause_button] {
            m_cpu_graph->set_visible(true);
            m_memory_graph->set_visible(false);
            m_disk_graph->set_visible(false);
            m_current_graph = m_cpu_graph;
            pause_button->set_pushed(m_current_graph->paused());
            pause_button->set_caption(pause_button->pushed() ? "Resume" : "Pause");
        });

        btn_mem->set_callback([this, pause_button] {
            m_cpu_graph->set_visible(false);
            m_memory_graph->set_visible(true);
            m_disk_graph->set_visible(false);
            m_current_graph = m_memory_graph;
            pause_button->set_pushed(m_current_graph->paused());
            pause_button->set_caption(pause_button->pushed() ? "Resume" : "Pause");
        });

        btn_disk->set_callback([this, pause_button] {
            m_cpu_graph->set_visible(false);
            m_memory_graph->set_visible(false);
            m_disk_graph->set_visible(true);
            m_current_graph = m_disk_graph;
            pause_button->set_pushed(m_current_graph->paused());
            pause_button->set_caption(pause_button->pushed() ? "Resume" : "Pause");
        });

        VScrollPanel *panel = new VScrollPanel(main_container);
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
        const auto &s = m_current_graph->stats();
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

        Screen::draw(ctx);
    }

    void draw_contents() override {

        m_cpu_graph->update();
        m_memory_graph->update();
        m_disk_graph->update();

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
            snprintf(caption, 128, "Nanomon (%.2f FPS)", 1.f / m_frame_timer.value());
            set_caption(caption);
        }
    }

private:
    // Apparently we have to use nanogui's ref type here because these objects will be referenced by the parent Widget
    // It allows us to delegate destruction of these objects to nanogui on shutdown
    ref<Graph> m_cpu_graph = nullptr;
    ref<Graph> m_memory_graph = nullptr;
    ref<Graph> m_disk_graph = nullptr;
    ref<Graph> m_current_graph = nullptr;

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