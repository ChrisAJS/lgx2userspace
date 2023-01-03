// Based on the work done by MasterAler (https://github.com/MasterAler/SampleYUVRenderer)

#ifndef LGX2USERSPACE_OPENGLDISPLAY_H
#define LGX2USERSPACE_OPENGLDISPLAY_H

#ifndef OPENGLDISPLAY_H
#define OPENGLDISPLAY_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QScopedPointer>
#include <QException>

#include <liblgx.h>

class OpenGLDisplay : public QOpenGLWidget, public QOpenGLFunctions, public lgx2::VideoOutput
{
public:
    explicit OpenGLDisplay(QWidget* parent = nullptr);
    ~OpenGLDisplay();

    void DisplayVideoFrame(uint32_t *data);

    void initialiseVideo() override;

    void videoFrameAvailable(uint32_t *image) override;

    void display() override;

    void shutdownVideo() override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    struct OpenGLDisplayImpl;
    QScopedPointer<OpenGLDisplayImpl> impl;

    void compileVertexShader();
    void compileFragmentShader();
    void compileShaderProgram();
};

/***********************************************************************/

class OpenGlException: public QException
{
public:
    void raise() const { throw *this; }
    OpenGlException *clone() const { return new OpenGlException(*this); }
};

#endif // OPENGLDISPLAY_H

#endif //LGX2USERSPACE_OPENGLDISPLAY_H
