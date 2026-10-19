#ifndef LGX2USERSPACE_COMMANDBRIDGE_H
#define LGX2USERSPACE_COMMANDBRIDGE_H

#include <cstdint>
#include <mutex>
#include <string>
#include <libusb-1.0/libusb.h>

namespace libusb {
    // The device's USB-to-I2C bridge on bulk OUT 0x01 / IN 0x81 (see PROTOCOL.md, "Bootstrap
    // command stream"). Used for the bootstrap and, while streaming on IN 0x83, for runtime
    // register access. Each call holds a mutex, so it can be shared between threads.
    class CommandBridge {
    public:
        // Matches the Windows driver, whose synchronous URB helper (FUN_1400157c4) waits up to
        // 5 s per command transfer: replies can be slow while video is streaming. Note that a
        // command in flight at shutdown can delay exit by up to this long per transfer.
        static constexpr unsigned int COMMAND_TIMEOUT_MS = 5000;
        // wMaxPacketSize of the command endpoints (SuperSpeed bulk).
        static constexpr int COMMAND_MAX_PACKET = 1024;

        explicit CommandBridge(libusb_device_handle *dev);

        // Read `count` bytes from register `reg` of the 7-bit I2C slave `address`.
        int i2cRead(uint8_t address, uint8_t reg, uint8_t *out, uint8_t count);
        int i2cWrite(uint8_t address, uint8_t reg, uint8_t value);

        // GC550 only: read `count` bytes at `address` of the AT88SC CryptoMemory configuration
        // zone (Read Config Zone, B6), which needs no authentication and changes nothing.
        int cryptoReadConfig(uint8_t address, uint8_t *out, uint8_t count);

        // Discard any replies waiting in IN 0x81, so the next reply is not attributed to the
        // wrong request.
        void drainResponses();

        // Replay a recorded command stream: ">hex" tokens are written to OUT 0x01, "<N" tokens
        // read N bytes from IN 0x81. Throws on a short transfer.
        void replay(const std::string &commands);

        // Which half of the last failed command failed, for diagnostics.
        const char *lastFailure() const;

    private:
        libusb_device_handle *_dev;
        mutable std::mutex _mutex;
        const char *_lastFailure{""};

        void drainResponsesLocked();
    };
}

#endif //LGX2USERSPACE_COMMANDBRIDGE_H
