#ifndef LGX2USERSPACE_WINDOWSERRORSINK_H
#define LGX2USERSPACE_WINDOWSERRORSINK_H

#include <lgxdevice.h>

namespace error {

    class WindowsErrorSink : public lgx2::ErrorSink {
    public:
        void catchErrors(const std::function<void ()> &run) override;
    };

} // error

#endif //LGX2USERSPACE_WINDOWSERRORSINK_H
