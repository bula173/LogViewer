#pragma once

#include <filesystem>

namespace util::portable
{

/// Portable mode keeps every user file (config, themes, plugins, session,
/// logs, QSettings) in a `data/` folder next to the executable instead of the
/// per-user profile (%APPDATA%, ~/Library, ~/.config, registry). It is switched
/// on by an empty marker file named `portable.txt` next to the executable, so
/// an unzipped copy can run from a USB stick and leaves nothing behind.
inline constexpr const char* kMarkerFile = "portable.txt";
inline constexpr const char* kDataDirName = "data";

/// Directory containing the running executable (empty if it cannot be found).
std::filesystem::path ExecutableDir();

/// True when the marker file exists next to the executable.
bool IsPortable();

/// `<executable dir>/data` in portable mode, empty otherwise. Not created here.
std::filesystem::path DataDir();

/// Tests: pretend the executable lives in @p dir. Empty path restores the real one.
void OverrideExecutableDirForTesting(const std::filesystem::path& dir);

} // namespace util::portable
