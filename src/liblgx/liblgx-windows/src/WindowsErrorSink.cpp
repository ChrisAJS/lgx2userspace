#include "WindowsErrorSink.h"

#include <windows.h>
#include <winuser.h>

namespace error {
    void WindowsErrorSink::catchErrors(const std::function<void()> &run) {
        try {
            run();
        } catch(std::exception &e) {
            MessageBox(nullptr, e.what(), "lgx2userspace error", MB_ICONERROR);
            exit(-1);
        }
    }
}