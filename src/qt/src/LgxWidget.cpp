#include <QPainter>
#include "LgxWidget.h"

static int convert_yuv_to_rgb_pixel(int y, int u, int v);
static int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb);
static int clamp(int value);

void LgxWidget::initialiseVideo() {
}

void LgxWidget::videoFrameAvailable(uint32_t *image) {
    convert_yuv_to_rgb_buffer((uint8_t*)image, _image.bits());
}

void LgxWidget::display() {
    repaint();
}

void LgxWidget::shutdownVideo() {
    qDebug("Shutting down video");
}

void LgxWidget::paintEvent(QPaintEvent *event) {
    QPainter painter{this};
    painter.drawImage(0, 0, _image);
}

static int convert_yuv_to_rgb_pixel(int y, int u, int v) {
    int pixel32 = 0;
    auto *pixel = (unsigned char *) &pixel32;

    pixel[0] = clamp(y + (1.370705 * (v - 128)));
    pixel[1] = clamp(y - (0.698001 * (v - 128)) - (0.337633 * (u - 128)));
    pixel[2] = clamp(y + (1.732446 * (u - 128)));
    return pixel32;
}

static int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb) {
    unsigned int width = 1920;
    unsigned int height = 1080;
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
    int y0, u, y1, v;

    for (in = 0; in < width * height * 2; in += 4) {
        pixel_16 =
                yuv[in + 3] << 24 |
                yuv[in + 2] << 16 |
                yuv[in + 1] << 8 |
                yuv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u = (pixel_16 & 0x0000ff00) >> 8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v = (pixel_16 & 0xff000000) >> 24;
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

static int clamp(int value) {
    if (value <= 0) {
        return 0;
    } else if (value >= 255) {
        return 255;
    }
    return value;
}