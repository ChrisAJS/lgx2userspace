#ifndef LGX2USERSPACE_NULLVIDEOOUTPUT_H
#define LGX2USERSPACE_NULLVIDEOOUTPUT_H

#include "lgxdevice.h"

class NullVideoOutput : public lgx2::VideoOutput {
public:
    void initialiseVideo(lgx2::VideoScale scale) override;

    void videoFrameAvailable(uint32_t *image) override;

    void display() override;

    void shutdownVideo() override;
};


#endif //LGX2USERSPACE_NULLVIDEOOUTPUT_H
