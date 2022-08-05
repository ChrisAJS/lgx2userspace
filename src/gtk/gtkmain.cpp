#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <liblgx.h>

class GdkPixbufVideoOutput : public lgx2::VideoOutput {
public:
    GdkPixbufVideoOutput(GtkWidget *drawingArea) : drawingArea{drawingArea} {

    }

    void initialiseVideo() override {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 0, 8, 1920, 1080);
    }

    void videoFrameAvailable(uint32_t *image) override {
        convert_yuv_to_rgb_buffer(reinterpret_cast<guchar *>(image), gdk_pixbuf_get_pixels(pixbuf));

    }

    void display() override {
        gtk_widget_queue_draw(drawingArea);
    }

    void shutdownVideo() override {

    }

    GdkPixbuf *getPixbuf() {
        return this->pixbuf;
    }

    inline int clamp(int value) {
        if (value <= 0) {
            return 0;
        } else if (value >= 255) {
            return 255;
        }
        return value;
    }

    int convert_yuv_to_rgb_pixel(int y, int u, int v)
    {
        int pixel32 = 0;
        auto *pixel = (unsigned char *)&pixel32;

        pixel[0] = clamp(y + (1.370705 * (v - 128)));
        pixel[1] = clamp(y - (0.698001 * (v - 128)) - (0.337633 * (u - 128)));
        pixel[2] = clamp(y + (1.732446 * (u - 128)));
        return pixel32;
    }

    int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb)
    {
        unsigned int width = 1920;
        unsigned int height = 1080;
        unsigned int in, out = 0;
        unsigned int pixel_16;
        unsigned char pixel_24[3];
        unsigned int pixel32;
        int y0, u, y1, v;

        for(in = 0; in < width * height * 2; in += 4)
        {
            pixel_16 =
                    yuv[in + 3] << 24 |
                    yuv[in + 2] << 16 |
                    yuv[in + 1] <<  8 |
                    yuv[in + 0];
            y0 = (pixel_16 & 0x000000ff);
            u  = (pixel_16 & 0x0000ff00) >>  8;
            y1 = (pixel_16 & 0x00ff0000) >> 16;
            v  = (pixel_16 & 0xff000000) >> 24;
            pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
            pixel_24[0] = (pixel32 & 0x000000ff);
            pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
            pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
            rgb[out++] = pixel_24[0];
            rgb[out++] = pixel_24[1];
            rgb[out++] = pixel_24[2];
            pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
            pixel_24[0] = (pixel32 & 0x000000ff);
            pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
            pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
            rgb[out++] = pixel_24[0];
            rgb[out++] = pixel_24[1];
            rgb[out++] = pixel_24[2];
        }
        return 0;

    }
private:
    GtkWidget *drawingArea;

    GdkPixbuf *pixbuf;
};

lgx2::Device *device{nullptr};
libusb::UsbStream *stream{nullptr};
lgx2::VideoOutput *videoOutput{nullptr};
lgx2::AudioOutput *audioOutput{nullptr};
lgx2::Logger *logger{nullptr};

gboolean
expose_event_callback (GtkWidget *widget, cairo_t *cairoContext, gpointer data)
{
    device->run();
    GdkPixbufVideoOutput *boop = (GdkPixbufVideoOutput*)data;
    gdk_cairo_set_source_pixbuf(cairoContext, boop->getPixbuf(), 0, 0);
    cairo_paint(cairoContext);
    return TRUE;
}

void size_allocate (
        GtkWidget* self,
        GtkAllocation* allocation,
        __attribute__((unused)) gpointer user_data) {
    if (allocation->width == 1920 && allocation->height == 1080) {
        logger = new NOOPLogger;
        stream = new libusb::UsbStream(libusb::LGXDeviceType::LGX);
        videoOutput = new GdkPixbufVideoOutput(self);
        audioOutput = new sdl::SdlFrameOutput();
        device = new lgx2::Device(stream, videoOutput, audioOutput, logger);
        device->initialise();

        g_signal_connect(G_OBJECT(self), "draw", G_CALLBACK(expose_event_callback), videoOutput);
    } else {
        g_print("Allocation is incorrect\n");
    }
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    GtkWindow *window;
    window = (GtkWindow*)gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(window, "LGX/LGX2 Userspace Driver");

    static GtkWidget *gtk_da;
    gtk_da = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), gtk_da);
    gtk_widget_show_all(GTK_WIDGET(window));

    gtk_widget_set_size_request(GTK_WIDGET(gtk_da), 1920, 1080);

    g_signal_connect(G_OBJECT(gtk_da),
                     "size-allocate", G_CALLBACK(size_allocate), NULL);

    g_signal_connect(G_OBJECT(window),
                     "destroy", gtk_main_quit, NULL);
    gtk_main();

    if (device != nullptr) {
        device->shutdown();
    }

    return 0;
}