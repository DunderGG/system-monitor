# Domain Module Rules

This module defines the core data types, metrics, process identity, health states, and error types for the system monitor.

## Dependency constraints

- **No Qt headers.** This module must not depend on Qt in any way.
- **No Windows headers.** This module must not include `<Windows.h>` or any platform-specific headers.
- **Standard library only.** Types here use only C++ standard library types (`std::string`, `std::optional`, `std::vector`, `std::chrono`, etc.).

## Design rules

- Model resource data with explicit units (bytes, bytes/sec, percent, milliseconds, counts).
- Represent missing or inaccessible data with `std::optional` or error variants, never as zero or a default value.
- Process identity is always `(PID, creation time)`, never PID alone.
- Use `enum class` for all enumerations.
- Use designated initializers for struct construction where practical.
- Mark accessors and query functions `[[nodiscard]]`.

