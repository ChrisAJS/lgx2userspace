#ifndef LGX2USERSPACE_NULLFRAMEOUTPUT_H
#define LGX2USERSPACE_NULLFRAMEOUTPUT_H

#include "lgxdevice.h"

namespace fake {
    class NullFrameOutput : public lgx2::FrameOutput {
    public:
        void videoFrameAvailable(uint32_t *image) override;

        void audioFrameAvailable(uint32_t *audio) override;

        void render() override;
    };
}


#endif //LGX2USERSPACE_NULLFRAMEOUTPUT_H
