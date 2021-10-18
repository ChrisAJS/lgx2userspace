#include "ChronoLogger.h"

#include <chrono>
#include <iostream>

void ChronoLogger::logTimeStart(const std::string &name) {
    long nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    _starts[name] = nanos;

    if (_counts.find(name) == _counts.end()) {
        _counts[name] = 0;
        _times[name] = 0;
    }

    _counts[name] = _counts[name] + 1;
}

void ChronoLogger::logTimeEnd(const std::string &name, const std::string &message) {
    long start = _starts[name];
    long nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    _times[name] = _times[name] + (nanos-start);

    if (_counts[name] == 10) {
        _counts[name] = 0;
        std::cout << message << " - avg. ns.: "<< _times[name] / 10 << std::endl;
        _times[name] = 0;
    }
}
