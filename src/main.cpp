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

        // We have to set the layout of the screen widget to allow its child elements to expand to the max width
        set_layout(new GroupLayout());

        // ---- tab bar ----
        Widget *tab_group = new Widget(this);
        tab_group->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));

        Button *btn_cpu = new Button(tab_group, "CPU");
        btn_cpu->set_flags(Button::RadioButton);
        btn_cpu->set_pushed(true);
        Button *btn_mem = new Button(tab_group, "Memory");
        btn_mem->set_flags(Button::RadioButton);
        Button *btn_disk = new Button(tab_group, "Disk");
        btn_disk->set_flags(Button::RadioButton);

        // ---- graphs ----
        Widget *graph_container = new Widget(this);
        graph_container->set_fixed_height(250);

        m_stats_widget = new StatsWidget(graph_container);

        m_cpu_graph = new Graph(graph_container, std::make_unique<CpuDataSource>(), m_stats_widget);
        m_memory_graph = new Graph(graph_container, std::make_unique<MemoryDataSource>(), m_stats_widget);
        m_disk_graph = new Graph(graph_container, std::make_unique<DiskDataSource>(), m_stats_widget);

        // Only make the CPU graph visible for now
        m_cpu_graph->set_visible(true);
        m_memory_graph->set_visible(false);
        m_disk_graph->set_visible(false);
        m_current_graph = m_cpu_graph;

        m_stats_widget->set_graph(m_current_graph);
        m_stats_widget->set_position(Vector2i(5, 5));
        m_stats_widget->set_visible(false);

        // We have to remove the stats widget before adding it back to ensure that it renders _after_ the graph
        graph_container->remove_child(m_stats_widget);
        graph_container->add_child(m_stats_widget);

        btn_cpu->set_callback([this] {
            m_cpu_graph->set_visible(true);
            m_memory_graph->set_visible(false);
            m_disk_graph->set_visible(false);
            m_current_graph = m_cpu_graph;
            m_stats_widget->set_graph(m_current_graph);
        });

        btn_mem->set_callback([this] {
            m_cpu_graph->set_visible(false);
            m_memory_graph->set_visible(true);
            m_disk_graph->set_visible(false);
            m_current_graph = m_memory_graph;
            m_stats_widget->set_graph(m_current_graph);
        });

        btn_disk->set_callback([this] {
            m_cpu_graph->set_visible(false);
            m_memory_graph->set_visible(false);
            m_disk_graph->set_visible(true);
            m_current_graph = m_disk_graph;
            m_stats_widget->set_graph(m_current_graph);
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
    ref<StatsWidget> m_stats_widget = nullptr;

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