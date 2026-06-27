#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <nanogui/window.h>
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <nanogui/button.h>
#include <nanogui/vscrollpanel.h>
#include <nanogui/texture.h>
#include <nanogui/shader.h>
#include <nanogui/renderpass.h>
#include <iostream>
#include <memory>
#include <sstream>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#include "CpuGraph.h"
#include "simple_vert_shader.h"
#include "simple_frag_shader.h"

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
        CpuGraph *cpu_graph = new CpuGraph(cpu_graph_container);

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
