#pragma once

namespace sysmon::ui
{

// Layout metrics shared by the top-level views, so tabs line up and use the
// same rhythm. Values are in device-independent pixels, and point sizes in
// points.

/** Margin around each tab's content. */
inline constexpr int kViewMargin = 16;

/** Space between a view's major sections: dashboard cards, the performance sidebar and page, adapter charts. */
inline constexpr int kSectionSpacing = 12;

/** Space between a heading or label and the content it introduces within a page. */
inline constexpr int kItemSpacing = 8;

/** Point size of view and page titles, e.g. "Dashboard" and "CPU". */
inline constexpr int kTitlePointSize = 16;

} // namespace sysmon::ui
