#ifndef LGX2USERSPACE_LGXDEVICETHREAD_H
#define LGX2USERSPACE_LGXDEVICETHREAD_H


#include <QThread>
#include <liblgx.h>

class LgxDeviceThread : public QThread {
Q_OBJECT

public:
    LgxDeviceThread(lgx2::Device *device);

protected:
    void run() override;

private:
    lgx2::Device *_device{nullptr};
};


#endif //LGX2USERSPACE_LGXDEVICETHREAD_H
