#ifndef LGX2USERSPACE_LGXWIDGET_H
#define LGX2USERSPACE_LGXWIDGET_H

#include <QWidget>
#include <liblgx.h>

class LgxWidget : public QWidget, public lgx2::VideoOutput {
    Q_OBJECT
public:

protected:
    void paintEvent(QPaintEvent *event) override;

public:

    void initialiseVideo() override;

    void videoFrameAvailable(uint32_t *image) override;

    void display() override;

    void shutdownVideo() override;

private:
    QImage _image{1920, 1080, QImage::Format::Format_RGB888};
};


#endif //LGX2USERSPACE_LGXWIDGET_H
