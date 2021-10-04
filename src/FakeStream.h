#ifndef LGX2USERSPACE_FAKESTREAM_H
#define LGX2USERSPACE_FAKESTREAM_H

#include "lgxdevice.h"

namespace fake {
    class FakeStream : public lgx2::Stream {
    public:
        FakeStream();

        void streamSetupCommands() override;

        void queueFrameRead(std::function<void(uint8_t *, uint32_t)> *onData) override;

        void update() override;

    private:
        std::function<void(uint8_t *, uint32_t)> *_onData;
        uint8_t *_fakeFrameBuffer;

        FILE *_fakeFrameSource;
    };
}


#endif //LGX2USERSPACE_FAKESTREAM_H
