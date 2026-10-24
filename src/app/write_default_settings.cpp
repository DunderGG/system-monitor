#include <cstdlib>

#include <spdlog/spdlog.h>

#include "domain/settings.h"
#include "persistence/settings_file.h"

// Writes the default settings to the file that its argument names. The build
// runs it for the settings.json that the portable zip holds beside
// verdandi.exe, which makes the copy portable (DC-131), so that the file holds
// the code's defaults. wmain() takes the path as UTF-16, whatever the code page.
int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2) {
        spdlog::error("Usage: verdandi_write_default_settings <settings file>");
        return EXIT_FAILURE;
    }
    if (const auto problem = verdandi::persistence::saveSettingsFile(argv[1], verdandi::domain::Settings{})) {
        spdlog::error("Cannot write the default settings: {}", *problem);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
