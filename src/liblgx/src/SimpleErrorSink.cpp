#include "SimpleErrorSink.h"

#include <iostream>

namespace error {
    void SimpleErrorSink::catchErrors(const std::function<void()> &run) {
        try {
            run();
        } catch (std::exception &e) {
            std::cerr << "Error occurred: " << e.what() << std::endl;
            exit(-1);
        }
    }
}