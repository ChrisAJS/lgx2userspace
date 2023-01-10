#include "ChronoLogger.h"

#include <chrono>
#include <iostream>
#include <algorithm>
#include <fstream>

ChronoLogger::ChronoLogger(bool summaryOnly) : _logfile{"lgx2userspace.log", std::ios::app}, _summaryOnly{summaryOnly} {
    _appStart = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

    _logfile << "\n\nApp started at - " << _appStart << std::endl;
}

void ChronoLogger::logTimeStart(const std::string &name) {
    long nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    _starts[name] = nanos;

    if (_counts.find(name) == _counts.end()) {
        _counts[name] = 0;
        _times[name] = 0;
        _summaries[name] = std::vector<uint64_t>();
    }

    _counts[name] = _counts[name] + 1;
}

void ChronoLogger::logTimeEnd(const std::string &name, const std::string &message) {
    long start = _starts[name];
    long nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

    _times[name] = _times[name] + (nanos - start);

    if (_counts[name] == 10) {
        _counts[name] = 0;
        if (!_summaryOnly) {
            _logfile << message << " - avg. ns.: " << _times[name] / 50 << std::endl;
        }
        _summaries[name].push_back(_times[name] / 50);
        _times[name] = 0;
    }
}

void ChronoLogger::summarise() {
    long endTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    uint64_t durationNanos = endTime - _appStart;
    double durationSeconds = durationNanos / 1000000000.0;

    _logfile << std::endl << "Summary (Include this in bug reports)" << std::endl;
    _logfile << "Total run time: " << durationNanos << "ns (" << durationSeconds << "s)" << std::endl;

    for (auto key = _summaries.begin(); key != _summaries.end(); key++) {
        uint64_t total = 0;
        for (uint64_t value: (*key).second) {
            total += value;
        }
        _logfile << (*key).first << " - average ns: " << total / (*key).second.size() << " (" << total / (*key).second.size() /1000000.0 << "ms)" << std::endl;
    }
    _logfile.close();
}
