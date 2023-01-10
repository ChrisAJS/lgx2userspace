#include "V4LFrameOutput.h"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>

v4l::V4LFrameOutput::V4LFrameOutput(const std::string &deviceName) {
    _v4l2fd = open(deviceName.c_str(), O_RDWR);
}

void v4l::V4LFrameOutput::initialiseVideo(lgx2::VideoScale scale) {
    _frameBuffer = new uint8_t[1920 * 1080 * 2];
    if (_v4l2fd == -1) {
        throw std::runtime_error("Failed to open V4L2 Loopback device");
    }

    struct v4l2_format v{};
    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(_v4l2fd, VIDIOC_G_FMT, &v) == -1) {
        throw std::runtime_error("Cannot setup V4L2 Loopback device for video output");
    }
    v.fmt.pix.width = 1920;
    v.fmt.pix.height = 1080;
    v.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    v.fmt.pix.sizeimage = 1920 * 1080 * 4;
    v.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(_v4l2fd, VIDIOC_S_FMT, &v) == -1) {
        throw std::runtime_error("Cannot setup V4L2 Loopback device");
    }
}

void v4l::V4LFrameOutput::videoFrameAvailable(uint32_t *image) {
    memcpy(_frameBuffer, image, 1920 * 1080 * 2);
}

void v4l::V4LFrameOutput::display() {
    ssize_t bytesWritten = write(_v4l2fd, _frameBuffer, 1920 * 1080 * 2);
    if (bytesWritten < 1920 * 1080 * 2) {
        printf("Only wrote %ld bytes (expected %d)\n", bytesWritten, 1920 * 1080 * 2);
    }
}

void v4l::V4LFrameOutput::shutdownVideo() {
    close(_v4l2fd);
}
