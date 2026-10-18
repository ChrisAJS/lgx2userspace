#include "V4LFrameOutput.h"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstdio>

v4l::V4LFrameOutput::V4LFrameOutput(const std::string &deviceName) : _deviceName{deviceName} {
    _v4l2fd = open(deviceName.c_str(), O_RDWR);
}

void v4l::V4LFrameOutput::initialiseVideo(lgx2::VideoScale) {
    if (_v4l2fd == -1) {
        throw std::runtime_error("Failed to open V4L2 Loopback device " + _deviceName);
    }

    struct v4l2_format v{};
    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(_v4l2fd, VIDIOC_G_FMT, &v) == -1) {
        throw std::runtime_error("Cannot setup V4L2 Loopback device for video output");
    }
    v.fmt.pix.width = WIDTH;
    v.fmt.pix.height = HEIGHT;
    v.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    v.fmt.pix.bytesperline = WIDTH * 2;
    v.fmt.pix.sizeimage = FRAME_BYTES;
    v.fmt.pix.field = V4L2_FIELD_NONE;
    v.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
    if (ioctl(_v4l2fd, VIDIOC_S_FMT, &v) == -1) {
        throw std::runtime_error("Cannot setup V4L2 Loopback device");
    }
}

void v4l::V4LFrameOutput::videoFrameAvailable(uint32_t *image) {
    // Written as it arrives, once per frame (writing from display() would repeat the same frame
    // at the caller's loop rate).
    ssize_t bytesWritten = write(_v4l2fd, image, FRAME_BYTES);
    if (bytesWritten < FRAME_BYTES && !_reportedShortWrite) {
        fprintf(stderr, "V4L2: only wrote %zd bytes (expected %d)\n", bytesWritten, FRAME_BYTES);
        _reportedShortWrite = true;
    }
}

void v4l::V4LFrameOutput::display() {
}

void v4l::V4LFrameOutput::shutdownVideo() {
    if (_v4l2fd != -1) close(_v4l2fd);
    _v4l2fd = -1;
}
