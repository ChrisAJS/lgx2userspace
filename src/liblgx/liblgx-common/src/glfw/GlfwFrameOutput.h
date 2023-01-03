#ifndef LGX2USERSPACE_GLFWFRAMEOUTPUT_H
#define LGX2USERSPACE_GLFWFRAMEOUTPUT_H

#include "lgxdevice.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace glfw {

    class GlfwFrameOutput : public lgx2::VideoOutput, public lgx2::AudioOutput {
    public:
        GlfwFrameOutput();

        void initialiseVideo() override;

        void videoFrameAvailable(uint32_t *image) override;

        void display() override;

        void shutdownVideo() override;

        void initialiseAudio() override;

        void audioFrameAvailable(uint32_t *audio) override;

        void render() override;

        void shutdownAudio() override;

    private:
        GLFWwindow *window{nullptr};

        GLuint textureUniformY;
        GLuint textureUniformU;
        GLuint textureUniformV;

        GLuint textures[3]{0};
    };

}

#endif //LGX2USERSPACE_GLFWFRAMEOUTPUT_H
