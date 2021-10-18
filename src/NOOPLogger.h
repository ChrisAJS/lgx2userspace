#ifndef LGX2USERSPACE_NOOPLOGGER_H
#define LGX2USERSPACE_NOOPLOGGER_H


#include "lgxdevice.h"

class NOOPLogger : public lgx2::Logger {
public:
    void logTimeStart(const std::string &name) override;

    void logTimeEnd(const std::string &name, const std::string &message) override;

    void summarise() override;
};


#endif //LGX2USERSPACE_NOOPLOGGER_H
