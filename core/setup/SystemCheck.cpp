#include "SystemCheck.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

namespace local_jarvis::setup {

std::uint64_t SystemCheck::totalRamBytes() const
{
#if defined(_WIN32)
    MEMORYSTATUSEX status {};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) == 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(status.ullTotalPhys);
#elif defined(__APPLE__)
    std::uint64_t memory = 0;
    std::size_t size = sizeof(memory);
    if (sysctlbyname("hw.memsize", &memory, &size, nullptr, 0) != 0) {
        return 0;
    }
    return memory;
#else
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || pageSize <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageSize);
#endif
}

double SystemCheck::totalRamGb() const
{
    return static_cast<double>(totalRamBytes()) / (1024.0 * 1024.0 * 1024.0);
}

} // namespace local_jarvis::setup
