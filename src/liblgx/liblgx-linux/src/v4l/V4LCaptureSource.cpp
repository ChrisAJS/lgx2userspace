#include "V4LCaptureSource.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

namespace v4l {

    std::string V4LCaptureSource::find(const std::string &cardName) {
        for (int index = 0; index < 64; index++) {
            std::string path = "/dev/video" + std::to_string(index);
            int fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
            if (fd == -1) continue;

            v4l2_capability capability{};
            bool match = ioctl(fd, VIDIOC_QUERYCAP, &capability) == 0 &&
                         cardName == reinterpret_cast<const char *>(capability.card) &&
                         (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE);
            close(fd);
            if (match) return path;
        }
        return "";
    }

    V4LCaptureSource::V4LCaptureSource(const std::string &path) : _path{path} {
        _fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
        if (_fd == -1) {
            throw std::runtime_error("Could not open " + path + ": " + strerror(errno));
        }

        try {
            v4l2_format format{};
            format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(_fd, VIDIOC_G_FMT, &format) == -1) {
                throw std::runtime_error(path + " is not a video capture device");
            }
            if (format.fmt.pix.width != WIDTH || format.fmt.pix.height != HEIGHT ||
                format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
                throw std::runtime_error(path + " does not deliver 1920x1080 YUYV (is it lgx2d's device?)");
            }
            startStreaming();
        } catch (...) {
            release();
            throw;
        }
    }

    V4LCaptureSource::~V4LCaptureSource() {
        release();
    }

    void V4LCaptureSource::startStreaming() {
        v4l2_requestbuffers request{};
        request.count = BUFFER_COUNT;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        if (ioctl(_fd, VIDIOC_REQBUFS, &request) == -1 || request.count == 0) {
            if (errno == EBUSY) {
                // v4l2loopback allows one capturing application per device
                throw std::runtime_error(_path + " is busy - another application (OBS?) is already capturing from it");
            }
            throw std::runtime_error(_path + ": could not allocate capture buffers: " + strerror(errno));
        }
        _bufferCount = request.count < BUFFER_COUNT ? request.count : BUFFER_COUNT;

        for (unsigned i = 0; i < _bufferCount; i++) {
            v4l2_buffer buffer{};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            if (ioctl(_fd, VIDIOC_QUERYBUF, &buffer) == -1) {
                throw std::runtime_error(_path + ": could not query capture buffer: " + strerror(errno));
            }
            void *start = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, buffer.m.offset);
            if (start == MAP_FAILED) {
                throw std::runtime_error(_path + ": could not map capture buffer: " + strerror(errno));
            }
            _buffers[i] = {start, buffer.length};
            if (ioctl(_fd, VIDIOC_QBUF, &buffer) == -1) {
                throw std::runtime_error(_path + ": could not queue capture buffer: " + strerror(errno));
            }
        }

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(_fd, VIDIOC_STREAMON, &type) == -1) {
            throw std::runtime_error(_path + ": could not start streaming: " + strerror(errno));
        }
        _streaming = true;
    }

    void V4LCaptureSource::release() {
        if (_fd == -1) return;
        if (_streaming) {
            v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(_fd, VIDIOC_STREAMOFF, &type);
            _streaming = false;
        }
        for (unsigned i = 0; i < _bufferCount; i++) {
            if (_buffers[i].start != nullptr) munmap(_buffers[i].start, _buffers[i].length);
            _buffers[i] = {};
        }
        _bufferCount = 0;
        close(_fd);
        _fd = -1;
    }

    bool V4LCaptureSource::readFrame(uint8_t *frame, int timeoutMs) {
        pollfd descriptor{_fd, POLLIN, 0};
        if (poll(&descriptor, 1, timeoutMs) <= 0 || !(descriptor.revents & POLLIN)) return false;

        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        if (ioctl(_fd, VIDIOC_DQBUF, &buffer) == -1) return false;

        bool complete = buffer.index < _bufferCount && buffer.bytesused >= static_cast<uint32_t>(FRAME_BYTES);
        if (complete) memcpy(frame, _buffers[buffer.index].start, FRAME_BYTES);
        ioctl(_fd, VIDIOC_QBUF, &buffer); // hand it straight back
        return complete;
    }
}
