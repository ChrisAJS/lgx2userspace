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

        void initialiseVideo(lgx2::VideoScale scale) override;

        void videoFrameAvailable(uint32_t *image) override;

        void display() override;

        void shutdownVideo() override;

    private:
        GLFWwindow *window{nullptr};

        GLint textureUniformY{-1};
        GLint textureUniformU{-1};
        GLint textureUniformV{-1};

        GLuint textures[3]{0};

        uint8_t *yuvImage;
        uint8_t *y;
        uint8_t *u;
        uint8_t *v;

        void populateYuvImageFromFrame(uint32_t *image);

        static void populateTexture(uint8_t *textureData) ;

        static GLuint compileShader(int shaderType, const char *shaderSource) ;

        GLuint compileShaderProgram() const;
    };

}

#endif //LGX2USERSPACE_GLFWVIDEOOUTPUT_H
