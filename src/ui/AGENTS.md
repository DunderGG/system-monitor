# UI Module Rules

This module contains all Qt Widgets, custom sparkline chart widgets, view models, and presentation logic.

## Dependency constraints

- **No Windows headers.** This module must not include `<Windows.h>` or any platform-specific headers. It must not call any Windows APIs directly.
- **No monitoring or platform imports.** This module receives data only through `SystemSnapshot` values and domain types. It does not call collectors or platform wrappers.

## Design rules

- All data arrives via immutable `SystemSnapshot` signals on the UI thread. Never reach into the monitoring or platform layer.
- Use `QAbstractItemModel` or `QAbstractTableModel` subclasses for tables. Update models incrementally with `beginInsertRows`/`endInsertRows`, `beginRemoveRows`/`endRemoveRows`, and `dataChanged`. Never use `beginResetModel`/`endResetModel` on every refresh.
- Charts are custom `QWidget` subclasses using `QPainter`. Do not add Qt Charts, QCustomPlot, or other charting library dependencies.
- Use the pointer-to-member signal/slot syntax for compile-time type safety.
- Pass `this` or an appropriate parent when constructing QObjects. Do not mix Qt parent-child ownership with `std::unique_ptr`.

