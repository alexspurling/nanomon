#pragma once

#include <string>

#include "ViewWindowRecorder.h"

struct GraphStats {
    int total_samples = 0;
    int excess_samples = 0;
    int step          = 0;
    int vertex_count  = 0;
    double data_width = 0.0;
    double start = 0.0;
    double end = 0.0;
    double scroll_offset = 0.0;
    double auto_scroll_offset = 0.0;
    double pan_offset = 0.0;
};

class ViewWindow {
public:
    ViewWindow(int max_vertices = 50);

    // ---- window management ----

    /** Shift the window by @p delta_idx sample indices. */
    void pan(double delta_idx);

    void set_view_window(double view_start, double view_end);
    void set_view_start(int start);
    void set_view_end(int end);

    void zoom(double factor, double mouse_ratio);

    // ---- accessors ----

    [[nodiscard]] int view_start() const { return m_view_start; }
    [[nodiscard]] int view_end()   const { return m_view_end; }
    [[nodiscard]] int view_width() const { return m_view_end - m_view_start; }
    [[nodiscard]] int num_samples() const { return m_num_samples; }

    void set_num_samples(int n);

    [[nodiscard]] int calculate_step() const;

    void add_sample();

    void update_scroll(double sample_progress);

    [[nodiscard]] double get_scroll_offset() const {
        return m_scroll_offset;
    }

    [[nodiscard]] const GraphStats& get_stats() const {
        return m_graph_stats;
    }

    // ---- recording ----

    /** Every output of this window, as written to a recording. */
    [[nodiscard]] ViewWindowState state() const;

    [[nodiscard]] const ViewWindowRecorder& recorder() const { return m_recorder; }

    /** Write the recorded inputs to @p path. Returns false if the file cannot be written. */
    [[nodiscard]] bool save_recording(const std::string& path) const { return m_recorder.save(path); }

    /**
     * When enabled (the default), moving the window past the newest sample saves
     * the recording and exits the process. A replay turns this off so that it can
     * carry on past the violation and report it itself.
     */
    void set_export_on_violation(const bool enabled) { m_export_on_violation = enabled; }

    /**
     * Turns input recording on or off (on by default). The recording grows for
     * the life of the window, so tests that drive millions of inputs and never
     * need to save turn it off.
     */
    void set_recording_enabled(const bool enabled) { m_recording_enabled = enabled; }

    // Set private for now to allow us to track number of vertices
    GraphStats m_graph_stats;

private:
    void update_offset();

    /**
     * Saves the recording and terminates the process. Called when the window has
     * moved somewhere it should not be able to reach, so that the inputs leading
     * up to it can be replayed by the viewwindow_replay target.
     */
    [[noreturn]] void export_recording_and_exit(const char* reason);

    /**
     * Adds one event to the recording per public call: the input on the way in,
     * the resulting state on the way out. Nested calls (pan() calling
     * set_view_window()) are left out so that a replay applies each input exactly
     * once. Recording the input up front means that a call which exits the
     * process still leaves behind the input that broke things.
     */
    class RecordScope {
    public:
        RecordScope(ViewWindow& window, ViewWindowOp op, double arg0 = 0.0, double arg1 = 0.0)
            : m_window(window) {
            if (m_window.m_recording_enabled && m_window.m_record_depth++ == 0)
                m_window.m_recorder.record(op, arg0, arg1, m_window.state());
        }
        ~RecordScope() {
            if (m_window.m_recording_enabled && --m_window.m_record_depth == 0)
                m_window.m_recorder.update_last_state(m_window.state());
        }
        RecordScope(const RecordScope&) = delete;
        RecordScope& operator=(const RecordScope&) = delete;

    private:
        ViewWindow& m_window;
    };

    ViewWindowRecorder m_recorder;
    int m_record_depth = 0;
    bool m_recording_enabled = true;
    bool m_export_on_violation = true;

    int m_max_vertices;
    int m_num_samples = 0;

    int m_view_start = -11;
    int m_view_end   = -1;

    double m_scroll_offset = 0.0;
    double m_pan_offset = 0.0;
    // Auto-scroll progress within the current step, in sample units [0, step)
    double m_sample_scroll = 0.0;
};