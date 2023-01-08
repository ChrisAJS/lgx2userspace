// Based on the work done by MasterAler (https://github.com/MasterAler/SampleYUVRenderer). Original shader code and OGL code lovingly adapted.

#include "GlfwVideoOutput.h"

#include <stdexcept>
#include <GLFW/glfw3.h>

static const GLfloat vertexVertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, 1.0f,
};

static const GLfloat textureVertices[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
};

static const char* vertex_shader_text =
        "#version 330 core\n"
        "in vec4 vertexIn; "
        "in vec2 textureIn; "
        "out vec2 textureOut;  "
        "void main(void)           "
        "{                         "
        "    gl_Position = vertexIn; "
        "    textureOut = textureIn; "
        "}";

static const char* fragment_shader_text =
        "#version 330 core\n"
        "layout(location = 0) out vec4 color;"
        "in vec2 textureOut;"
        "uniform sampler2D tex_y;"
        "uniform sampler2D tex_u;"
        "uniform sampler2D tex_v;"
        "void main(void)"
        "{"
        "    vec3 yuv;"
        "    vec3 rgb;"
        "    float y = texture2D(tex_y, textureOut).r;"
        "    float u = texture2D(tex_u, textureOut).r;"
        "    float v = texture2D(tex_v, textureOut).r;"
        ""
        "    rgb.r =  clamp(y + (1.370705 * (v - 0.5)), 0, 1);"
        "    rgb.g = clamp(y - (0.698001 * (v - 0.5)) - (0.337633 * (u - 0.5)), 0, 1);"
        "    rgb.b = clamp(y + (1.732446 * (u - 0.5)), 0, 1);"
        "    color = vec4(rgb, 1);"
        "}";

namespace glfw {
    GlfwVideoOutput::GlfwVideoOutput() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialise GLFW3");
        }

        yuvImage = new uint8_t[1920*1080*3];
    }

    void GlfwVideoOutput::initialiseVideo() {
        window = glfwCreateWindow(1920, 1080, "lgx2userspace", nullptr, nullptr);
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

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertex_shader_text, nullptr);
        glCompileShader(vertexShader);
        int vertexMaxLength = 4096;
        GLchar vertexErrorLog[4096]{0};
        glGetShaderInfoLog(vertexShader, vertexMaxLength, &vertexMaxLength, &vertexErrorLog[0]);
        if (vertexMaxLength > 0) {
            fprintf(stderr, "Vertex shader error: %s\n", vertexErrorLog);
            throw std::runtime_error("Failed to compile vertex shader.");
        }

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragment_shader_text, nullptr);
        glCompileShader(fragmentShader);
        int maxLength = 4096;
        GLchar errorLog[4096]{0};
        glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &errorLog[0]);
        if (maxLength > 0) {
            fprintf(stderr, "Fragment shader error: %s\n", errorLog);
            throw std::runtime_error("Failed to compile fragment shader.");
        }

        GLuint shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, fragmentShader);
        glAttachShader(shaderProgram, vertexShader);
        glBindAttribLocation(shaderProgram, 0, "vertexIn");
        glBindAttribLocation(shaderProgram, 1, "textureIn");
        glLinkProgram(shaderProgram);
        glUseProgram(shaderProgram);

        textureUniformY = glGetUniformLocation(shaderProgram, "tex_y");
        textureUniformU = glGetUniformLocation(shaderProgram, "tex_u");
        textureUniformV = glGetUniformLocation(shaderProgram, "tex_v");

        glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, vertexVertices);
        glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, textureVertices);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glGenTextures(3, textures);
    }

    void GlfwVideoOutput::videoFrameAvailable(uint32_t *image) {
        uint8_t *rawImage = reinterpret_cast<uint8_t *>(image);

        for(int i = 0; i < 1920*1080; i++) {
            yuvImage[i] = rawImage[i*2];
        }

        for(int i = 0; i < 1920*1080; i+=2) {
            yuvImage[1920*1080 + i] = rawImage[i*2+1];
            yuvImage[1920*1080 + i+1] = rawImage[i*2+1];
        }

        for(int i = 1; i < 1920*1080; i+=2) {
            yuvImage[1920*1080*2 + i] = rawImage[i*2+1];
            yuvImage[1920*1080*2 + i+1] = rawImage[i*2+1];
        }

        glActiveTexture(GL_TEXTURE0);

        glBindTexture(GL_TEXTURE_2D, textures[0]);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvImage);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Load u data texture
        glActiveTexture(GL_TEXTURE1);//Activate texture unit GL_TEXTURE1
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, (char *) yuvImage + 1920*1080);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Load v data texture
        glActiveTexture(GL_TEXTURE2);//Activate texture unit GL_TEXTURE2
        glBindTexture(GL_TEXTURE_2D, textures[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, (char *) yuvImage + 1920*1080*2);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glUniform1i(textureUniformY, 0);
        glUniform1i(textureUniformU, 1);
        glUniform1i(textureUniformV, 2);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void GlfwVideoOutput::display() {
        glfwPollEvents();

        glfwSwapBuffers(window);

        if (glfwWindowShouldClose(window)) {
            throw std::runtime_error("Window closed");
        }
    }

    void GlfwVideoOutput::shutdownVideo() {

    }
}