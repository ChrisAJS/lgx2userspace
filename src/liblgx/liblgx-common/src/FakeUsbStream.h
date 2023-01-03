#ifndef LGX2USERSPACE_FAKEUSBSTREAM_H
#define LGX2USERSPACE_FAKEUSBSTREAM_H

#include "lgxdevice.h"

class FakeUsbStream : public lgx2::Stream {
public:
    explicit FakeUsbStream();
    ~FakeUsbStream() override = default;

    bool deviceAvailable(lgx2::DeviceType deviceType) override;

    void streamSetupCommands(lgx2::DeviceType deviceType) override;

    void queueFrameRead(std::function<void(uint8_t *)> *onData) override;

    void update() override;

    void shutdownStream() override;
private:
    std::function<void(uint8_t*)> *_onFrameDataCallback;
};


#endif //LGX2USERSPACE_FAKEUSBSTREAM_H
