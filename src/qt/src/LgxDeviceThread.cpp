#include "LgxDeviceThread.h"

LgxDeviceThread::LgxDeviceThread(lgx2::Device *device) : _device{device} {

}

void LgxDeviceThread::run() {

    _device->initialise(lgx2::DeviceType::LGX2);

    forever {
        if (QThread::currentThread()->isInterruptionRequested()) {
            _device->shutdown();
            return;
        }

        _device->run();
    }
}
