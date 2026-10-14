#pragma once

#include <QIcon>

#include "domain/process_info.h"

namespace sysmon::ui
{

/**
 * Returns the small icon shown before a process's name for its details
 * status: an amber padlock when Windows denied access, a blue shield for a
 * process the monitor never opens, and a grey circle with a bar when there
 * are no details to read. A process whose details were read gets a
 * transparent icon of the same size, so all names line up.
 *
 * The icons are painted on a 16-pixel grid at 1x, 1.5x, and 2x scale, in
 * mid-tone colours that show on light and dark rows. Each call paints new
 * pixmaps, so callers keep the icons. Requires a QGuiApplication.
 */
[[nodiscard]] QIcon detailsStatusIcon(domain::ProcessDetailsStatus status);

} // namespace sysmon::ui
