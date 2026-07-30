# ViewWindow replay fixtures

Each `*.txt` file here is a recording of a live `ViewWindow` session, written by
the app's bounds trip-wire (`ViewWindow::export_recording_and_exit`) or by
`ViewWindow::save_recording`. CMake registers one CTest per file
(`replay_<name>`) that runs `viewwindow_replay` over it; the test fails if the
replay ever moves the window past the newest sample.

To capture a new one: run `nanomon`, reproduce the bad behaviour, and the app
writes `viewwindow-recording.txt` in its working directory as it exits. Drop that
file in here (rename it to describe the scenario) and re-run CMake.

- `healthy_session.txt` — a known-good session (zoom out/in, pan, auto-scroll).
  Guards against regressions that would break normal use.
