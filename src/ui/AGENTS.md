# UI Module Rules

This module contains all Qt Widgets, custom sparkline chart widgets, view models, and presentation logic.

## Dependency constraints

- **No Windows headers.** This module must not include `<Windows.h>` or any platform-specific headers. It must not call any Windows APIs directly.
- **No monitoring or platform imports.** This module receives data only through `SystemSnapshot` values and domain types. It does not call collectors or platform wrappers.
- **Actions arrive as injected functions.** An action that changes the system, such as ending a process, calls a `std::function` typed with domain values (e.g. `ProcessTerminator`) that `main.cpp` sets. The UI never names the platform function behind it. Disable the action while no function is set.

## Design rules

- All data arrives via immutable `SystemSnapshot` signals on the UI thread. Never reach into the monitoring or platform layer.
- Use `QAbstractItemModel` or `QAbstractTableModel` subclasses for tables. Update models incrementally with `beginInsertRows`/`endInsertRows`, `beginRemoveRows`/`endRemoveRows`, and `dataChanged`. Never use `beginResetModel`/`endResetModel` on every refresh.
- Charts are custom `QWidget` subclasses using `QPainter`. Do not add Qt Charts, QCustomPlot, or other charting library dependencies.
- **Text from outside the program is plain text.** A `QLabel` renders its text as HTML when it looks like markup (`Qt::AutoText`). A label that shows text from the system or the network, such as an adapter's alias or description, a network's name, an address, a volume or a process name, calls `setTextFormat(Qt::PlainText)`. Inside rich text, such as a tooltip, escape it with `toHtmlEscaped()`. `ReadoutGrid` labels are already plain text.
- **Text is readable in both colour schemes.** Dimmed and status text uses a `TextTone` (`text_tone.h`): a `TonedLabel`, or `textToneColor()`/`dimmedTextColor()` for item models. Never colour text with `palette(mid)`, a fixed colour, or a style sheet. Every tone reaches 4.5:1 on both schemes' window and base colours; see [coding_guidelines.md — Text colours](../../docs/coding_guidelines.md#text-colours).
- Use the pointer-to-member signal/slot syntax for compile-time type safety.
- Pass `this` or an appropriate parent when constructing QObjects. Do not mix Qt parent-child ownership with `std::unique_ptr`.

