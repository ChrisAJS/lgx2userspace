#ifndef LGX2USERSPACE_GLFWVIDEOOUTPUT_H
#define LGX2USERSPACE_GLFWVIDEOOUTPUT_H

#include "lgxdevice.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace glfw {

    class GlfwVideoOutput : public lgx2::VideoOutput {
    public:
        GlfwVideoOutput();
        ~GlfwVideoOutput() noexcept;

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

        uint8_t *yuvImage;
        uint8_t *y;
        uint8_t *u;
        uint8_t *v;

        void populateYuvImageFromFrame(uint32_t *image);

        void populateTexture(uint8_t *textureData) const;

        GLuint compileShader(int shaderType, const char *shaderSource) const;

        GLuint compileShaderProgram() const;
    };

}

#endif //LGX2USERSPACE_GLFWVIDEOOUTPUT_H
