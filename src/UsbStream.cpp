#include "UsbStream.h"

#include <libusb-1.0/libusb.h>
#include <exception>
#include <system_error>
#include <sstream>

#include "lgxdevice.h"

#include "commanddata.h"

static int goodFrameCount = 0;

static void usbTransferComplete(struct libusb_transfer *transfer) {
    auto *stream = static_cast<libusb::UsbStream *>(transfer->user_data);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        if (*(uint32_t *) transfer->buffer == 0xC0FFFF00) {
            goodFrameCount++;

            printf("Good frames: %d\n", goodFrameCount);
        }
        stream->onFrameData(transfer);
    }

    libusb_submit_transfer(transfer);
}


namespace libusb {
    UsbStream::UsbStream() {
        libusb_init(nullptr);
        _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x0551);

        if (_dev == nullptr) {
            throw std::runtime_error("Failed to open lgx2 - is it connected? Run lsusb to check");
        }

        _frameBuffer = new uint8_t[0x1FC000 * 16];

        if (libusb_set_configuration(_dev, 1) != LIBUSB_SUCCESS) {
            throw std::runtime_error("Failed to set configuration\n");
        }

        if (libusb_claim_interface(_dev, 0) != LIBUSB_SUCCESS) {
            throw std::runtime_error("Could not claim interface for lgx2 - is something else using the device?\n");
        }
    }

    void UsbStream::streamSetupCommands() {
        int actualLength;
        uint8_t transferBuffer[512]{0};
        auto commands = std::istringstream{bin2c_commands_txt};
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
        uint8_t endpoints[] = {LIBUSB_ENDPOINT_IN | 0x03};
        int streams = libusb_alloc_streams(_dev, 4, endpoints, 1);

        for (int i = 0; i < 8; i++) {
            libusb_transfer *transfer = libusb_alloc_transfer(0);

            libusb_fill_bulk_stream_transfer(transfer, _dev, LIBUSB_ENDPOINT_IN | 0x03, streams + i,
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
        libusb_submit_transfer(transfer);

        (*_onFrameDataCallback)(transfer->buffer);
    }
}