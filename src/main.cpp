#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <nanogui/window.h>
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <nanogui/button.h>
#include <nanogui/slider.h>
#include <nanogui/vscrollpanel.h>
#include <nanogui/texture.h>
#include <nanogui/shader.h>
#include <nanogui/renderpass.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#include "CpuGraph.h"
#include "simple_vert_shader.h"
#include "simple_frag_shader.h"
#include "SinGraph.h"

using namespace nanogui;

class ExampleApplication : public Screen {
public:
    ExampleApplication() : Screen(Vector2i(1024, 768), "NanoGUI Test") {
        inc_ref();

        this->set_layout(new GroupLayout());

        /* No need to store a pointer, the data structure will be automatically
           freed when the parent window is deleted */
        // new Label(this, "Push buttons", "sans-bold");

        Widget *tab_group = new Widget(this);
        tab_group->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));

        Button *b = new Button(tab_group, "CPU");
        b->set_flags(Button::RadioButton);
        b->set_pushed(true);
        b->set_callback([] { std::cout << "CPU pushed!" << std::endl; });
        b = new Button(tab_group, "Memory");
        b->set_callback([] { std::cout << "Memory pushed!" << std::endl; });
        b->set_flags(Button::RadioButton);
        b = new Button(tab_group, "GPU");
        b->set_callback([] { std::cout << "GPU pushed!" << std::endl; });
        b->set_flags(Button::RadioButton);
        b = new Button(tab_group, "Disk");
        b->set_callback([] { std::cout << "Disk pushed!" << std::endl; });
        b->set_flags(Button::RadioButton);

        Widget *cpu_graph_container = new Widget(this);
        cpu_graph_container->set_fixed_height(250);
        // CpuGraph *cpu_graph = new CpuGraph(cpu_graph_container);

        SinGraph *cpu_graph = new SinGraph(cpu_graph_container);

        // Zoom slider row
        Widget *zoom_row = new Widget(this);
        zoom_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        new Label(zoom_row, "Zoom:", "sans-bold");
        Slider *zoom_slider = new Slider(zoom_row);
        zoom_slider->set_range({0.5f, 1.0f});
        zoom_slider->set_value(1.0f);
        Label *zoom_value_label = new Label(zoom_row, "1.0x", "sans-bold");
        zoom_slider->set_callback([cpu_graph, zoom_value_label](float value) {
            cpu_graph->set_zoom(value);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << value << "x";
            zoom_value_label->set_caption(oss.str());
        });

        // Start X slider row
        auto *start_x_row = new Widget(this);
        start_x_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        new Label(start_x_row, "Start X:", "sans-bold");
        auto *start_x_slider = new Slider(start_x_row);
        start_x_slider->set_range({0.0f, 50.0f});
        start_x_slider->set_value(0.0f);
        start_x_slider->set_fixed_width(200);
        auto *start_x_label = new Label(start_x_row, "0.0", "sans-bold");
        start_x_label->set_fixed_width(50);
        start_x_slider->set_callback([cpu_graph, start_x_label](float value) {
            cpu_graph->set_start_x(value);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << value;
            start_x_label->set_caption(oss.str());
        });

        // End X slider row
        auto *end_x_row = new Widget(this);
        end_x_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        new Label(end_x_row, "End X:", "sans-bold");
        auto *end_x_slider = new Slider(end_x_row);
        end_x_slider->set_range({2.0f, 50.0f});
        end_x_slider->set_value(10.0f);
        end_x_slider->set_fixed_width(200);
        auto *end_x_label = new Label(end_x_row, "2.0", "sans-bold");
        end_x_label->set_fixed_width(50);
        end_x_slider->set_callback([cpu_graph, end_x_label](float value) {
            cpu_graph->set_end_x(value);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << value;
            end_x_label->set_caption(oss.str());
        });

        // Sample offset
        auto *sample_offset_row = new Widget(this);
        sample_offset_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        new Label(sample_offset_row, "Sample offset:", "sans-bold");
        auto *sample_offset_slider = new Slider(sample_offset_row);
        sample_offset_slider->set_range({-0.1f, 0.1f});
        sample_offset_slider->set_value(0.0f);
        sample_offset_slider->set_fixed_width(200);
        auto *sample_offset_label = new Label(sample_offset_row, "20.0", "sans-bold");
        sample_offset_label->set_fixed_width(50);
        sample_offset_slider->set_callback([cpu_graph, sample_offset_label](float value) {
            cpu_graph->set_sample_offset(value);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(5) << value;
            sample_offset_label->set_caption(oss.str());
        });

        // Pause button row
        Widget *pause_row = new Widget(this);
        pause_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
        Button *pause_button = new Button(pause_row, "Pause");
        pause_button->set_flags(Button::ToggleButton);
        pause_button->set_change_callback([cpu_graph, pause_button](bool pushed) {
            cpu_graph->set_paused(pushed);
            pause_button->set_caption(pushed ? "Resume" : "Pause");
        });

        m_coord_label = new Label(this, "Graph mouse coords: (x, y)", "sans-bold");

        cpu_graph->set_mouse_over_callback([this](float x, float y) {
            std::ostringstream oss;
            oss << "Graph mouse coords: (" << x << ", " << y << ")";
            m_coord_label->set_caption(oss.str());
        });

        VScrollPanel *panel = new VScrollPanel(this);
        panel->set_fixed_height(100);

        // Container inside scroll panel (this is where items go)
        Widget *content = new Widget(panel);
        content->set_layout(new GroupLayout());
        //

        for (int i = 0; i < 10; ++i) {
            new Label(content, "Label " + std::to_string(i), "sans-bold");
        }

        perform_layout();

        /* All NanoGUI widgets are initialized at this point. Now
           create shaders to draw the main window contents.

           NanoGUI comes with a simple wrapper around GLES/Metal/OpenGL 3,
           which eliminates most of the tedious and error-prone shader and
           buffer object management.
        */

        m_render_pass = new RenderPass({ this });
        m_render_pass->set_clear_color(0, Color(0.3f, 0.3f, 0.32f, 1.f));

        m_shader = new Shader(
            m_render_pass,
            "a_simple_shader",
            simple_vert,
            simple_frag
        );

        const uint32_t indices[3*2] = {
            0, 1, 2,
            2, 3, 0
        };

        const float positions[3*4] = {
            -1.f, -1.f, 0.f,
            1.f, -1.f, 0.f,
            1.f, 1.f, 0.f,
            -1.f, 1.f, 0.f
        };

        m_shader->set_buffer("indices", VariableType::UInt32, {3*2}, indices);
        m_shader->set_buffer("position", VariableType::Float32, {4, 3}, positions);
        m_shader->set_uniform("intensity", .5f);
    }

    // Enable components to be resized
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
        /* Draw the user interface */
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
            snprintf(caption, 128, "NanoGUI test (%.2f FPS)", 1.f / m_frame_timer.value());
            set_caption(caption);
        }
    }
private:
    Label *m_coord_label;
    ref<Shader> m_shader;
    ref<RenderPass> m_render_pass;
};

int main(int /* argc */, char ** /* argv */) {
    try {
        nanogui::init();

        /* scoped variables */ {
            ref<ExampleApplication> app = new ExampleApplication();
            app->dec_ref();
            app->set_visible(true);
            nanogui::run(RunMode::VSync);
        }

        nanogui::shutdown();
    } catch (const std::exception &e) {
        std::string error_msg = std::string("Caught a fatal error: ") + std::string(e.what());
        std::cerr << error_msg << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Caught an unknown error!" << std::endl;
    }

    return 0;
}
