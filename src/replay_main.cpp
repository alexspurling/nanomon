#include "ViewWindow.h"
#include "ViewWindowRecorder.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/**
 * Replays a recording written by ViewWindow's bounds trip-wire.
 *
 * It reports two things: whether replaying the inputs reproduces the states that
 * were recorded live (if not, ViewWindow depends on something outside its own
 * inputs), and the first event at which the window ran past the newest sample,
 * printed with the run-up that produced it so it can be turned into a test.
 */

namespace {

constexpr double TOLERANCE = 1e-9;

bool close_enough(const double a, const double b) {
    return std::abs(a - b) <= TOLERANCE * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}

// excess_samples is only recomputed inside update_offset(), which runs on these
// ops. On any other op the recorded excess is left over from an earlier call and
// must not be treated as a fresh reading.
bool op_refreshes_excess(const ViewWindowOp op) {
    return op == ViewWindowOp::Pan || op == ViewWindowOp::Zoom || op == ViewWindowOp::UpdateScroll;
}

std::string describe_input(const ViewWindowEvent &e) {
    std::ostringstream oss;
    oss << view_window_op_name(e.op);
    switch (e.op) {
        case ViewWindowOp::Construct:
        case ViewWindowOp::SetNumSamples:
        case ViewWindowOp::SetViewStart:
        case ViewWindowOp::SetViewEnd:
            oss << '(' << static_cast<int>(e.arg0) << ')';
            break;
        case ViewWindowOp::UpdateScroll:
        case ViewWindowOp::Pan:
            oss << '(' << std::setprecision(6) << e.arg0 << ')';
            break;
        case ViewWindowOp::Zoom:
        case ViewWindowOp::SetViewWindow:
            oss << '(' << std::setprecision(6) << e.arg0 << ", " << e.arg1 << ')';
            break;
        case ViewWindowOp::AddSample:
            oss << "()";
            break;
    }
    return oss.str();
}

void apply(ViewWindow &window, const ViewWindowEvent &e) {
    switch (e.op) {
        case ViewWindowOp::Construct:                                             break;
        case ViewWindowOp::SetNumSamples: window.set_num_samples(static_cast<int>(e.arg0)); break;
        case ViewWindowOp::AddSample:     window.add_sample();                    break;
        case ViewWindowOp::UpdateScroll:  window.update_scroll(e.arg0);           break;
        case ViewWindowOp::Pan:           window.pan(e.arg0);                     break;
        case ViewWindowOp::Zoom:          window.zoom(e.arg0, e.arg1);            break;
        case ViewWindowOp::SetViewWindow: window.set_view_window(e.arg0, e.arg1); break;
        case ViewWindowOp::SetViewStart:  window.set_view_start(static_cast<int>(e.arg0)); break;
        case ViewWindowOp::SetViewEnd:    window.set_view_end(static_cast<int>(e.arg0));   break;
    }
}

std::string diff_state(const ViewWindowState &want, const ViewWindowState &got) {
    std::ostringstream oss;
    const auto cmp_int = [&oss](const char *name, const int a, const int b) {
        if (a != b) oss << ' ' << name << "(recorded " << a << ", replay " << b << ')';
    };
    const auto cmp_dbl = [&oss](const char *name, const double a, const double b) {
        if (!close_enough(a, b))
            oss << ' ' << name << "(recorded " << std::setprecision(9) << a << ", replay " << b << ')';
    };
    cmp_int("view_start", want.view_start, got.view_start);
    cmp_int("view_end", want.view_end, got.view_end);
    cmp_int("num_samples", want.num_samples, got.num_samples);
    cmp_int("excess_samples", want.excess_samples, got.excess_samples);
    cmp_int("step", want.step, got.step);
    cmp_dbl("pan_offset", want.pan_offset, got.pan_offset);
    cmp_dbl("sample_scroll", want.sample_scroll, got.sample_scroll);
    cmp_dbl("scroll_offset", want.scroll_offset, got.scroll_offset);
    cmp_dbl("data_width", want.data_width, got.data_width);
    return oss.str();
}

void print_event(const std::size_t index, const ViewWindowEvent &e) {
    const ViewWindowState &s = e.state;
    std::cout << std::setw(7) << index << "  " << std::left << std::setw(26) << describe_input(e)
              << std::right
              << " start=" << std::setw(6) << s.view_start
              << " end=" << std::setw(6) << s.view_end
              << " width=" << std::setw(5) << (s.view_end - s.view_start)
              << " n=" << std::setw(6) << s.num_samples
              << " step=" << std::setw(3) << s.step
              << " excess=" << std::setw(4) << s.excess_samples
              << std::setprecision(6) << std::fixed
              << " pan=" << std::setw(10) << s.pan_offset
              << " auto=" << std::setw(10) << s.sample_scroll
              << std::defaultfloat << '\n';
}

} // namespace

int main(int argc, char **argv) {
    const std::string path = argc > 1 ? argv[1] : "viewwindow-recording.txt";
    const std::size_t context = argc > 2 ? std::strtoul(argv[2], nullptr, 10) : 30;

    const std::vector<ViewWindowEvent> events = ViewWindowRecorder::load(path);
    if (events.empty()) {
        std::cerr << "No events loaded from " << path << '\n';
        return EXIT_FAILURE;
    }
    if (events.front().op != ViewWindowOp::Construct) {
        std::cerr << path << " does not start with a Construct event; a replay cannot be seeded\n";
        return EXIT_FAILURE;
    }

    const int max_vertices = static_cast<int>(events.front().arg0);
    std::cout << "Replaying " << events.size() << " events from " << path
              << " (max_vertices=" << max_vertices << ")\n\n";

    ViewWindow window(max_vertices);
    window.set_export_on_violation(false);

    std::size_t first_divergence = events.size();
    std::size_t first_violation = events.size();
    std::string divergence_detail;

    for (std::size_t i = 1; i < events.size(); ++i) {
        apply(window, events[i]);

        const ViewWindowState got = window.state();
        if (first_divergence == events.size()) {
            const std::string diff = diff_state(events[i].state, got);
            if (!diff.empty()) {
                first_divergence = i;
                divergence_detail = diff;
            }
        }
        if (first_violation == events.size() && op_refreshes_excess(events[i].op) &&
            got.excess_samples <= 0)
            first_violation = i;
    }

    if (first_divergence != events.size()) {
        std::cout << "Replay diverges from the recording at event " << first_divergence
                  << " (" << describe_input(events[first_divergence]) << "):"
                  << divergence_detail << "\n\n";
    } else {
        std::cout << "Replay reproduces every recorded state exactly.\n\n";
    }

    if (first_violation == events.size()) {
        std::cout << "No bounds violation during replay (excess_samples stayed >= 1).\n";
        return EXIT_SUCCESS;
    }

    const std::size_t from = first_violation > context ? first_violation - context : 1;
    std::cout << "Bounds violated at event " << first_violation << ". Run-up:\n\n";
    for (std::size_t i = from; i <= first_violation; ++i)
        print_event(i, events[i]);

    std::cout << "\nThe violating input is " << describe_input(events[first_violation])
              << " at event " << first_violation << ".\n";
    return EXIT_FAILURE;
}
