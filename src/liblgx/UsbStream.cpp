#include "UsbStream.h"

#include <libusb-1.0/libusb.h>
#include <exception>
#include <system_error>
#include <sstream>
#include <algorithm>

#include "lgxdevice.h"

#include "commanddata_lgx2.h"
#include "commanddata_lgx.h"

static void usbTransferComplete(struct libusb_transfer *transfer) {
    auto *stream = static_cast<libusb::UsbStream *>(transfer->user_data);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        stream->onFrameData(transfer);
    }

    stream->submitTransfer(transfer);
}


namespace libusb {
    UsbStream::UsbStream(LGXDeviceType deviceType) : _deviceType{deviceType}, _shuttingDown{false} {
        libusb_init(nullptr);
        if (_deviceType == LGXDeviceType::LGX2) {
            _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x0551);
        } else {
            _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x4710);
        }

        if (_dev == nullptr) {
            throw std::runtime_error("Failed to open lgx/lgx2 - is it connected? Run lsusb to check and ensure you have installed the udev rules (and restarted udev if necessary!)");
        }

        _frameBuffer = new uint8_t[0x1FC000 * 8];

        if (libusb_set_configuration(_dev, 1) != LIBUSB_SUCCESS) {
            throw std::runtime_error("Failed to set configuration\n");
        }

        if (libusb_claim_interface(_dev, 0) != LIBUSB_SUCCESS) {
            throw std::runtime_error("Could not claim interface for lgx/lgx2 - is something else using the device?\n");
        }
    }

    void UsbStream::streamSetupCommands() {
        int actualLength;
        uint8_t transferBuffer[512]{0};
        std::string targetCommands;
        if (_deviceType == LGXDeviceType::LGX2) {
            targetCommands = lgx2_setup_commands;
        } else {
            targetCommands = lgx_setup_commands;
        }

        auto commands = std::istringstream{targetCommands};
        std::string command;
        while (commands >> command) {
            if (command[0] == '>') {
                unsigned long commandLength = (command.length() - 1) / 2;
                for (unsigned long i = 0; i < commandLength; i++) {
                    transferBuffer[i] = std::stoi(command.substr(1 + i * 2, 2), nullptr, 16);
                }
                libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, transferBuffer, static_cast<int>(commandLength),
                                     &actualLength, 0);
                if (actualLength != commandLength) {
                    throw std::runtime_error("Underrun when attempting to write command data for setup");
                }
            } else {
                int bytesToRead = std::stoi(command.substr(1));
                libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x01, transferBuffer, bytesToRead, &actualLength, 0);
                if (actualLength != bytesToRead) {
                    throw std::runtime_error("Underrun when attempting to read data during setup");
                }
            }
        }
    }

    void UsbStream::queueFrameRead(std::function<void(uint8_t *)> *onData) {
        _onFrameDataCallback = onData;

        for (int i = 0; i < 8; i++) {
            libusb_transfer *transfer = libusb_alloc_transfer(0);

            libusb_fill_bulk_transfer(transfer, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                             _frameBuffer + 0x1FC000 * i, 0x1FC000,
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