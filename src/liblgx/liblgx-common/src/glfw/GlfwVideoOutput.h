#ifndef LGX2USERSPACE_GLFWVIDEOOUTPUT_H
#define LGX2USERSPACE_GLFWVIDEOOUTPUT_H

#include "lgxdevice.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace glfw {

    class GlfwVideoOutput : public lgx2::VideoOutput {
    public:
        GlfwVideoOutput();

        void initialiseVideo() override;

        void videoFrameAvailable(uint32_t *image) override;

        void display() override;

        void shutdownVideo() override;

    private:
        GLFWwindow *window{nullptr};

        GLuint textureUniformY;
        GLuint textureUniformU;
        GLuint textureUniformV;

        GLuint textures[3]{0};
    };

}

#endif //LGX2USERSPACE_GLFWVIDEOOUTPUT_H
