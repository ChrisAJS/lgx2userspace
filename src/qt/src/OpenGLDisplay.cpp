// Based on the work done by MasterAler (https://github.com/MasterAler/SampleYUVRenderer)

#include "OpenGLDisplay.h"

#include <QOpenGLShader>
#include <QOpenGLTexture>
#include <QCoreApplication>
#include <QResizeEvent>

#define ATTRIB_VERTEX 0
#define ATTRIB_TEXTURE 1


struct OpenGLDisplay::OpenGLDisplayImpl {
    OpenGLDisplayImpl()
            : mBufYuv(nullptr), mFrameSize(0) {}

    uint8_t *mBufYuv;
    int mFrameSize;

    QOpenGLShader *mVShader;
    QOpenGLShader *mFShader;
    QOpenGLShaderProgram *mShaderProgram;

    QOpenGLTexture *mTextureY;
    QOpenGLTexture *mTextureU;
    QOpenGLTexture *mTextureV;

    GLuint id_y, id_u, id_v;
    int textureUniformY, textureUniformU, textureUniformV;
    GLsizei mVideoW, mVideoH;

};

/*************************************************************************/

OpenGLDisplay::OpenGLDisplay(QWidget *parent)
        : QOpenGLWidget(parent), impl(new OpenGLDisplayImpl) {
    impl->mFrameSize = 1920 * 1080 * 2;
    impl->mVideoW = 1920;
    impl->mVideoH = 1080;
    impl->mBufYuv = new uint8_t[1920 * 1080 * 2];
}

OpenGLDisplay::~OpenGLDisplay() {
    delete[] reinterpret_cast<unsigned char *>(impl->mBufYuv);
}

void OpenGLDisplay::DisplayVideoFrame(uint32_t *data) {
    memcpy(impl->mBufYuv, data, impl->mFrameSize);
    update();
}

void OpenGLDisplay::initializeGL() {
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);

    compileVertexShader();
    compileFragmentShader();
    compileShaderProgram();

    // Read the position of the data variables tex_y, tex_u, tex_v in the shader, the declaration
    // of these variables can be seen in
    // fragment shader source
    impl->textureUniformY = impl->mShaderProgram->uniformLocation("tex_y");
    impl->textureUniformU = impl->mShaderProgram->uniformLocation("tex_u");
    impl->textureUniformV = impl->mShaderProgram->uniformLocation("tex_v");

    // Set the value of the vertex matrix of the attribute ATTRIB_VERTEX and format
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertexVertices);
    // Set the texture matrix value and format of the attribute ATTRIB_TEXTURE
    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
    // Enable the ATTRIB_VERTEX attribute data, the default is off
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    // Enable the ATTRIB_TEXTURE attribute data, the default is off
    glEnableVertexAttribArray(ATTRIB_TEXTURE);

    // Create y, u, v texture objects respectively
    impl->mTextureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    impl->mTextureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    impl->mTextureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    impl->mTextureY->create();
    impl->mTextureU->create();
    impl->mTextureV->create();

    // Get the texture index value of the return y component
    impl->id_y = impl->mTextureY->textureId();
    // Get the texture index value of the returned u component
    impl->id_u = impl->mTextureU->textureId();
    // Get the texture index value of the returned v component
    impl->id_v = impl->mTextureV->textureId();

    glClearColor(0.3, 0.3, 0.3, 0.0);
}

void OpenGLDisplay::compileShaderProgram() {
    this->impl->mShaderProgram = new QOpenGLShaderProgram(this);
    this->impl->mShaderProgram->addShader(this->impl->mFShader);
    this->impl->mShaderProgram->addShader(this->impl->mVShader);

    this->impl->mShaderProgram->bindAttributeLocation("vertexIn", ATTRIB_VERTEX);
    this->impl->mShaderProgram->bindAttributeLocation("textureIn", ATTRIB_TEXTURE);
    this->impl->mShaderProgram->link();
    this->impl->mShaderProgram->bind();
}

void OpenGLDisplay::compileFragmentShader() {
    impl->mFShader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    bool bCompile = this->impl->mFShader->compileSourceCode(
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
            "    rgb.r =  clamp(y + (1.370705 * (v - 0.5)), 0, 1);"
            "    rgb.g = clamp(y - (0.698001 * (v - 0.5)) - (0.337633 * (u - 0.5)), 0, 1);"
            "    rgb.b = clamp(y + (1.732446 * (u - 0.5)), 0, 1);"
            "    gl_FragColor = vec4(rgb, 1);"
            "}"
    );

    if (!bCompile) {
        throw OpenGlException();
    }
}

void OpenGLDisplay::compileVertexShader() {
    this->impl->mVShader = new QOpenGLShader(QOpenGLShader::Vertex, this);

    bool bCompile = this->impl->mVShader->compileSourceCode(
            "attribute vec4 vertexIn; "
            "attribute vec2 textureIn; "
            "varying vec2 textureOut;  "
            "void main(void)           "
            "{                         "
            "    gl_Position = vertexIn; "
            "    textureOut = textureIn; "
            "}"
    );
    if (!bCompile) {
        throw OpenGlException();
    }
}

void OpenGLDisplay::resizeGL(int w, int h) {
    if (h == 0)// prevents being divided by zero
        h = 1;// set the height to 1

    // Set the viewport
    glViewport(0, 0, w, h);
    update();
}

void OpenGLDisplay::paintGL() {
    uint8_t y[1920*1080];
    uint8_t u[1920*1080];
    uint8_t v[1920*1080];

    for(int i = 0; i < 1920*1080; i++) {
        y[i] = impl->mBufYuv[i*2];
    }

    for(int i = 0; i < 1920*1080; i+=2) {
        u[i] = impl->mBufYuv[i*2+1];
        u[i+1] = impl->mBufYuv[i*2+1];
    }

    for(int i = 1; i < 1920*1080; i+=2) {
        v[i] = impl->mBufYuv[i*2+1];
        v[i+1] = impl->mBufYuv[i*2+1];
    }

    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_2D, impl->id_y);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    // Use the memory mBufYuv data to create a real y data texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, impl->mVideoW, impl->mVideoH, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 y);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Load u data texture
    glActiveTexture(GL_TEXTURE1);//Activate texture unit GL_TEXTURE1
    glBindTexture(GL_TEXTURE_2D, impl->id_u);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, impl->mVideoW, impl->mVideoH, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, (char *) u);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Load v data texture
    glActiveTexture(GL_TEXTURE2);//Activate texture unit GL_TEXTURE2
    glBindTexture(GL_TEXTURE_2D, impl->id_v);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, impl->mVideoW, impl->mVideoH, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, (char *) v);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    glUniform1i(impl->textureUniformY, 0);
    glUniform1i(impl->textureUniformU, 1);
    glUniform1i(impl->textureUniformV, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void OpenGLDisplay::initialiseVideo() {

}

void OpenGLDisplay::videoFrameAvailable(uint32_t *image) {
    DisplayVideoFrame(image);
}

void OpenGLDisplay::display() {

}

void OpenGLDisplay::shutdownVideo() {

}
