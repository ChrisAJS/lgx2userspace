#ifndef LGX2USERSPACE_CHRONOLOGGER_H
#define LGX2USERSPACE_CHRONOLOGGER_H

#include <vector>
#include <map>
#include "lgxdevice.h"

class ChronoLogger : public lgx2::Logger {
public:
    ChronoLogger(bool summaryOnly);

    void logTimeStart(const std::string &name) override;

    void logTimeEnd(const std::string &name, const std::string &message) override;

    void summarise() override;

private:
    bool _summaryOnly;
    uint64_t _appStart;
    std::map<std::string, std::vector<uint64_t>> _summaries;
    std::map<std::string, uint64_t> _starts;
    std::map<std::string, uint64_t> _times;
    std::map<std::string, uint32_t> _counts;
};


#endif //LGX2USERSPACE_CHRONOLOGGER_H
