#include "GlfwFrameOutput.h"

#include <stdexcept>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <vector>

//Vertex matrix
static const GLfloat vertexVertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, 1.0f,
};

//Texture matrix
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
    GlfwFrameOutput::GlfwFrameOutput() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialise GLFW3");
        }
    }

    void GlfwFrameOutput::initialiseVideo() {
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
        printf("ERRORLOG: %s\n", vertexErrorLog);


        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragment_shader_text, nullptr);
        glCompileShader(fragmentShader);
        int maxLength = 4096;
        GLchar errorLog[4096]{0};
        glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &errorLog[0]);
        printf("ERRORLOG: %s\n", errorLog);


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

    void GlfwFrameOutput::videoFrameAvailable(uint32_t *image) {
        uint8_t *rawImage = reinterpret_cast<uint8_t *>(image);
        uint8_t y[1920*1080];
        uint8_t u[1920*1080];
        uint8_t v[1920*1080];

        for(int i = 0; i < 1920*1080; i++) {
            y[i] = rawImage[i*2];
        }

        for(int i = 0; i < 1920*1080; i+=2) {
            u[i] = rawImage[i*2+1];
            u[i+1] = rawImage[i*2+1];
        }

        for(int i = 1; i < 1920*1080; i+=2) {
            v[i] = rawImage[i*2+1];
            v[i+1] = rawImage[i*2+1];
        }

        glActiveTexture(GL_TEXTURE0);

        glBindTexture(GL_TEXTURE_2D, textures[0]);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,y);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Load u data texture
        glActiveTexture(GL_TEXTURE1);//Activate texture unit GL_TEXTURE1
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, (char *) u);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Load v data texture
        glActiveTexture(GL_TEXTURE2);//Activate texture unit GL_TEXTURE2
        glBindTexture(GL_TEXTURE_2D, textures[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1920, 1080, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, (char *) v);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glUniform1i(textureUniformY, 0);
        glUniform1i(textureUniformU, 1);
        glUniform1i(textureUniformV, 2);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void GlfwFrameOutput::display() {
        glfwPollEvents();

        glfwSwapBuffers(window);

        if (glfwWindowShouldClose(window)) {
            throw std::runtime_error("Window closed");
        }
    }

    void GlfwFrameOutput::shutdownVideo() {

    }

    void GlfwFrameOutput::initialiseAudio() {

    }

    void GlfwFrameOutput::audioFrameAvailable(uint32_t *audio) {

    }

    void GlfwFrameOutput::render() {

    }

    void GlfwFrameOutput::shutdownAudio() {

    }
}