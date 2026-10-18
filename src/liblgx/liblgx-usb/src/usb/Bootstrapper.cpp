#include "Bootstrapper.h"

#include <cstdio>
#include <stdexcept>
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

        bool alreadyBootstrapped = false;
        BoardState boardState = deviceType == lgx2::DeviceType::LGX2 ? detectLgx2BoardState() : BoardState::Unknown;
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

        if (deviceType == lgx2::DeviceType::LGX2) {
            setAutoInputColourSpace();
        }
        return alreadyBootstrapped;
    }

    bool Bootstrapper::videoAlreadyFlowing() {
        std::vector<uint8_t> buffer(CaptureStream::CHUNK_BYTES);
        int check = 0;
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x03, buffer.data(),
                                       static_cast<int>(buffer.size()), &check, 100);
        return res != LIBUSB_ERROR_TIMEOUT;
    }

    Bootstrapper::BoardState Bootstrapper::detectLgx2BoardState() {
        // A reply left over from an interrupted previous session would be read as our answer.
        _bridge.drainResponses();

        // The PCA9539A GPIO expander powers up with both configuration registers at 0xFF (all
        // inputs); the bootstrap sets them to 00/09. It is bus powered and unaffected by a USB
        // reset, so these values identify a board that is still configured from an earlier run.
        constexpr uint8_t PCA9539A = 0x74;
        uint8_t pcaConfig[2];
        if (_bridge.i2cRead(PCA9539A, 0x06, pcaConfig, sizeof(pcaConfig)) != LIBUSB_SUCCESS) {
            _bridge.drainResponses();
            return BoardState::Unknown;
        }
        if (pcaConfig[0] == 0xFF && pcaConfig[1] == 0xFF) {
            return BoardState::Fresh;
        }

        // Confirm with the ADV7604, whose EDID map address (IO 0xFA) the bootstrap sets to 0x6C.
        constexpr uint8_t ADV_IO = 0x20;
        uint8_t edidMapAddress = 0;
        if (pcaConfig[0] == 0x00 && pcaConfig[1] == 0x09 &&
            _bridge.i2cRead(ADV_IO, 0xFA, &edidMapAddress, 1) == LIBUSB_SUCCESS && edidMapAddress == 0x6C) {
            return BoardState::Configured;
        }

        _bridge.drainResponses();
        fprintf(stderr, "Unrecognised board state (PCA9539A config %02X %02X, ADV7604 EDID map 0x%02X)\n",
                pcaConfig[0], pcaConfig[1], edidMapAddress);
        return BoardState::Unknown;
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
}
