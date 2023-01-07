#ifndef LGX2USERSPACE_SIMPLEERRORSINK_H
#define LGX2USERSPACE_SIMPLEERRORSINK_H

#include <lgxdevice.h>

namespace error {

    class SimpleErrorSink : public lgx2::ErrorSink {
    public:
        void catchErrors(const std::function<void()> &run) override;
    };

} // error

#endif //LGX2USERSPACE_SIMPLEERRORSINK_H
