// Based on the work done by MasterAler (https://github.com/MasterAler/SampleYUVRenderer). Original shader code and OGL code lovingly adapted.

#include "GlfwVideoOutput.h"

#include <stdexcept>
#include <GLFW/glfw3.h>

#include "glfwdata.h"

namespace glfw {
    GlfwVideoOutput::GlfwVideoOutput() {
        yuvImage = new uint8_t[1920*1080*3];
        y = yuvImage;
        u = yuvImage + 1920*1080;
        v = u + 1920*1080;
    }

    GlfwVideoOutput::~GlfwVideoOutput() noexcept {
        delete[] yuvImage;
    }

    void GlfwVideoOutput::initialiseVideo() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialise GLFW3");
        }

        window = glfwCreateWindow(1920, 1080, "lgx2userspace - glfw", nullptr, nullptr);

        if (!window) {
            throw std::runtime_error("Failed to create GLFW3 window");
        }

        glfwMakeContextCurrent(window);

        gladLoadGL();

        glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int, int, int) {
            if (key == GLFW_KEY_ESCAPE) {
                glfwSetWindowShouldClose(window, 1);
            }
        });

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        GLuint shaderProgram = compileShaderProgram();

        textureUniformY = glGetUniformLocation(shaderProgram, "tex_y");
        textureUniformU = glGetUniformLocation(shaderProgram, "tex_u");
        textureUniformV = glGetUniformLocation(shaderProgram, "tex_v");

        glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, vertexVertices);
        glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, textureVertices);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glGenTextures(3, textures);
    }

    void GlfwVideoOutput::display() {
        glfwPollEvents();

        glfwSwapBuffers(window);

        if (glfwWindowShouldClose(window)) {
            throw std::runtime_error("Window closed");
        }
    }

    void GlfwVideoOutput::shutdownVideo() {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    GLuint GlfwVideoOutput::compileShaderProgram() const {
        GLuint shaderProgram = glCreateProgram();

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

        glAttachShader(shaderProgram, fragmentShader);
        glAttachShader(shaderProgram, vertexShader);

        glBindAttribLocation(shaderProgram, 0, "vertexIn");
        glBindAttribLocation(shaderProgram, 1, "textureIn");

        glLinkProgram(shaderProgram);
        glUseProgram(shaderProgram);

        return shaderProgram;
    }

    GLuint GlfwVideoOutput::compileShader(int shaderType, const char *shaderSource) const {
        GLuint vertexShader = glCreateShader(shaderType);
        glShaderSource(vertexShader, 1, &shaderSource, nullptr);
        glCompileShader(vertexShader);

        int vertexMaxLength = 4096;
        GLchar vertexErrorLog[4096]{0};
        glGetShaderInfoLog(vertexShader, vertexMaxLength, &vertexMaxLength, &vertexErrorLog[0]);

        if (vertexMaxLength > 0) {
            fprintf(stderr, "Shader error: %s\n", vertexErrorLog);
            throw std::runtime_error("Failed to compile shader.");
        }
        return vertexShader;
    }

    void GlfwVideoOutput::videoFrameAvailable(uint32_t *image) {
        populateYuvImageFromFrame(image);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        populateTexture(yuvImage);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        populateTexture(yuvImage + 1920*1080);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textures[2]);
        populateTexture(yuvImage + 1920*1080 * 2);

        glUniform1i(textureUniformY, 0);
        glUniform1i(textureUniformU, 1);
        glUniform1i(textureUniformV, 2);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void GlfwVideoOutput::populateTexture(uint8_t *textureData) const {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, textureData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void GlfwVideoOutput::populateYuvImageFromFrame(uint32_t *image) {
        uint8_t *rawImage = reinterpret_cast<uint8_t *>(image);

        for(int i = 0; i < 1920*1080; i++) {
            y[i] = rawImage[i * 2];
        }

        for(int i = 0; i < 1920*1080; i+=2) {
            u[i] = rawImage[i * 2 + 1];
            u[i + 1] = rawImage[i * 2 + 1];
        }

        for(int i = 1; i < 1920*1080; i+=2) {
            v[i] = rawImage[i * 2 + 1];
            v[i + 1] = rawImage[i * 2 + 1];
        }
    }
}