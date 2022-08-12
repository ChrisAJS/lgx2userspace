//
// Created by Chris Sawczuk on 12/08/2022.
//

#include "SFMLFrameOutput.h"

namespace sfml {
    SFMLFrameOutput::SFMLFrameOutput() {
        frameBuffer = new uint32_t[1920 * 1080];
        window = new sf::RenderWindow(sf::VideoMode(1920, 1080), "lgx2userspace driver (SFML output)");

    }

    void SFMLFrameOutput::initialiseVideo() {
        buffer.create(1920, 1080);
        sprite = sf::Sprite{buffer};
    }

    void SFMLFrameOutput::videoFrameAvailable(uint32_t *image) {

        convert_yuv_to_rgb_buffer(reinterpret_cast<unsigned char *>(image),
                                  reinterpret_cast<unsigned char *>(frameBuffer));

        buffer.update(reinterpret_cast<unsigned char *>(frameBuffer), 1920, 1080, 0, 0);

        window->draw(sprite);
    }

    void SFMLFrameOutput::display() {
        window->display();
    }

    void SFMLFrameOutput::shutdownVideo() {
        if (window->isOpen()) {
            window->close();
        }
    }

    void SFMLFrameOutput::initialiseAudio() {

    }

    void SFMLFrameOutput::audioFrameAvailable(uint32_t *audio) {
        soundBuffer.loadFromSamples((sf::Int16*)audio, 1600, 2, 48000);
    }

    void SFMLFrameOutput::render() {
        sound.play();
    }

    void SFMLFrameOutput::shutdownAudio() {

    }

    void SFMLFrameOutput::whileRunning(const std::function<bool()> &run) {
        while (window->isOpen()) {
            if (!run()) {
                window->close();
            }
            sf::Event event;
            while (window->pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window->close();
                }
            }
        }
    }

    int SFMLFrameOutput::clamp(int value) {
        if (value <= 0) {
            return 0;
        } else if (value >= 255) {
            return 255;
        }
        return value;
    }

    int SFMLFrameOutput::convert_yuv_to_rgb_pixel(int y, int u, int v) {
        int pixel32 = 0;
        auto *pixel = (unsigned char *) &pixel32;

        pixel[0] = clamp(y + (1.370705 * (v - 128)));
        pixel[1] = clamp(y - (0.698001 * (v - 128)) - (0.337633 * (u - 128)));
        pixel[2] = clamp(y + (1.732446 * (u - 128)));
        return pixel32;
    }

    int SFMLFrameOutput::convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb) {
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
            rgb[out++] = 0xff;

            pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
            pixel_24[0] = (pixel32 & 0x000000ff);
            pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
            pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
            rgb[out++] = pixel_24[0];
            rgb[out++] = pixel_24[1];
            rgb[out++] = pixel_24[2];
            rgb[out++] = 0xff;
        }
        return 0;

    }
} // sfml