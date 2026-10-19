#include "Bootstrapper.h"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include "CaptureStream.h"
#include "bootstrap/commanddata_lgx2.h"
#ifdef GC550_SUPPORT
#   include "bootstrap/commanddata_lgx.h"
#endif

namespace libusb {

    Bootstrapper::Bootstrapper(libusb_device_handle *dev, CommandBridge &bridge) : _dev{dev}, _bridge{bridge} {
    }

    bool Bootstrapper::bootstrap(lgx2::DeviceType deviceType) {
        std::string targetCommands;
        if (deviceType == lgx2::DeviceType::LGX2) {
            targetCommands = lgx2_setup_commands;
        } else {
#ifdef GC550_SUPPORT
            targetCommands = lgx_setup_commands;
#else
            throw std::runtime_error("Dangerous LGX (GC550) support has not been compiled. Check the README.md");
#endif
        }

        if (deviceType != lgx2::DeviceType::LGX2) {
            reportCryptoAttemptCounters();
        }

        bool alreadyBootstrapped = false;
        BoardState boardState = detectBoardState();
        if (boardState == BoardState::Configured) {
            // Re-running the bootstrap on a configured board is what produced garbage output, and
            // the configuration survives a USB reset, so keep it and just start streaming.
            printf("Device is already bootstrapped - reusing its existing configuration\n");
            alreadyBootstrapped = true;
        } else {
            if (boardState == BoardState::Unknown && videoAlreadyFlowing()) {
                // Board state could not be read: fall back to checking whether video is flowing.
                throw std::runtime_error("Device already been bootstrapped. This results in garbage output. Please unplug and re-plug in the device.");
            }
            printf("Bootstrapping the device\n");
            _bridge.replay(targetCommands);
            printf("Bootstrapping complete\n");

            // The LGX2 command capture was cut off mid-transaction: it ends with a read request
            // (>02220102A8) whose 2-byte reply is never read. Collect it now, otherwise it sits in
            // IN 0x81 and is returned as the answer to the next command read.
            _bridge.drainResponses();
        }

        // Both cards use an ADV7604 at the same I2C addresses, and both replays leave it set up
        // for the source that was connected when they were recorded.
        setAutoInputColourSpace();
        resetPictureAdjustment();
        return alreadyBootstrapped;
    }

    bool Bootstrapper::videoAlreadyFlowing() {
        std::vector<uint8_t> buffer(CaptureStream::CHUNK_BYTES);
        int check = 0;
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x03, buffer.data(),
                                       static_cast<int>(buffer.size()), &check, 100);
        return res != LIBUSB_ERROR_TIMEOUT;
    }

    Bootstrapper::BoardState Bootstrapper::detectBoardState() {
        // A reply left over from an interrupted previous session would be read as our answer.
        _bridge.drainResponses();

        // The ADV7604's EDID map address (IO 0xFA) resets to 0x00 and the bootstrap sets it to
        // 0x6C; the PCA9539A GPIO expander's configuration registers end up 00/09 after the
        // bootstrap (the GC551's own firmware leaves them at 00/89 on power-up). Both are bus
        // powered and unaffected by a USB reset, so together they identify a board that is
        // still configured from an earlier run. The GC550 bootstrap sets the same values.
        constexpr uint8_t ADV_IO = 0x20;
        constexpr uint8_t PCA9539A = 0x74;
        uint8_t edidMapAddress = 0;
        uint8_t pcaConfig[2] = {0, 0};
        if (_bridge.i2cRead(ADV_IO, 0xFA, &edidMapAddress, 1) != LIBUSB_SUCCESS ||
            _bridge.i2cRead(PCA9539A, 0x06, pcaConfig, sizeof(pcaConfig)) != LIBUSB_SUCCESS) {
            _bridge.drainResponses();
            return BoardState::Unknown;
        }

        if (edidMapAddress != 0x6C) return BoardState::Fresh;
        if (pcaConfig[0] == 0x00 && pcaConfig[1] == 0x09) return BoardState::Configured;

        fprintf(stderr, "Unrecognised board state (ADV7604 EDID map 0x%02X, PCA9539A config %02X %02X)\n",
                edidMapAddress, pcaConfig[0], pcaConfig[1]);
        return BoardState::Unknown;
    }

    void Bootstrapper::reportCryptoAttemptCounters() {
        // The GC550's AT88SC CryptoMemory (checked by the Windows driver) permanently locks a key
        // or password set after a few failed attempts. The bootstrap no longer authenticates, so
        // these counters should never change under this driver; print them so that can be checked.
        // A key set counter is FF when unused and steps FF -> EE -> CC -> 88 -> 00 (locked).
        _bridge.drainResponses();
        constexpr struct { uint8_t address; const char *name; } COUNTERS[] = {
            {0x50, "key set 0"}, {0x60, "key set 1"}, {0x70, "key set 2"},
            {0xB0, "password 0"}, {0xB8, "password 1"}, {0xC0, "password 2"}, {0xD0, "password 4"},
        };
        std::string line;
        for (const auto &counter : COUNTERS) {
            uint8_t value = 0;
            int res = _bridge.cryptoReadConfig(counter.address, &value, 1);
            if (res != LIBUSB_SUCCESS) {
                fprintf(stderr, "Could not read the anti-clone chip's %s counter (%s on %s)\n", counter.name,
                        libusb_error_name(res), _bridge.lastFailure());
                _bridge.drainResponses();
                return;
            }
            char entry[32];
            snprintf(entry, sizeof(entry), "%s%s %02X", line.empty() ? "" : ", ", counter.name, value);
            line += entry;
        }
        printf("Anti-clone chip attempt counters (FF = never failed, 00 = locked): %s\n", line.c_str());
    }

    void Bootstrapper::setAutoInputColourSpace() {
        // The replayed bootstrap leaves the ADV7604's input colour space (IO 0x02 bits 7:4) fixed to
        // full-range RGB (0x1C), as the Windows driver set it for the source connected when the
        // capture was taken. A YCbCr source (e.g. a camera) is then converted as if it were RGB and
        // its colour is destroyed. Use automatic mode instead (0xF: chosen from the source's AVI
        // InfoFrame), as Linux adv7604.c does for HDMI, keeping the output-format bits.
        constexpr uint8_t ADV_IO = 0x20;
        uint8_t io02 = 0;
        int res = _bridge.i2cRead(ADV_IO, 0x02, &io02, 1);
        if (res != LIBUSB_SUCCESS) {
            fprintf(stderr, "Could not read the receiver's input colour space (%s on %s)\n",
                    libusb_error_name(res), _bridge.lastFailure());
            _bridge.drainResponses();
            return;
        }
        uint8_t automatic = static_cast<uint8_t>((io02 & 0x0F) | 0xF0);
        if (automatic == io02) return;
        if (_bridge.i2cWrite(ADV_IO, 0x02, automatic) != LIBUSB_SUCCESS) {
            fprintf(stderr, "Could not set the receiver's input colour space to automatic\n");
            return;
        }
        printf("Receiver input colour space: automatic (IO 0x02 0x%02X -> 0x%02X)\n", io02, automatic);
    }

    void Bootstrapper::resetPictureAdjustment() {
        // When the Windows driver saw the RGB source used for the capture, it also raised the CP
        // brightness (CP 0x3C) to 0x0E, and the replay keeps that offset for every source (black
        // is lifted). Put the CP picture adjustment (contrast, saturation, brightness, hue) back
        // to the neutral values the bootstrap itself writes, as Linux adv7604.c defaults them.
        constexpr uint8_t ADV_CP = 0x22;
        constexpr struct { uint8_t reg; uint8_t neutral; const char *name; } ADJUSTMENTS[] = {
            {0x3A, 0x80, "contrast"}, {0x3B, 0x80, "saturation"}, {0x3C, 0x00, "brightness"}, {0x3D, 0x00, "hue"},
        };
        for (const auto &adjustment : ADJUSTMENTS) {
            uint8_t value = 0;
            int res = _bridge.i2cRead(ADV_CP, adjustment.reg, &value, 1);
            if (res != LIBUSB_SUCCESS) {
                fprintf(stderr, "Could not read the receiver's %s (%s on %s)\n", adjustment.name,
                        libusb_error_name(res), _bridge.lastFailure());
                _bridge.drainResponses();
                continue;
            }
            if (value == adjustment.neutral) continue;
            if (_bridge.i2cWrite(ADV_CP, adjustment.reg, adjustment.neutral) != LIBUSB_SUCCESS) {
                fprintf(stderr, "Could not reset the receiver's %s\n", adjustment.name);
                continue;
            }
            printf("Receiver %s: neutral (CP 0x%02X 0x%02X -> 0x%02X)\n", adjustment.name, adjustment.reg, value,
                   adjustment.neutral);
        }
    }
}
