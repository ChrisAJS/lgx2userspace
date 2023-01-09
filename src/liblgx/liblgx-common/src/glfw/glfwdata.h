#ifndef LGX2USERSPACE_GLFWDATA_H
#define LGX2USERSPACE_GLFWDATA_H

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

static const char *vertexShaderSource =
        "#version 120\n"
        "attribute vec4 vertexIn; "
        "attribute vec2 textureIn; "
        "varying vec2 textureOut;  "
        "void main(void)           "
        "{                         "
        "    gl_Position = vertexIn; "
        "    textureOut = textureIn; "
        "}";

static const char *fragmentShaderSource =
        "#version 120\n"
        "varying vec2 textureOut;"
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
        "    rgb.r =  clamp(y + (1.370705 * (v - 0.5)), 0.0, 1.0);"
        "    rgb.g = clamp(y - (0.698001 * (v - 0.5)) - (0.337633 * (u - 0.5)), 0.0, 1.0);"
        "    rgb.b = clamp(y + (1.732446 * (u - 0.5)), 0.0, 1.0);"
        "    gl_FragColor = vec4(rgb, 1);"
        "}";


#endif //LGX2USERSPACE_GLFWDATA_H
