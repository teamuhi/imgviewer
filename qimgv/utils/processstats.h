#pragma once

#include <QElapsedTimer>
#include <QtGlobal>

// CPU / memory of the running process. sample() is cheap (a couple of syscalls), call it about once a second.
class ProcessStats {
public:
    // returns false when the platform is not supported (or reading failed)
    bool sample();
    // false until two samples exist (the first one only primes the counters)
    bool hasCpu() const;
    // share of the whole machine (all cores), 0..100
    double cpuPercent() const;
    // resident set / working set
    qint64 memoryBytes() const;

private:
    QElapsedTimer mWall;
    qint64 mLastCpuNs = -1;
    qint64 mLastWallNs = 0;
    bool mHasCpu = false;
    double mCpu = 0.0;
    qint64 mMemory = 0;
};
