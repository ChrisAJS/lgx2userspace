#ifndef LGX2USERSPACE_COMMANDGUARD_H
#define LGX2USERSPACE_COMMANDGUARD_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "lgxinterfaces.h"

namespace libusb {
    // Allow-list for everything sent to the board's command endpoint (bulk OUT 0x01).
    //
    // The GC550 carries an Atmel AT88SC CryptoMemory that the official Windows driver checks. A
    // few failed authentication or password attempts lock it permanently, after which the card no
    // longer works on Windows. The board also has config memory written by bridge opcode 0D/15.
    // So only commands the recorded Windows bootstrap itself sends get through:
    //   01  I2C write, 1-byte register address, to a chip the recorded bootstrap writes
    //   02  I2C read, 1-byte register address, from a chip the recorded bootstrap reads
    //   0E  only the exact board-info read 0E 02 07 FE 02 00
    //   08  (GC550) / 14 (GC551): the single-byte commands in each recording
    // Everything else, including the CryptoMemory transport (05/06/07/09), config-memory
    // writes (0D/15) and any other opcode, is refused before anything is sent. The I2C address
    // sets are per card, so the GC550 can never address 0x50-0x5F (EEPROMs, and the AT88SC
    // command bytes B0-BA read as 7-bit addresses).
    class CommandRejected : public std::runtime_error {
    public:
        explicit CommandRejected(const std::string &what) : std::runtime_error(what) {}
    };

    // Empty when `command` may be sent to a board of `type`, otherwise the reason it may not.
    std::string commandGuardReason(lgx2::DeviceType type, const uint8_t *command, size_t length);

    // Checks every ">hex" command of a recorded replay stream without sending anything; throws
    // CommandRejected (naming the command's index) on the first one that is not allowed, or on a
    // malformed token.
    void checkReplayCommands(lgx2::DeviceType type, const std::string &commands);
}

#endif //LGX2USERSPACE_COMMANDGUARD_H
