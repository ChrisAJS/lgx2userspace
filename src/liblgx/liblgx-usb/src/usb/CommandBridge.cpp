#include "CommandBridge.h"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace libusb {

    CommandBridge::CommandBridge(libusb_device_handle *dev) : _dev{dev} {
    }

    int CommandBridge::i2cRead(uint8_t address, uint8_t reg, uint8_t *out, uint8_t count) {
        std::lock_guard<std::mutex> lock(_mutex);

        // Bridge read request: 02 <addr> <reg addr length=1> <count> <reg>, answered by
        // exactly <count> bytes on bulk IN 0x01 (see PROTOCOL.md, "Bootstrap command stream").
        uint8_t request[] = {0x02, address, 0x01, count, reg};
        int actualLength = 0;

        _lastFailure = "request (OUT 0x01)";
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, request, sizeof(request),
                                       &actualLength, COMMAND_TIMEOUT_MS);
        if (res != LIBUSB_SUCCESS) return res;
        if (actualLength != static_cast<int>(sizeof(request))) return LIBUSB_ERROR_IO;

        // Receive into a full max-packet buffer: asking for exactly <count> bytes turns a stale or
        // unexpected reply into LIBUSB_ERROR_OVERFLOW, which can leave the endpoint unusable.
        _lastFailure = "reply (IN 0x81)";
        uint8_t reply[COMMAND_MAX_PACKET];
        res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x01, reply, sizeof(reply), &actualLength,
                                   COMMAND_TIMEOUT_MS);
        if (res == LIBUSB_ERROR_PIPE) {
            libusb_clear_halt(_dev, LIBUSB_ENDPOINT_IN | 0x01);
        }
        if (res != LIBUSB_SUCCESS) return res;
        if (actualLength != count) {
            _lastFailure = "reply (IN 0x81, unexpected length)";
            return LIBUSB_ERROR_IO;
        }
        memcpy(out, reply, count);
        return LIBUSB_SUCCESS;
    }

    int CommandBridge::i2cWrite(uint8_t address, uint8_t reg, uint8_t value) {
        std::lock_guard<std::mutex> lock(_mutex);

        // Bridge write request: 01 <addr> <reg addr length=1> <reg> <data>; no reply.
        uint8_t request[] = {0x01, address, 0x01, reg, value};
        int actualLength = 0;
        _lastFailure = "request (OUT 0x01)";
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, request, sizeof(request),
                                       &actualLength, COMMAND_TIMEOUT_MS);
        if (res != LIBUSB_SUCCESS) return res;
        return actualLength == static_cast<int>(sizeof(request)) ? LIBUSB_SUCCESS : LIBUSB_ERROR_IO;
    }

    int CommandBridge::cryptoReadConfig(uint8_t address, uint8_t *out, uint8_t count) {
        std::lock_guard<std::mutex> lock(_mutex);

        // As recorded from the Windows driver (see PROTOCOL.md, "GC550 CryptoMemory"): 05 sends
        // the 4-byte CryptoMemory command <cmd> <p1> <p2> <N> without a reply, then 06 <N> clocks
        // in N bytes, answered on IN 0x81.
        uint8_t command[] = {0x05, 0xB6, 0x00, address, count};
        uint8_t readRequest[] = {0x06, count};
        int actualLength = 0;

        _lastFailure = "command (OUT 0x01)";
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, command, sizeof(command), &actualLength,
                                       COMMAND_TIMEOUT_MS);
        if (res != LIBUSB_SUCCESS) return res;
        if (actualLength != static_cast<int>(sizeof(command))) return LIBUSB_ERROR_IO;

        _lastFailure = "read request (OUT 0x01)";
        res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, readRequest, sizeof(readRequest), &actualLength,
                                   COMMAND_TIMEOUT_MS);
        if (res != LIBUSB_SUCCESS) return res;
        if (actualLength != static_cast<int>(sizeof(readRequest))) return LIBUSB_ERROR_IO;

        _lastFailure = "reply (IN 0x81)";
        uint8_t reply[COMMAND_MAX_PACKET];
        res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x01, reply, sizeof(reply), &actualLength,
                                   COMMAND_TIMEOUT_MS);
        if (res == LIBUSB_ERROR_PIPE) {
            libusb_clear_halt(_dev, LIBUSB_ENDPOINT_IN | 0x01);
        }
        if (res != LIBUSB_SUCCESS) return res;
        if (actualLength != count) {
            _lastFailure = "reply (IN 0x81, unexpected length)";
            return LIBUSB_ERROR_IO;
        }
        memcpy(out, reply, count);
        return LIBUSB_SUCCESS;
    }

    void CommandBridge::drainResponses() {
        std::lock_guard<std::mutex> lock(_mutex);
        drainResponsesLocked();
    }

    void CommandBridge::drainResponsesLocked() {
        uint8_t discard[COMMAND_MAX_PACKET];
        int actualLength = 0;
        while (libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x01, discard, sizeof(discard), &actualLength, 10) ==
               LIBUSB_SUCCESS) {
        }
    }

    void CommandBridge::replay(const std::string &commands) {
        std::lock_guard<std::mutex> lock(_mutex);

        int actualLength;
        uint8_t transferBuffer[512]{0};

        auto stream = std::istringstream{commands};
        std::string command;
        while (stream >> command) {
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

    const char *CommandBridge::lastFailure() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _lastFailure;
    }
}
