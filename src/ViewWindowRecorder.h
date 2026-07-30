#pragma once

#include <cstddef>
#include <string>
#include <vector>

/** Every input that can move a ViewWindow. */
enum class ViewWindowOp {
    Construct,
    SetNumSamples,
    AddSample,
    UpdateScroll,
    Pan,
    Zoom,
    SetViewWindow,
    SetViewStart,
    SetViewEnd,
};

const char *view_window_op_name(ViewWindowOp op);
bool view_window_op_from_name(const std::string &name, ViewWindowOp &op);

/** Every observable output of a ViewWindow, sampled after one input is applied. */
struct ViewWindowState {
    int view_start = 0;
    int view_end = 0;
    int num_samples = 0;
    double pan_offset = 0.0;
    double sample_scroll = 0.0;
    // The NDC offset handed to the vertex shader
    double scroll_offset = 0.0;
    // The same offset before conversion to NDC, in samples
    double stats_scroll_offset = 0.0;
    int excess_samples = 0;
    int step = 0;
    double data_width = 0.0;
};

struct ViewWindowEvent {
    ViewWindowOp op = ViewWindowOp::Construct;
    double arg0 = 0.0;
    double arg1 = 0.0;
    // State after the op was applied
    ViewWindowState state;
};

/**
 * Records the inputs a ViewWindow receives together with the state each one
 * produced, so that a live session can be replayed away from the GUI. Doubles
 * are stored as hex floats, so a replay is fed bit-identical inputs.
 */
class ViewWindowRecorder {
public:
    void record(ViewWindowOp op, double arg0, double arg1, const ViewWindowState &state);

    /**
     * Overwrites the state of the most recent event. An event is appended when a
     * call starts and its state filled in when the call returns, so that a call
     * which never returns still leaves its input in the recording.
     */
    void update_last_state(const ViewWindowState &state);

    /** Write the recording to @p path. Returns false if the file cannot be written. */
    [[nodiscard]] bool save(const std::string &path) const;

    /** Read a recording back, oldest event first. Empty if @p path cannot be read. */
    [[nodiscard]] static std::vector<ViewWindowEvent> load(const std::string &path);

    [[nodiscard]] std::size_t size() const { return m_events.size(); }
    [[nodiscard]] const std::vector<ViewWindowEvent> &events() const { return m_events; }

private:
    // update_scroll() runs once per frame, so this grows by roughly 5 MB per
    // hour of live graphing. The recording is only useful in full: dropping the
    // oldest events would leave a replay without the state they set up.
    std::vector<ViewWindowEvent> m_events;
};
