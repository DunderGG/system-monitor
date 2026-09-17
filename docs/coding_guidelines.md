# Coding Guidelines

This document defines the coding conventions, patterns, and practices for the System Monitor project. The goal is a codebase that is consistent, readable, and easy to maintain for contributors with varying levels of C++ experience.

These guidelines draw from the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) (Stroustrup & Sutter) and [Qt Coding Style](https://wiki.qt.io/Coding_Conventions), adapted to the needs of this project.

---

## Naming conventions

| Element | Style | Example |
| --- | --- | --- |
| Classes, structs, enums, type aliases | `PascalCase` | `SystemSnapshot`, `CpuSample`, `ProcessIdentity` |
| Functions, methods | `camelCase` | `collectSample()`, `calculateUsage()`, `toPercent()` |
| Local variables, parameters | `camelCase` | `coreCount`, `elapsedTime`, `snapshot` |
| Member variables | `m_` prefix + `camelCase` | `m_ringBuffer`, `m_scheduler`, `m_isRunning` |
| Constants (`constexpr`, `const`) | `PascalCase` with a `k` prefix | `kMaxBufferSize`, `kDefaultInterval` |
| Enum values | `PascalCase` | `ConnectivityLevel::InternetAccess` |
| Macros (avoid if possible) | `UPPER_SNAKE_CASE` | `SYSTEM_MONITOR_VERSION` |
| Namespaces | `lowercase` | `sysmon`, `sysmon::platform`, `sysmon::monitoring` |
| File names | `snake_case` | `cpu_collector.h`, `ring_buffer.h`, `system_snapshot.h` |
| Test files | `snake_case` + `_test` suffix | `ring_buffer_test.cpp`, `cpu_collector_test.cpp` |

### Naming guidance

- Choose descriptive names. Prefer `elapsedSeconds` over `t` or `dur`.
- Avoid abbreviations unless universally understood (`pid`, `cpu`, `io` are fine; `proc`, `mgr`, `ctx` are not).
- Boolean variables and functions should read as true/false questions: `isEmpty()`, `hasAccess`, `m_isConnected`.
- Getter functions omit the `get` prefix: `name()`, not `getName()`. This follows Qt convention.
- Setter functions use the `set` prefix: `setInterval()`, `setLogLevel()`.

---

## File organization

### Header files (`.h`)

```cpp
#pragma once

// Standard library headers
#include <chrono>
#include <optional>
#include <string>
#include <vector>

// Third-party headers (Qt, spdlog, nlohmann)
#include <QObject>
#include <spdlog/spdlog.h>

// Project headers
#include "domain/cpu_sample.h"
#include "domain/process_identity.h"

namespace sysmon::monitoring {

// ...

} // namespace sysmon::monitoring
```

Rules:
- Use `#pragma once` instead of include guards. All target compilers (MSVC, and Clang/GCC if used via CI) support it.
- Group includes in the order shown above: standard library, third-party, project. Separate groups with a blank line. Alphabetize within each group.
- Keep headers self-contained: every header must compile on its own without relying on include order.
- Forward-declare types when a full definition is not needed. Prefer forward declarations in headers to reduce compile times.

### Source files (`.cpp`)

```cpp
#include "monitoring/sampling_scheduler.h"  // Own header first

#include <algorithm>

#include <QTimer>

#include "monitoring/ring_buffer.h"
```

The file's own header comes first (before all other includes). This ensures the header is self-contained — if it's missing an include, the `.cpp` will fail to compile.

### One class per file

Each public class gets its own header and source file, named after the class in `snake_case`:

- `CpuCollector` → `cpu_collector.h` / `cpu_collector.cpp`
- `SystemSnapshot` → `system_snapshot.h` / `system_snapshot.cpp`

Small helper types (e.g., a struct used only by one class) may live in the same file as the class that uses them.

---

## Namespace structure

All project code lives under the `sysmon` namespace, with sub-namespaces matching the module structure:

```cpp
sysmon::domain       // Data types, process identity, metrics
sysmon::monitoring   // Collectors, scheduler, ring buffers, snapshots
sysmon::platform     // Windows API wrappers
sysmon::persistence  // Settings, future data storage
sysmon::ui           // Qt widgets, view models, charts
```

Rules:
- Never use `using namespace` in headers.
- `using namespace` is allowed in `.cpp` files for the file's own namespace (e.g., `using namespace sysmon::monitoring;` inside `sampling_scheduler.cpp`).
- Do not use `using namespace std;` anywhere. Qualify standard library names explicitly or use targeted `using` declarations (e.g., `using std::chrono::steady_clock;`).

---

## Modern C++20 features to use

This project targets **C++20**. Use modern language features where they improve clarity, safety, or performance.

### Prefer

| Feature | Use for | Example |
| --- | --- | --- |
| `std::jthread` + `std::stop_token` | Background threads with cooperative cancellation | Scheduler and collector threads |
| `std::chrono::steady_clock` | All timing and interval measurement | Rate calculations, scheduler tick |
| `std::format` | String formatting in logs and UI labels | `std::format("{:.1f}%", cpuUsage)` |
| `std::optional<T>` | Values that may be absent | Process path that couldn't be queried |
| `std::span<T>` | Non-owning views over contiguous data | Passing ring buffer contents to chart widgets |
| `std::string_view` | Non-owning string references | Function parameters that don't need ownership |
| Concepts | Constraining template parameters | Collector interface constraints |
| Designated initializers | Constructing structs with named fields | `CpuSample{.totalUsage = 42.5, .coreCount = 8}` |
| Scoped enums (`enum class`) | All enumerations | `enum class ConnectivityLevel { None, LocalAccess, ... }` |
| Structured bindings | Decomposing pairs, tuples, structs | `auto [pid, createTime] = processId;` |
| Range-based `for` | Iterating containers | `for (const auto& sample : m_buffer)` |
| `[[nodiscard]]` | Functions whose return value must not be ignored | Collector `collect()` methods, factory functions |
| CTAD (class template argument deduction) | When the deduced type is obvious | `std::lock_guard lock(m_mutex);` |

### Avoid

| Feature | Why | Use instead |
| --- | --- | --- |
| Raw `new` / `delete` | Memory leaks, exception unsafety | `std::make_unique`, `std::make_shared`, RAII, Qt parent-child ownership |
| Raw C arrays | No bounds checking, decay to pointers | `std::array`, `std::vector`, `std::span` |
| C-style casts `(int)x` | Unsafe, unclear intent | `static_cast`, `reinterpret_cast` (for Windows API interop only) |
| `#define` constants | No type safety, no scoping | `constexpr` variables |
| `#define` functions | No type safety, surprising evaluation | `constexpr` functions, templates |
| `std::bind` | Hard to read, surprising lifetime issues | Lambdas |
| `volatile` for threading | Does not provide synchronization on MSVC or any modern compiler | `std::atomic`, `std::mutex` |
| Exceptions across module boundaries | Qt doesn't use exceptions; Windows API callbacks can't throw | Return error types or `std::optional` (see error handling section) |

---

## Error handling

### Strategy: error values, not exceptions

This project does **not** use C++ exceptions for error handling. Reasons:

1. Qt does not use exceptions. Mixing exception-based and non-exception code leads to inconsistency.
2. Windows API callbacks (window procedures, COM methods) cannot propagate exceptions.
3. Error conditions in this application (process access denied, counter unavailable, network probe timeout) are expected and routine, not exceptional.

### Patterns

**For functions that may fail, return the error in the type:**

```cpp
// Use std::optional when "no value" is the only failure mode
std::optional<std::wstring> queryProcessPath(DWORD pid);

// Use a result struct when you need error detail
struct CollectResult {
    CpuSample sample;
    std::vector<CollectorError> errors;  // Partial success is normal
};

CollectResult collectCpuSample();
```

**For programming errors (bugs), use assertions:**

```cpp
void RingBuffer::push(const Sample& sample) {
    assert(m_capacity > 0 && "RingBuffer must have non-zero capacity");
    // ...
}
```

**Never substitute zero or a default for missing data.** If a value couldn't be obtained, represent that explicitly with `std::optional` or an error variant. The UI layer decides how to display missing data (e.g., "N/A", "Access denied", a dimmed indicator).

---

## Memory management

### Ownership rules

1. **Use `std::unique_ptr` for single ownership.** This is the default for owning pointers.
2. **Use `std::shared_ptr` sparingly.** Only when ownership is genuinely shared (rare in this project). Document why shared ownership is needed when you use it.
3. **Use raw pointers and references for non-owning access.** A raw pointer or reference means "I don't own this and won't delete it."
4. **Use Qt's parent-child ownership for widgets.** When constructing a `QWidget` with a parent, Qt manages the lifetime. Do not also wrap it in a smart pointer.

```cpp
// Good: Qt parent manages lifetime
auto* label = new QLabel("CPU Usage", parentWidget);

// Good: unique_ptr for non-Qt objects with clear ownership
auto collector = std::make_unique<CpuCollector>();

// Bad: double ownership — smart pointer + Qt parent
auto label = std::make_unique<QLabel>("CPU Usage", parentWidget); // Don't do this
```

### RAII for Windows resources

Wrap all Windows handles and resources in RAII types that release them in the destructor:

```cpp
// Example: RAII wrapper for a process handle
class ProcessHandle {
public:
    explicit ProcessHandle(HANDLE handle) : m_handle(handle) {}
    ~ProcessHandle() { if (m_handle) CloseHandle(m_handle); }

    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    ProcessHandle(ProcessHandle&& other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}
    ProcessHandle& operator=(ProcessHandle&& other) noexcept {
        if (this != &other) {
            if (m_handle) CloseHandle(m_handle);
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }

    HANDLE get() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

private:
    HANDLE m_handle = nullptr;
};
```

Apply the same pattern to: PDH query handles, MIB tables (`FreeMibTable`), COM initialization (`CoUninitialize`), and any other resource that needs cleanup.

---

## Threading

### Rules

1. **No shared mutable state between threads** unless protected by a mutex or atomic. Prefer passing immutable data (snapshots) across thread boundaries via Qt queued signals.
2. **Use `std::jthread` with `std::stop_token`** for all background threads. Never use `std::thread` directly (it terminates the program if destroyed while joinable).
3. **Use `std::mutex` and `std::lock_guard`** (or `std::scoped_lock` for multiple mutexes) for protecting shared state. Never lock manually without a guard.
4. **Keep critical sections short.** Copy data under the lock, then process it outside the lock.
5. **Qt signals across threads** must use `Qt::QueuedConnection` (or `Qt::AutoConnection`, which is queued when sender and receiver are on different threads). Never call widget methods from a non-UI thread.

```cpp
// Good: emit immutable snapshot via signal, auto-queued across threads
emit snapshotReady(snapshot);  // snapshot is a value type, implicitly shared via Qt

// Bad: directly updating a widget from a background thread
m_cpuLabel->setText(/* ... */);  // Undefined behavior
```

---

## Qt-specific patterns

### Signals and slots

- Use the pointer-to-member function syntax for connections (type-safe, catches errors at compile time):

```cpp
// Good: compile-time checked
connect(m_scheduler, &SamplingScheduler::snapshotReady,
        this, &DashboardView::onSnapshotReady);

// Avoid: string-based, errors only at runtime
connect(m_scheduler, SIGNAL(snapshotReady(SystemSnapshot)),
        this, SLOT(onSnapshotReady(SystemSnapshot)));
```

### Models

- Subclass `QAbstractItemModel` or `QAbstractTableModel` for process and network tables.
- Use `beginInsertRows` / `endInsertRows`, `beginRemoveRows` / `endRemoveRows`, and `dataChanged` to update models incrementally. Never call `beginResetModel` / `endResetModel` on every refresh — this destroys scroll position, selection state, and sort order.

### Resource management

- Pass `this` (or another appropriate parent) as the parent when constructing QObjects so Qt manages lifetime.
- Do not mix Qt parent-child ownership with `std::unique_ptr` (see memory management section).

---

## Windows API wrapping

All Windows API calls must be isolated in the `src/platform/windows/` module. No other module includes Windows headers.

### Rules

1. **Define `WIN32_LEAN_AND_MEAN` and `NOMINMAX`** before including `<Windows.h>` (or define them project-wide in CMake). `NOMINMAX` prevents the Windows `min`/`max` macros from conflicting with `<algorithm>`.
2. **Convert Windows types to domain types at the boundary.** A Windows wrapper returns `sysmon::domain::CpuSample`, never a `FILETIME` or `SYSTEM_PROCESS_INFORMATION`.
3. **Handle errors at the call site.** Check return values from every Windows API call. Convert error codes to domain error types.
4. **Use RAII for all handles and resources** (see memory management section).
5. **Use `W` (wide) variants of Windows APIs**, not `A` (ANSI). We target Unicode throughout.

```cpp
// Good: wrapper converts Windows types to domain types
std::optional<std::wstring> queryProcessImagePath(DWORD pid) {
    ProcessHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process) {
        return std::nullopt;
    }

    std::wstring path(MAX_PATH, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &size)) {
        return std::nullopt;
    }

    path.resize(size);
    return path;
}
```

---

## Formatting and whitespace

### Indentation and braces

- Use **4 spaces** for indentation. No tabs.
- Use **Allman style** (opening brace on its own line) for function, class, struct, enum, and namespace bodies:

```cpp
namespace sysmon::monitoring
{

class SamplingScheduler : public QObject
{
    Q_OBJECT

public:
    explicit SamplingScheduler(QObject* parent = nullptr);
    ~SamplingScheduler() override;

    void start();
    void stop();

signals:
    void snapshotReady(const SystemSnapshot& snapshot);

private:
    void tick();

    std::jthread m_thread;
    std::chrono::milliseconds m_interval{1000};
};

} // namespace sysmon::monitoring
```

- Use **K&R style** (opening brace on same line) for control flow statements (`if`, `for`, `while`, `switch`):

```cpp
if (sample.totalUsage > kHighCpuThreshold) {
    spdlog::warn("High CPU usage: {:.1f}%", sample.totalUsage);
}

for (const auto& adapter : adapters) {
    if (adapter.type == AdapterType::Loopback) {
        continue;
    }
    result.push_back(toNetworkSample(adapter));
}
```

- Always use braces for `if`, `for`, `while`, and `do-while`, even for single-line bodies. This prevents bugs from adding lines to unbraced blocks.

### Line length

- Aim for **100 characters** per line. Hard limit at **120 characters**. Break long lines at logical points (after commas, before operators).

### Spacing

- One space after control keywords: `if (`, `for (`, `while (`, `switch (`.
- No space after function names: `calculateUsage(`, `m_buffer.push(`.
- One blank line between function definitions.
- Two blank lines between sections (e.g., between public and private method implementations in a `.cpp` file).
- No trailing whitespace.

---

## Comments and documentation

### When to comment

- **Do** document public class interfaces (purpose, usage, thread safety).
- **Do** explain non-obvious design decisions, workarounds, and "why" (not "what").
- **Do** add `// TODO:` comments for known improvements (include your name or a tracking issue).
- **Don't** comment obvious code (`i++; // increment i`).
- **Don't** leave commented-out code in the codebase. Use version control instead.

### Style

Use `//` for single-line and short comments. Use `/** */` (Doxygen-style) for public API documentation:

```cpp
/**
 * A fixed-capacity circular buffer for time-series metric samples.
 *
 * New samples are pushed to the back. When the buffer is full,
 * the oldest sample is overwritten. The buffer is not thread-safe;
 * the caller must synchronize access.
 *
 * @tparam T The sample type. Must be default-constructible and copyable.
 */
template<typename T>
class RingBuffer
{
public:
    /**
     * Constructs a ring buffer with the given capacity.
     * @param capacity Maximum number of samples. Must be > 0.
     */
    explicit RingBuffer(std::size_t capacity);

    /**
     * Pushes a sample to the back of the buffer.
     * If the buffer is full, the oldest sample is overwritten.
     */
    void push(const T& sample);

    /** Returns the number of samples currently stored. */
    [[nodiscard]] std::size_t size() const;

    /** Returns a span over the stored samples in chronological order. */
    [[nodiscard]] std::span<const T> samples() const;
};
```

---

## Testing guidelines

### Naming

Name test cases and test functions descriptively. Use the pattern `TypeName_Scenario_ExpectedResult`:

```cpp
TEST(RingBuffer, PushBeyondCapacity_OverwritesOldest)
{
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push(4);  // Overwrites 1

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_EQ(buffer.samples()[0], 2);
}
```

### Structure

- **Arrange–Act–Assert**: Set up state, perform the action, verify the outcome. Separate the three phases with blank lines.
- **One logical assertion per test.** Multiple `EXPECT_*` calls that verify different aspects of the same result are fine. Separate tests for separate behaviors.
- **Use `EXPECT_*` by default**, not `ASSERT_*`. `EXPECT` continues executing after failure, so you see all failures. Use `ASSERT` only when the rest of the test is meaningless without the assertion passing.

### Fakes over mocks

Prefer hand-written fake implementations over mocking frameworks. Fakes are easier to understand and maintain:

```cpp
class FakeCpuCollector : public ICpuCollector
{
public:
    CpuSample nextSample;  // Set this in the test

    CollectResult collect() override
    {
        return {.sample = nextSample, .errors = {}};
    }
};
```

---

## Logging

Use spdlog for all logging. Do not use `qDebug()`, `qWarning()`, or `std::cout` for diagnostic output.

```cpp
#include <spdlog/spdlog.h>

spdlog::info("Application started, refresh interval: {}ms", interval.count());
spdlog::warn("Cannot query process path for PID {}: access denied", pid);
spdlog::error("GetIfTable2 failed with error code {}", errorCode);
spdlog::debug("Scheduler tick completed in {:.2f}ms", elapsed.count());
```

Use structured values in log messages (numbers, PIDs, error codes), not preformatted strings. This makes logs easier to search and filter.

---

## References

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) — Bjarne Stroustrup & Herb Sutter. The authoritative reference for modern C++ best practices. Specific sections referenced:
  - [I: Interfaces](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-interfaces)
  - [R: Resource management](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-resource)
  - [CP: Concurrency and parallelism](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-concurrency)
  - [E: Error handling](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-errors)
  - [Enum: Enumerations](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-enum)
  - [NL: Naming and layout](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-naming)
- [Qt Coding Conventions](https://wiki.qt.io/Coding_Conventions) — Qt Project wiki. Referenced for getter/setter naming, signal/slot style, and parent-child ownership.
- [Qt API Design Principles](https://wiki.qt.io/API_Design_Principles) — Informed the decision on naming getters without `get` prefix and the approach to model updates.

