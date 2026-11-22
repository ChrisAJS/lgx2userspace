#include "CommandBridge.h"
#include "CommandGuard.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace libusb {

    CommandBridge::CommandBridge(libusb_device_handle *dev, lgx2::DeviceType type) : _dev{dev}, _type{type} {
    }

    int CommandBridge::sendCommand(const uint8_t *command, int length, int *actualLength, unsigned int timeoutMs) {
        *actualLength = 0;
        std::string reason = commandGuardReason(_type, command, length > 0 ? static_cast<size_t>(length) : 0);
        if (!reason.empty()) {
            fprintf(stderr, "Command guard refused to send %s\n", reason.c_str());
            _lastFailure = "refused by the command guard";
            return LIBUSB_ERROR_NOT_SUPPORTED;
        }
        // libusb takes a non-const buffer even for OUT transfers; it does not modify it.
        return libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, const_cast<uint8_t *>(command), length,
                                    actualLength, timeoutMs);
    }

    int CommandBridge::i2cRead(uint8_t address, uint8_t reg, uint8_t *out, uint8_t count) {
        std::lock_guard<std::mutex> lock(_mutex);

        // Bridge read request: 02 <addr> <reg addr length=1> <count> <reg>, answered by
        // exactly <count> bytes on bulk IN 0x01 (see PROTOCOL.md, "Bootstrap command stream").
        uint8_t request[] = {0x02, address, 0x01, count, reg};
        int actualLength = 0;

        _lastFailure = "request (OUT 0x01)";
        int res = sendCommand(request, sizeof(request), &actualLength, COMMAND_TIMEOUT_MS);
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
        int res = sendCommand(request, sizeof(request), &actualLength, COMMAND_TIMEOUT_MS);
        if (res != LIBUSB_SUCCESS) return res;
        return actualLength == static_cast<int>(sizeof(request)) ? LIBUSB_SUCCESS : LIBUSB_ERROR_IO;
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

        // Refuse the whole stream before sending anything if any command in it is not allowed,
        // so a bad recording can never leave the board half-configured.
        checkReplayCommands(_type, commands);

        int actualLength = 0;
        uint8_t transferBuffer[512]{0};

        auto stream = std::istringstream{commands};
        std::string command;
        while (stream >> command) {
            if (command[0] == '>') {
                int commandLength = (int) (command.length() - 1) / 2;
                for (int i = 0; i < commandLength; i++) {
                    transferBuffer[i] = std::stoi(command.substr(1 + i * 2, 2), nullptr, 16);
                }
                if (commandLength > static_cast<int>(sizeof(transferBuffer))) {
                    throw std::runtime_error("Setup command too long");
                }
                sendCommand(transferBuffer, commandLength, &actualLength, 0);
                if (actualLength != commandLength) {
                    throw std::runtime_error("Under-run when attempting to write command data for setup");
                }
            } else {
                int bytesToRead = std::stoi(command.substr(1));
                if (bytesToRead <= 0 || bytesToRead > static_cast<int>(sizeof(transferBuffer))) {
                    throw std::runtime_error("Setup read too long");
                }
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
