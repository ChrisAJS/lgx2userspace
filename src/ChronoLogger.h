#ifndef LGX2USERSPACE_CHRONOLOGGER_H
#define LGX2USERSPACE_CHRONOLOGGER_H

#include <map>
#include "lgxdevice.h"

class ChronoLogger : public lgx2::Logger {
public:
    void logTimeStart(const std::string &name) override;

    void logTimeEnd(const std::string &name, const std::string &message) override;
private:
    std::map<std::string, uint64_t> _starts;
    std::map<std::string, uint64_t> _times;
    std::map<std::string, uint32_t> _counts;
};


#endif //LGX2USERSPACE_CHRONOLOGGER_H
