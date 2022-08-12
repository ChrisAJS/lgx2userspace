#ifndef LGX2USERSPACE_SFMLFRAMEOUTPUT_H
#define LGX2USERSPACE_SFMLFRAMEOUTPUT_H

#include <lgxdevice.h>
#include <functional>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

namespace sfml {

    class SFMLFrameOutput : public lgx2::VideoOutput, public lgx2::AudioOutput {
    public:
        SFMLFrameOutput();

        void initialiseVideo() override;

        void videoFrameAvailable(uint32_t *image) override;

        void display() override;

        void shutdownVideo() override;

        void initialiseAudio() override;

        void audioFrameAvailable(uint32_t *audio) override;

        void render() override;

        void shutdownAudio() override;

    public:
        void whileRunning(const std::function<bool()> &run);

    private:
        int convert_yuv_to_rgb_pixel(int y, int u, int v);
        int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb);
        int clamp(int input);

    private:
        sf::RenderWindow *window;
        sf::Texture buffer;
        uint32_t *frameBuffer;
        sf::SoundBuffer soundBuffer{};
        sf::Sound sound{soundBuffer};
        sf::Sprite sprite;
    };

} // sfml

#endif //LGX2USERSPACE_SFMLFRAMEOUTPUT_H
