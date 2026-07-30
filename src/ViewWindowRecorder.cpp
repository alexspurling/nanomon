#include "ViewWindowRecorder.h"

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

// Index matches the ViewWindowOp enumerators
constexpr std::array<const char *, 9> OP_NAMES = {
    "Construct", "SetNumSamples", "AddSample", "UpdateScroll", "Pan",
    "Zoom", "SetViewWindow", "SetViewStart", "SetViewEnd",
};

constexpr const char *FILE_HEADER = "# nanomon-viewwindow-recording 1";

} // namespace

const char *view_window_op_name(const ViewWindowOp op) {
    const auto idx = static_cast<std::size_t>(op);
    return idx < OP_NAMES.size() ? OP_NAMES[idx] : "?";
}

bool view_window_op_from_name(const std::string &name, ViewWindowOp &op) {
    for (std::size_t i = 0; i < OP_NAMES.size(); ++i) {
        if (name == OP_NAMES[i]) {
            op = static_cast<ViewWindowOp>(i);
            return true;
        }
    }
    return false;
}

void ViewWindowRecorder::record(const ViewWindowOp op, const double arg0, const double arg1,
                                const ViewWindowState &state) {
    m_events.push_back(ViewWindowEvent{op, arg0, arg1, state});
}

void ViewWindowRecorder::update_last_state(const ViewWindowState &state) {
    if (!m_events.empty())
        m_events.back().state = state;
}

bool ViewWindowRecorder::save(const std::string &path) const {
    std::ofstream out(path);
    if (!out)
        return false;

    out << FILE_HEADER << '\n'
        << "# op arg0 arg1 view_start view_end num_samples pan_offset sample_scroll"
           " scroll_offset stats_scroll_offset excess_samples step data_width\n";

    // Hex floats round-trip exactly through strtod, so a replay sees the same bits
    out << std::hexfloat;
    for (const ViewWindowEvent &e : m_events) {
        const ViewWindowState &s = e.state;
        out << view_window_op_name(e.op)
            << ' ' << e.arg0 << ' ' << e.arg1
            << ' ' << s.view_start << ' ' << s.view_end << ' ' << s.num_samples
            << ' ' << s.pan_offset << ' ' << s.sample_scroll
            << ' ' << s.scroll_offset << ' ' << s.stats_scroll_offset
            << ' ' << s.excess_samples << ' ' << s.step << ' ' << s.data_width << '\n';
    }
    out.close();
    return out.good();
}

std::vector<ViewWindowEvent> ViewWindowRecorder::load(const std::string &path) {
    std::vector<ViewWindowEvent> events;
    std::ifstream in(path);
    if (!in)
        return events;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        // Tokenise by hand: operator>> cannot read the hex floats written above
        std::istringstream iss(line);
        std::vector<std::string> tok;
        for (std::string word; iss >> word;)
            tok.push_back(word);
        if (tok.size() != 13)
            continue;

        ViewWindowEvent e;
        if (!view_window_op_from_name(tok[0], e.op))
            continue;

        const auto as_double = [&tok](const std::size_t i) { return std::strtod(tok[i].c_str(), nullptr); };
        const auto as_int = [&as_double](const std::size_t i) { return static_cast<int>(as_double(i)); };

        e.arg0 = as_double(1);
        e.arg1 = as_double(2);
        e.state.view_start = as_int(3);
        e.state.view_end = as_int(4);
        e.state.num_samples = as_int(5);
        e.state.pan_offset = as_double(6);
        e.state.sample_scroll = as_double(7);
        e.state.scroll_offset = as_double(8);
        e.state.stats_scroll_offset = as_double(9);
        e.state.excess_samples = as_int(10);
        e.state.step = as_int(11);
        e.state.data_width = as_double(12);
        events.push_back(e);
    }
    return events;
}
