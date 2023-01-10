// Based on the work done by MasterAler (https://github.com/MasterAler/SampleYUVRenderer). Original shader code and OGL code lovingly adapted.

#include "GlfwVideoOutput.h"

#include <stdexcept>
#include <GLFW/glfw3.h>

#include "glfwdata.h"

namespace glfw {
    GlfwVideoOutput::GlfwVideoOutput() {

    }

    GlfwVideoOutput::~GlfwVideoOutput() noexcept {
        delete[] yuvImage;
    }

    void GlfwVideoOutput::initialiseVideo(lgx2::VideoScale scale) {
        targetScale = scale;
        int width = 1920;
        int height = 1080;
        if (scale == lgx2::VideoScale::Half) {
            width = 1920 / 2;
            height = 1080 / 2;
        } else if (scale == lgx2::VideoScale::Quarter) {
            width = 1920 / 4;
            height = 1080 / 4;
        }

        yuvImage = new uint8_t[width*height*3];
        bufferWidth = width;
        bufferHeight = height;
        bufferSize = width * height;
        y = yuvImage;
        u = yuvImage + bufferSize;
        v = u + bufferSize;

        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialise GLFW3");
        }

        window = glfwCreateWindow(width, height, "lgx2userspace - glfw", nullptr, nullptr);

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

        glfwSetWindowSizeCallback(window, [](GLFWwindow *, int width, int height) {
            glViewport(0, 0, width, height);
        });

        int viewportWidth, viewportHeight;
        glfwGetFramebufferSize(window, &viewportWidth, &viewportHeight);
        glViewport(0, 0, viewportWidth, viewportHeight);

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

        glfwSwapBuffers(window);

        glfwPollEvents();

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

    GLuint GlfwVideoOutput::compileShader(int shaderType, const char *shaderSource) {
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
        populateTexture(yuvImage + bufferSize);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textures[2]);
        populateTexture(yuvImage + bufferSize * 2);

        glUniform1i(textureUniformY, 0);
        glUniform1i(textureUniformU, 1);
        glUniform1i(textureUniformV, 2);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void GlfwVideoOutput::populateTexture(uint8_t *textureData) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, bufferWidth, bufferHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, textureData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void GlfwVideoOutput::populateYuvImageFromFrame(uint32_t *image) {
        auto *rawImage = reinterpret_cast<uint8_t *>(image);

        int step = 1;
        if (targetScale == lgx2::VideoScale::Half) {
            step = 2;
        } else if (targetScale == lgx2::VideoScale::Quarter) {
            step = 4;
        }

        if (targetScale == lgx2::VideoScale::Full) {
            for (int i = 0; i < 1920 * 1080; i += 2) {
                y[i] = rawImage[i * 2];
                y[i + 1] = rawImage[(i + 1) * 2];

                u[i] = rawImage[(i * 2 + 1)];
                u[i + 1] = u[i];

                v[i] = rawImage[((i + 1) * 2 + 1)];
                v[i + 1] = v[i];
            }
        } else if (targetScale == lgx2::VideoScale::Half) {
            for (int frameY = 0; frameY < 540; frameY++) {
                for (int x = 0; x < 960; x++) {
                    int frameOffset = (((frameY*2) * 1920) + x*2)*2;
                    y[frameY*960 + x] = rawImage[frameOffset];
                    y[frameY*960 + x + 1] = rawImage[(frameOffset + 2)];

                    u[frameY*960 + x] = rawImage[frameOffset + 1];
                    u[frameY*960 + x + 1] = u[frameY*960 + x];

                    v[frameY*960 + x] = rawImage[frameOffset + 3];
                    v[frameY*960 + x + 1] = v[frameY*960 + x];
                }
            }
        } else if (targetScale == lgx2::VideoScale::Quarter) {
            for (int frameY = 0; frameY < 270; frameY++) {
                for (int x = 0; x < 480; x++) {
                    int frameOffset = (((frameY*4) * 1920) + x*4)*2;
                    y[frameY*480 + x] = rawImage[frameOffset];
                    y[frameY*480 + x + 1] = rawImage[(frameOffset + 2)];

                    u[frameY*480 + x] = rawImage[frameOffset + 1];
                    u[frameY*480 + x + 1] = u[frameY*480 + x];

                    v[frameY*480 + x] = rawImage[frameOffset + 3];
                    v[frameY*480 + x + 1] = v[frameY*480 + x];
                }
            }
        }

//        if (pixels != nullptr && result == 0) {
//            if (_targetScale == lgx2::VideoScale::Full) {
//                memcpy(pixels, image, 1920 * 1080 * 2);
//            } else if (_targetScale == lgx2::VideoScale::Half) {
//                for (int y = 0; y < 540; y++) {
//                    for (int x = 0; x < 960; x++) {
//                        int frameOffset = ((y * 960) + x) << 1;
//                        int textureOffset = ((y * pitch/4) + x);
//                        pixels[textureOffset] = image[frameOffset];
//                    }
//                }
//            } else {
//                for (int y = 0; y < 1080; y += 4) {
//                    for (int x = 0; x < 1920; x += 4) {
//                        int frameOffset = y * 1920 + x;
//                        int textureOffset = (((y >> 2) * (pitch>>2)) + (x >> 2));
//                        *(pixels + textureOffset) = image[frameOffset];
//                    }
//                }
//            }
//        }

    }
}