#include "processstats.h"
#include <QThread>
#include <algorithm>

#if defined(Q_OS_WIN)
    #ifndef PSAPI_VERSION
        #define PSAPI_VERSION 2 // K32GetProcessMemoryInfo lives in kernel32, no extra library needed
    #endif
    #include <windows.h>
    #include <psapi.h>
#elif defined(Q_OS_LINUX)
    #include <QFile>
    #include <QByteArray>
    #include <QList>
    #include <unistd.h>
#elif defined(Q_OS_MACOS)
    #include <sys/resource.h>
    #include <mach/mach.h>
#endif

namespace {

// cpuNs: total cpu time (user + kernel) of this process in ns, memory: resident bytes
bool readProcess(qint64 &cpuNs, qint64 &memory) {
#if defined(Q_OS_WIN)
    FILETIME creation, exitTime, kernel, user;
    HANDLE process = GetCurrentProcess();
    if(!GetProcessTimes(process, &creation, &exitTime, &kernel, &user))
        return false;
    auto toNs = [](const FILETIME &time) {
        ULARGE_INTEGER value;
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        return static_cast<qint64>(value.QuadPart) * 100; // 100 ns units
    };
    cpuNs = toNs(kernel) + toNs(user);
    PROCESS_MEMORY_COUNTERS counters;
    counters.cb = sizeof(counters);
    if(!K32GetProcessMemoryInfo(process, &counters, sizeof(counters)))
        return false;
    memory = static_cast<qint64>(counters.WorkingSetSize);
    return true;
#elif defined(Q_OS_LINUX)
    long ticks = sysconf(_SC_CLK_TCK);
    long pageSize = sysconf(_SC_PAGESIZE);
    if(ticks <= 0 || pageSize <= 0)
        return false;
    QFile stat("/proc/self/stat");
    if(!stat.open(QIODevice::ReadOnly))
        return false;
    QByteArray line = stat.readAll();
    // the process name (field 2) may contain spaces and parentheses: parse after the last closing one
    int close = line.lastIndexOf(')');
    if(close < 0)
        return false;
    const QList<QByteArray> fields = line.mid(close + 2).split(' ');
    // fields[0] is field 3 (state), so utime (14) is [11] and stime (15) is [12]
    if(fields.size() < 13)
        return false;
    qint64 utime = fields.at(11).toLongLong();
    qint64 stime = fields.at(12).toLongLong();
    cpuNs = (utime + stime) * 1000000000LL / ticks;

    QFile statm("/proc/self/statm");
    if(!statm.open(QIODevice::ReadOnly))
        return false;
    const QList<QByteArray> pages = statm.readAll().simplified().split(' ');
    if(pages.size() < 2)
        return false;
    memory = pages.at(1).toLongLong() * pageSize; // resident pages
    return true;
#elif defined(Q_OS_MACOS)
    struct rusage usage;
    if(getrusage(RUSAGE_SELF, &usage) != 0)
        return false;
    cpuNs = (static_cast<qint64>(usage.ru_utime.tv_sec) + usage.ru_stime.tv_sec) * 1000000000LL
          + (static_cast<qint64>(usage.ru_utime.tv_usec) + usage.ru_stime.tv_usec) * 1000LL;
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        return false;
    memory = static_cast<qint64>(info.resident_size);
    return true;
#else
    Q_UNUSED(cpuNs)
    Q_UNUSED(memory)
    return false;
#endif
}

}

bool ProcessStats::sample() {
    qint64 cpuNs = 0, memory = 0;
    if(!readProcess(cpuNs, memory))
        return false;
    if(!mWall.isValid())
        mWall.start();
    qint64 wallNs = mWall.nsecsElapsed();
    if(mLastCpuNs >= 0 && wallNs > mLastWallNs) {
        double cores = qMax(1, QThread::idealThreadCount());
        double busy = static_cast<double>(cpuNs - mLastCpuNs) / static_cast<double>(wallNs - mLastWallNs);
        mCpu = std::clamp(busy / cores * 100.0, 0.0, 100.0);
        mHasCpu = true;
    }
    mLastCpuNs = cpuNs;
    mLastWallNs = wallNs;
    mMemory = memory;
    return true;
}

bool ProcessStats::hasCpu() const { return mHasCpu; }
double ProcessStats::cpuPercent() const { return mCpu; }
qint64 ProcessStats::memoryBytes() const { return mMemory; }
