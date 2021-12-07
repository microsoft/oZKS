// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "mem_utils.h"

// OS specific
#if defined(_WIN32)
#include <windows.h>
#include <Psapi.h>
#elif defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#include <unistd.h>
#include <sys/resource.h>
#include <stdio.h>
#else
#error "Unknown OS"
#endif


using namespace std;

pair<size_t, size_t> get_current_mem()
{
#if defined(_WIN32)
    /* Windows */
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(
        GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pmc), sizeof(pmc));
    return { pmc.WorkingSetSize, pmc.PrivateUsage };
#elif defined(__APPLE__) && defined(__MACH__)
    /* OSX */
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) !=
        KERN_SUCCESS) {
        return {};
    }
    size_t resident_size = (size_t)info.resident_size;
    return { resident_size, resident_size };
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    /* Linux  */
    long rss = 0L;
    FILE *fp = NULL;
    if ((fp = fopen("/proc/self/statm", "r")) == NULL)
        return {}; /* Can't open? */
    if (fscanf(fp, "%*s%ld", &rss) != 1) {
        fclose(fp);
        return {}; /* Can't read? */
    }
    fclose(fp);
    size_t resident_size = (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);
    return { resident_size, resident_size };
#endif
}
