#include "PortableMode.hpp"

#include <system_error>

#ifdef _WIN32
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <vector>
#else
#  include <unistd.h>
#  include <climits>
#endif

namespace util::portable
{

namespace
{
std::filesystem::path& Override()
{
    static std::filesystem::path dir;
    return dir;
}

std::filesystem::path RealExecutableDir()
{
    std::error_code ec;
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return {};
    const auto exe = std::filesystem::weakly_canonical(std::filesystem::path(buf), ec);
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size + 1, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0)
        return {};
    const auto exe = std::filesystem::weakly_canonical(std::filesystem::path(buf.data()), ec);
#else
    char buf[PATH_MAX] = {};
    const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0)
        return {};
    const auto exe = std::filesystem::weakly_canonical(std::filesystem::path(buf), ec);
#endif
    return ec ? std::filesystem::path{} : exe.parent_path();
}
} // namespace

std::filesystem::path ExecutableDir()
{
    if (!Override().empty())
        return Override();
    static const std::filesystem::path real = RealExecutableDir();
    return real;
}

bool IsPortable()
{
    const auto dir = ExecutableDir();
    if (dir.empty())
        return false;
    std::error_code ec;
    return std::filesystem::exists(dir / kMarkerFile, ec);
}

std::filesystem::path DataDir()
{
    return IsPortable() ? ExecutableDir() / kDataDirName : std::filesystem::path{};
}

void OverrideExecutableDirForTesting(const std::filesystem::path& dir)
{
    Override() = dir;
}

} // namespace util::portable
