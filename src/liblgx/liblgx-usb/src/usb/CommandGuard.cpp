#include "CommandGuard.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <vector>

namespace libusb {

    namespace {
        // Largest I2C payload / read in either recording (an EDID block).
        constexpr size_t MAX_I2C_DATA = 32;
        constexpr uint8_t BOARD_INFO_READ[] = {0x0E, 0x02, 0x07, 0xFE, 0x02, 0x00};

        struct AddressSet {
            std::vector<uint8_t> writable;
            std::vector<uint8_t> readable;
            uint8_t singleByteCommand;
        };

        // Exactly the 7-bit I2C addresses each recorded Windows bootstrap writes / reads
        // (utils/decode_bootstrap.py); the runtime fixes and status poll stay within them.
        const AddressSet &addressesFor(lgx2::DeviceType type) {
#ifdef GC550_SUPPORT
            static const AddressSet gc550{
                    // FPGA, AIC3x codec, ADV7604 IO/CP/AFE/REP/HDMI/EDID/ESDP, ADV7511, 0x4A mixer,
                    // 0x65, PCA9539A
                    {0x10, 0x18, 0x20, 0x22, 0x26, 0x32, 0x34, 0x36, 0x38, 0x3D, 0x4A, 0x65, 0x74},
                    // as above without the write-only maps, plus 0x3F and the InfoFrame map 0x46
                    {0x10, 0x20, 0x22, 0x26, 0x32, 0x34, 0x3D, 0x3F, 0x46, 0x4A, 0x74},
                    0x08};
            if (type == lgx2::DeviceType::LGX) return gc550;
#endif
            static const AddressSet gc551{
                    {0x10, 0x20, 0x22, 0x26, 0x2C, 0x32, 0x34, 0x36, 0x3E, 0x44, 0x45, 0x46, 0x47, 0x4B, 0x58,
                     0x65, 0x6C, 0x74},
                    {0x10, 0x20, 0x22, 0x26, 0x2C, 0x32, 0x34, 0x3E, 0x44, 0x45, 0x46, 0x47, 0x4B, 0x58, 0x74},
                    0x14};
            (void) type;
            return gc551;
        }

        bool contains(const std::vector<uint8_t> &set, uint8_t value) {
            return std::find(set.begin(), set.end(), value) != set.end();
        }

        std::string hex(const uint8_t *bytes, size_t length) {
            std::string out;
            char byte[4];
            for (size_t i = 0; i < length && i < 8; i++) {
                snprintf(byte, sizeof(byte), "%s%02X", i ? " " : "", bytes[i]);
                out += byte;
            }
            if (length > 8) out += " ...";
            return out;
        }
    }

    std::string commandGuardReason(lgx2::DeviceType type, const uint8_t *command, size_t length) {
        if (command == nullptr || length == 0) return "empty command";
        const AddressSet &addresses = addressesFor(type);
        const std::string what = "command [" + hex(command, length) + "]: ";

        switch (command[0]) {
            case 0x01:  // 01 <addr> 01 <reg> <data...>
                if (length < 5 || command[2] != 0x01 || length - 4 > MAX_I2C_DATA)
                    return what + "malformed I2C write";
                if (!contains(addresses.writable, command[1]))
                    return what + "I2C write to an address this card's bootstrap never writes";
                return {};
            case 0x02:  // 02 <addr> 01 <count> <reg>
                if (length != 5 || command[2] != 0x01 || command[3] == 0 || command[3] > MAX_I2C_DATA)
                    return what + "malformed I2C read";
                if (!contains(addresses.readable, command[1]))
                    return what + "I2C read from an address this card's bootstrap never reads";
                return {};
            case 0x0E:
                if (length == sizeof(BOARD_INFO_READ) && std::equal(command, command + length, BOARD_INFO_READ))
                    return {};
                return what + "memory read other than the recorded board-info read";
            default:
                if (length == 1 && command[0] == addresses.singleByteCommand) return {};
                return what + "opcode not allowed (CryptoMemory, config-memory write or unknown)";
        }
    }

    void checkReplayCommands(lgx2::DeviceType type, const std::string &commands) {
        auto stream = std::istringstream{commands};
        std::string token;
        size_t index = 0;
        std::vector<uint8_t> bytes;
        while (stream >> token) {
            if (token[0] == '<') continue;
            if (token[0] != '>' || token.size() < 3 || token.size() % 2 == 0)
                throw CommandRejected("Replay token " + std::to_string(index) + " is malformed: " + token);
            bytes.clear();
            for (size_t i = 1; i < token.size(); i += 2) {
                if (!isxdigit(static_cast<unsigned char>(token[i])) ||
                    !isxdigit(static_cast<unsigned char>(token[i + 1])))
                    throw CommandRejected("Replay token " + std::to_string(index) + " is malformed: " + token);
                bytes.push_back(static_cast<uint8_t>(std::stoi(token.substr(i, 2), nullptr, 16)));
            }
            std::string reason = commandGuardReason(type, bytes.data(), bytes.size());
            if (!reason.empty())
                throw CommandRejected("Replay command " + std::to_string(index) + " refused: " + reason);
            index++;
        }
    }
}
