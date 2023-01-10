#include "MacOsErrorSink.h"

#include <iostream>

namespace error {
    void MacOsErrorSink::catchErrors(const std::function<void()> &run) {
        try {
            run();
        } catch(std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
            exit(-1);
        }
    }
}