#ifndef LGX2USERSPACE_LGXSESSION_H
#define LGX2USERSPACE_LGXSESSION_H

#include "lgxdevice.h"
#include "usb/UsbStream.h"
#include "gdkpixbufvideooutput.h"

class LGXSession {
public:
    LGXSession(struct _GtkWidget *drawingArea);

    void startCapture();

    void run();

    GdkPixbuf *frame();

    void stopCapture();

private:
    GtkWidget *drawingArea{nullptr};
    lgx2::Device *device{nullptr};
    libusb::UsbStream *stream{nullptr};
    GdkPixbufVideoOutput *videoOutput{nullptr};
    lgx2::AudioOutput *audioOutput{nullptr};
    lgx2::Logger *logger{nullptr};
};

#endif //LGX2USERSPACE_LGXSESSION_H
