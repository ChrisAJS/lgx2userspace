#ifndef LGX2USERSPACE_GDKPIXBUFVIDEOOUTPUT_H
#define LGX2USERSPACE_GDKPIXBUFVIDEOOUTPUT_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "lgxdevice.h"

class GdkPixbufVideoOutput : public lgx2::VideoOutput {
public:
    GdkPixbufVideoOutput(struct _GtkWidget *drawingArea);

    void initialiseVideo() override;

    void videoFrameAvailable(__uint32_t *image) override;

    void display() override;

    void shutdownVideo() override;

    GdkPixbuf *frame();

private:
    inline int clamp(int value);

    int convert_yuv_to_rgb_pixel(int y, int u, int v);

    int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb);

private:
    GtkWidget *drawingArea;

    GdkPixbuf *pixbuf;
};

#endif //LGX2USERSPACE_GDKPIXBUFVIDEOOUTPUT_H
