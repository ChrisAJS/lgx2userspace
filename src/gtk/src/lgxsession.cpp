#include "gdkpixbufvideooutput.h"
#include <liblgx.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "lgxsession.h"

void onDrawingAreaDraw(__attribute__((unused)) GtkDrawingArea *drawingArea, cairo_t *cairoContext,
                       __attribute__((unused)) int width, __attribute__((unused)) int height, gpointer data) {
    auto *lgxSession = (LGXSession *) data;
    GdkPixbuf *frame = lgxSession->frame();

    gdk_cairo_set_source_pixbuf(cairoContext, frame, 0, 0);
    cairo_paint(cairoContext);
}

void
onDrawingAreaDrawPlaceholder(__attribute__((unused)) GtkDrawingArea *drawingArea, cairo_t *cairoContext,
                             __attribute__((unused)) int width, __attribute__((unused)) int height,
                             __attribute__((unused)) gpointer data) {
    cairo_set_source_rgb(cairoContext, 0.2, 0.2, 0.2);
    cairo_fill(cairoContext);
    cairo_paint(cairoContext);
}

LGXSession::LGXSession(GtkWidget *drawingArea) : drawingArea{drawingArea} {
    logger = new NOOPLogger;
    stream = new libusb::UsbStream();
    videoOutput = new GdkPixbufVideoOutput(drawingArea);
    audioOutput = new sdl::SdlFrameOutput();
    device = new lgx2::Device(stream, videoOutput, audioOutput, logger);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawingArea), onDrawingAreaDrawPlaceholder, this, nullptr);
}

void LGXSession::startCapture() {
#ifdef GC550_SUPPORT
    bool lgxAvailable = device->isDeviceAvailable(lgx2::DeviceType::LGX);
    bool lgx2Available = device->isDeviceAvailable(lgx2::DeviceType::LGX2);

    if (lgxAvailable) {
        device->initialise(lgx2::DeviceType::LGX);
    } else if (lgx2Available) {
        device->initialise(lgx2::DeviceType::LGX2);
    } else {
        g_print("Attempted to start capture, but no LGX or LGX2 were detected\n");
    }
#else
    if (device->isDeviceAvailable(lgx2::DeviceType::LGX2)) {
        device->initialise(lgx2::DeviceType::LGX2);
    } else {
        g_print("Attempted to start capture, but LGX2 was not detected\n");
    }

#endif
    g_object_ref(drawingArea);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawingArea), onDrawingAreaDraw, this, nullptr);
}

void LGXSession::run() {
    device->run();
}

void LGXSession::stopCapture() {
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawingArea), onDrawingAreaDrawPlaceholder, this, nullptr);
    g_object_unref(drawingArea);

    if (device != nullptr) {
        device->shutdown();
        delete device;
    }
    delete stream;
    stream = new libusb::UsbStream();
    device = new lgx2::Device(stream, videoOutput, audioOutput, logger);
}

GdkPixbuf *LGXSession::frame() {
    return videoOutput->frame();
}
