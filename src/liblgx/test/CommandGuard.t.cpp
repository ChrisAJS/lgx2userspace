#include "catch_amalgamated.hpp"

#include "usb/CommandGuard.h"
#include "bootstrap/commanddata_lgx2.h"
#ifdef GC550_SUPPORT
#   include "bootstrap/commanddata_lgx.h"
#endif

#include <string>
#include <vector>

using lgx2::DeviceType;
using libusb::checkReplayCommands;
using libusb::commandGuardReason;
using libusb::CommandRejected;

namespace {
    bool allowed(DeviceType type, std::vector<uint8_t> command) {
        return commandGuardReason(type, command.data(), command.size()).empty();
    }
}

TEST_CASE("The recorded GC551 bootstrap passes the guard") {
    REQUIRE_NOTHROW(checkReplayCommands(DeviceType::LGX2, lgx2_setup_commands));
}

TEST_CASE("Empty and unknown commands are refused") {
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {}));
    REQUIRE_FALSE(commandGuardReason(DeviceType::LGX2, nullptr, 0).empty());
    for (int op = 0; op < 256; op++) {
        INFO("opcode " << op);
        REQUIRE(allowed(DeviceType::LGX2, {static_cast<uint8_t>(op)}) == (op == 0x14));
    }
    // Config-memory write and any other 0E read.
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {0x0D, 0x02, 0x07, 0xFE, 0x01, 0x00, 0xAA}));
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {0x15, 0x02, 0x07, 0xFE, 0xAA}));
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {0x0E, 0x02, 0x07, 0x00, 0x02, 0x00}));
    REQUIRE(allowed(DeviceType::LGX2, {0x0E, 0x02, 0x07, 0xFE, 0x02, 0x00}));
}

TEST_CASE("Malformed I2C commands are refused") {
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {0x01, 0x20, 0x01, 0x02}));              // no data
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {0x01, 0x20, 0x02, 0x00, 0x02, 0x00}));  // 2-byte register
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {0x02, 0x20, 0x01, 0x00, 0x02}));        // zero-length read
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {0x02, 0x20, 0x01, 0x21, 0x02}));        // too long
    REQUIRE_FALSE(allowed(DeviceType::LGX2, {0x02, 0x20, 0x01, 0x01, 0x02, 0x00}));  // trailing byte
    std::vector<uint8_t> tooLong{0x01, 0x36, 0x01, 0x00};
    tooLong.resize(4 + 33, 0x00);
    REQUIRE_FALSE(allowed(DeviceType::LGX2, tooLong));
}

TEST_CASE("A replay containing a refused command throws before anything is sent") {
    REQUIRE_THROWS_AS(checkReplayCommands(DeviceType::LGX2, lgx2_setup_commands + " >05B6005001 >0601 <1"),
                      CommandRejected);
    REQUIRE_THROWS_AS(checkReplayCommands(DeviceType::LGX2, ">01200102F0 >0G"), CommandRejected);
    REQUIRE_THROWS_AS(checkReplayCommands(DeviceType::LGX2, ">012"), CommandRejected);
    REQUIRE_THROWS_AS(checkReplayCommands(DeviceType::LGX2, "01200102F0"), CommandRejected);
}

#ifdef GC550_SUPPORT
TEST_CASE("The recorded GC550 bootstrap passes the guard") {
    REQUIRE_NOTHROW(checkReplayCommands(DeviceType::LGX, lgx_setup_commands));
}

TEST_CASE("No AT88SC CryptoMemory command reaches a GC550") {
    // Taken from the GC550 recording before the CryptoMemory section was removed (f150158^).
    const std::vector<std::vector<uint8_t>> crypto{
            {0x05, 0xB8, 0x00, 0x00, 0x0A},                                  // security reset
            {0x07, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0x05, 0xB6, 0x00, 0x00, 0x08}, {0x06, 0x08},                    // read config zone
            {0x05, 0xB6, 0x00, 0x50, 0x01}, {0x06, 0x01},                    // attempt counter read
            {0x05, 0xB8, 0x01, 0x00, 0x10},                                  // verify crypto
            {0x05, 0xBA, 0x02, 0x00, 0x03},                                  // verify password
            {0x05, 0xB4, 0x03, 0x00, 0x00},                                  // select user zone
            {0x09, 0x0A},
    };
    for (const auto &command : crypto) {
        INFO("command starting " << int(command[0]));
        REQUIRE_FALSE(allowed(DeviceType::LGX, command));
    }
    for (int op = 0; op < 256; op++) {
        INFO("opcode " << op);
        REQUIRE(allowed(DeviceType::LGX, {static_cast<uint8_t>(op)}) == (op == 0x08));
    }
    REQUIRE_THROWS_AS(checkReplayCommands(DeviceType::LGX, ">05B6005001 >0601 <1 " + lgx_setup_commands),
                      CommandRejected);
}

TEST_CASE("A GC550 never addresses 0x50-0x5F over I2C") {
    // EEPROMs, and the AT88SC command bytes B0-BA (e.g. B8 = verify crypto) read as 7-bit addresses.
    for (int address = 0x50; address <= 0x5F; address++) {
        INFO("address " << address);
        REQUIRE_FALSE(allowed(DeviceType::LGX, {0x01, static_cast<uint8_t>(address), 0x01, 0x00, 0x00}));
        REQUIRE_FALSE(allowed(DeviceType::LGX, {0x02, static_cast<uint8_t>(address), 0x01, 0x01, 0x00}));
    }
}

TEST_CASE("GC550 runtime register access stays allowed") {
    REQUIRE(allowed(DeviceType::LGX, {0x02, 0x20, 0x01, 0x01, 0xFA}));  // board-state detection
    REQUIRE(allowed(DeviceType::LGX, {0x02, 0x74, 0x01, 0x02, 0x06}));
    REQUIRE(allowed(DeviceType::LGX, {0x01, 0x20, 0x01, 0x02, 0xF0}));  // auto input colour space
    REQUIRE(allowed(DeviceType::LGX, {0x01, 0x22, 0x01, 0x3C, 0x00}));  // picture adjustment
    REQUIRE(allowed(DeviceType::LGX, {0x02, 0x46, 0x01, 0x01, 0x01}));  // InfoFrame status poll
    REQUIRE(allowed(DeviceType::LGX, {0x02, 0x34, 0x01, 0x0D, 0x2A}));
    REQUIRE_FALSE(allowed(DeviceType::LGX, {0x01, 0x46, 0x01, 0x01, 0x00}));  // never written
    REQUIRE_FALSE(allowed(DeviceType::LGX, {0x14}));
}
#endif
