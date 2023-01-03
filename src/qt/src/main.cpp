#include <QApplication>
#include <liblgx.h>
#include "OpenGLDisplay.h"
#include "LgxDeviceThread.h"

int main(int argc, char *argv[]) {

    QApplication a(argc, argv);

    OpenGLDisplay widget;

    lgx2::Device device{new libusb::UsbStream(), &widget, new NullAudioOutput(), new NOOPLogger()};

    widget.resize(1920, 1080);
    widget.show();

    LgxDeviceThread deviceThread{&device};

    deviceThread.start();

    return QApplication::exec();
}