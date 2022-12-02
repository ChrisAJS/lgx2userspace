#include "UsbStream.h"

#include <libusb-1.0/libusb.h>
#include <exception>
#include <system_error>
#include <sstream>
#include <algorithm>

#include "lgxdevice.h"

#include "bootstrap/commanddata_lgx2.h"
#include "bootstrap/commanddata_lgx.h"

static void usbTransferComplete(struct libusb_transfer *transfer) {
    auto *stream = static_cast<libusb::UsbStream *>(transfer->user_data);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        stream->onFrameData(transfer);
    }

    stream->submitTransfer(transfer);
}


namespace libusb {
    static const int LGX_DATA_FRAME_LEN = 0x1FC000;

    UsbStream::UsbStream() : _dev{nullptr}, _onFrameDataCallback{}, _shuttingDown{false} {
        libusb_init(nullptr);

        libusb_device **list = nullptr;
        ssize_t count = libusb_get_device_list(nullptr, &list);

        for (ssize_t idx = 0; idx < count; ++idx) {
            libusb_device *device = list[idx];
            libusb_device_descriptor desc{};

            libusb_get_device_descriptor(device, &desc);
            if (desc.idVendor == 0x07ca) {
                if (desc.idProduct == 0x0551) {
                    printf("LGX2 (GC551) detected\n");
                    _availableDevices.push_back(lgx2::DeviceType::LGX2);
                }
#ifdef GC550_SUPPORT
                else if (desc.idProduct == 0x4710) {
                    printf("LGX (GC550) detected\n");
                    _availableDevices.push_back(lgx2::DeviceType::LGX);
                }
#endif
            }
        }

        libusb_free_device_list(list, (int) count);
        _frameBuffer = new uint8_t[LGX_DATA_FRAME_LEN * 8];
    }

    UsbStream::~UsbStream() {
        libusb_exit(nullptr);
    }

    bool UsbStream::deviceAvailable(lgx2::DeviceType deviceType) {
        return std::find(_availableDevices.begin(), _availableDevices.end(), deviceType) != _availableDevices.end();
    }

    void UsbStream::streamSetupCommands(lgx2::DeviceType deviceType) {
        std::string targetCommands;
        if (deviceType == lgx2::DeviceType::LGX2) {
            _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x0551);
            targetCommands = lgx2_setup_commands;
        } else {
#ifdef GC550_SUPPORT
            _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x4710);
            targetCommands = lgx_setup_commands;
#else
            throw std::runtime_error("Dangerous LGX (GC550) support has not been compiled. Check the README.md");
#endif
        }

        if (_dev == nullptr) {
            throw std::runtime_error(
                    "Failed to open device - is it connected? Run lsusb to check and ensure you have installed the udev rules (and restarted udev if necessary!)");
        }

        if (libusb_set_configuration(_dev, 1) != LIBUSB_SUCCESS) {
            throw std::runtime_error("Failed to set configuration\n");
        }

        if (libusb_claim_interface(_dev, 0) != LIBUSB_SUCCESS) {
            throw std::runtime_error(
                    "Could not claim interface for the device - is something else using the device?\n");
        }

        int check = 0;
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x03, _frameBuffer, LGX_DATA_FRAME_LEN, &check, 100);
        if (res != LIBUSB_ERROR_TIMEOUT) {
            printf("Skipping bootstrap - device is already producing frame data\n");
            return;
        }

        int actualLength;
        uint8_t transferBuffer[512]{0};


        auto commands = std::istringstream{targetCommands};
        std::string command;
        while (commands >> command) {
            if (command[0] == '>') {
                int commandLength = (int) (command.length() - 1) / 2;
                for (int i = 0; i < commandLength; i++) {
                    transferBuffer[i] = std::stoi(command.substr(1 + i * 2, 2), nullptr, 16);
                }
                libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, transferBuffer, static_cast<int>(commandLength),
                                     &actualLength, 0);
                if (actualLength != commandLength) {
                    throw std::runtime_error("Under-run when attempting to write command data for setup");
                }
            } else {
                int bytesToRead = std::stoi(command.substr(1));
                libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x01, transferBuffer, bytesToRead, &actualLength, 0);
                if (actualLength != bytesToRead) {
                    throw std::runtime_error("Under-run when attempting to read data during setup");
                }
            }
        }
    }

    void UsbStream::queueFrameRead(std::function<void(uint8_t *)> *onData) {
        _onFrameDataCallback = onData;

        for (int i = 0; i < 8; i++) {
            libusb_transfer *transfer = libusb_alloc_transfer(0);

            libusb_fill_bulk_transfer(transfer, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                      _frameBuffer + LGX_DATA_FRAME_LEN * i, LGX_DATA_FRAME_LEN,
                                      usbTransferComplete, this, 0);

            _transfers.push_back(transfer);
        }

        for (int i = 0; i < 8; i++) {
            libusb_submit_transfer(_transfers[i]);
        }
    }

    void UsbStream::update() {
        libusb_handle_events(nullptr);
    }

    void UsbStream::onFrameData(libusb_transfer *transfer) {
        if (!_shuttingDown) {
            (*_onFrameDataCallback)(transfer->buffer);
        }
    }

    void UsbStream::shutdownStream() {

        _shuttingDown = true;

        while (!_transfers.empty()) {
            libusb_handle_events(nullptr);
        }

        delete[] _frameBuffer;

        libusb_release_interface(_dev, 0);
        libusb_close(_dev);
    }

    void UsbStream::submitTransfer(libusb_transfer *transfer) {
        if (!_shuttingDown) {
            libusb_submit_transfer(transfer);
        } else {
            libusb_cancel_transfer(transfer);
            libusb_free_transfer(transfer);
            _transfers.erase(std::find(_transfers.begin(), _transfers.end(), transfer));
        }
    }
}