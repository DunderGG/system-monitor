# Persistence Module Rules

This module reads and writes what the monitor keeps between runs: the JSON settings file (`settings_file.h`, [DC-123](../../docs/design_decisions.md#dc-123)), and later, when durable history becomes a feature, a store behind an interface (see [architecture.md — Configuration and persistence](../../docs/architecture.md#configuration-and-persistence)).

## Dependency constraints

- **No Qt headers and no Windows headers.** Files are read and written with the standard library (`std::filesystem`, `std::fstream`).
- **May depend on `domain/` and nlohmann/json, and nothing else of the project.** The settings types are in `domain/settings.h`, not here, because `ui` edits and applies them and may depend only on `domain`.
- **Keep nlohmann/json out of this module's headers.** Callers see domain and standard library types only, and the CMake target links nlohmann/json privately.

## Design rules

- **No exceptions.** nlohmann/json throws by default: parse with `allow_exceptions` off and check `is_discarded()`, check a value's type before `get()`, and never call `at()`. Use the `std::error_code` overloads of `std::filesystem`.
- **Return problems; do not log them.** Reading returns each problem it found, with the value it used instead, for the caller to log. The log level is itself a setting, so the logger is not configured yet when the settings are read.
- **A setting that is missing or invalid keeps its default** and never stops the others from being read.
- **Write through a temporary file** that then replaces the target, so a write that fails leaves the previous file whole.
- Tests of reading and writing text go in `tests/unit/`; tests that touch the file system go in `tests/integration/`.
