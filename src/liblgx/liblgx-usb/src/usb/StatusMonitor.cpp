#include "StatusMonitor.h"

#include <cstdio>
#include <cstdlib>

namespace libusb {

    namespace {
        lgx2::InputState inputStateOf(const lgx2::SignalStatus &status) {
            if (status.cableDetected && status.hdcpEncrypted) return lgx2::InputState::Protected;
            if (status.signalPresent()) return lgx2::InputState::Signal;
            return status.cableDetected ? lgx2::InputState::NoSignal : lgx2::InputState::NoCable;
        }
    }

    StatusMonitor::StatusMonitor(CommandBridge &bridge, lgx2::DeviceType deviceType)
            : _bridge{bridge}, _infoFrameAddress{static_cast<uint8_t>(deviceType == lgx2::DeviceType::LGX2 ? 0x3E : 0x46)},
              _hasFrontEnd{deviceType == lgx2::DeviceType::LGX2},
              _frontEndDebug{_hasFrontEnd && std::getenv("LGX2_FRONTEND_DEBUG") != nullptr} {
    }

    StatusMonitor::~StatusMonitor() {
        stop();
    }

    lgx2::InputState StatusMonitor::inputState() const {
        return _inputState.load(std::memory_order_relaxed);
    }

    void StatusMonitor::probeOnce() {
        lgx2::AdvStatusRegisters regs;
        auto start = std::chrono::steady_clock::now();
        int res = readAdvStatus(regs);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        if (res == LIBUSB_SUCCESS) {
            lgx2::SignalStatus status = lgx2::SignalStatus::decode(regs);
            _inputState = inputStateOf(status);
            printf("HDMI input (before streaming): %s (status read took %lld ms)\n",
                   status.describe().c_str(), static_cast<long long>(ms));
            reportHdcpLock(status, regs);
            reportFrontEnd();
        } else {
            fprintf(stderr, "HDMI input status read before streaming failed: %s on %s after %lld ms\n",
                    libusb_error_name(res), _bridge.lastFailure(), static_cast<long long>(ms));
            _bridge.drainResponses();
        }
    }

    void StatusMonitor::start() {
        if (_thread.joinable()) return;
        _stopping = false;
        _thread = std::thread(&StatusMonitor::pollLoop, this);
    }

    void StatusMonitor::stop() {
        _stopping = true;
        {
            // Taking the lock orders the flag store before the poll thread's predicate check.
            std::lock_guard<std::mutex> lock(_mutex);
        }
        _wake.notify_all();
        if (_thread.joinable()) {
            _thread.join();
        }
    }

    void StatusMonitor::pollLoop() {
        lgx2::SignalStatus lastStatus;
        bool haveStatus = false;
        int consecutiveFailures = 0;

        std::unique_lock<std::mutex> lock(_mutex);
        while (!_stopping) {
            lock.unlock();

            lgx2::AdvStatusRegisters regs;
            auto pollStart = std::chrono::steady_clock::now();
            int res = readAdvStatus(regs);
            auto pollMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - pollStart).count();
            if (res == LIBUSB_ERROR_NO_DEVICE) {
                // The capture stream's own transfers report the disconnect.
                return;
            }

            if (res != LIBUSB_SUCCESS) {
                if (_stopping) return;
                // A request may have been answered late: flush it so the next reply is not
                // attributed to the wrong register.
                _bridge.drainResponses();
                if (++consecutiveFailures == MAX_CONSECUTIVE_FAILURES) {
                    fprintf(stderr, "HDMI input status poll failed %d times in a row (%s on %s, last attempt %lld ms) — stopping the poll\n",
                            consecutiveFailures, libusb_error_name(res), _bridge.lastFailure(),
                            static_cast<long long>(pollMs));
                    _inputState = lgx2::InputState::Signal; // no longer known: never leave audio muted for good
                    return;
                }
            } else {
                consecutiveFailures = 0;
                lgx2::SignalStatus status = lgx2::SignalStatus::decode(regs);
                _inputState = inputStateOf(status);
                if (!haveStatus || status != lastStatus) {
                    // The poll duration shows how slowly the bridge answers mid-stream.
                    printf("HDMI input: %s (status poll took %lld ms)\n", status.describe().c_str(),
                           static_cast<long long>(pollMs));
                    lastStatus = status;
                    haveStatus = true;
                    reportHdcpLock(status, regs);
                }
                reportFrontEnd();
            }

            lock.lock();
            _wake.wait_for(lock, POLL_INTERVAL, [this] { return _stopping.load(); });
        }
    }

    void StatusMonitor::reportHdcpLock(const lgx2::SignalStatus &status, const lgx2::AdvStatusRegisters &regs) {
        // On the GC551 the IT6664 front-end stays locked after an HDCP-encrypted source: its
        // encryption flag stays set and it does not pass the next source on until the card is
        // power-cycled (see PROTOCOL.md, "IT6664 front-end and HDCP (GC551)").
        if (_reportedHdcpLock || !regs.haveFrontEnd || !status.hdcpEncrypted) return;
        _reportedHdcpLock = true;
        printf("The LGX2 stays in this state after an HDCP-protected source, even once another source is "
               "connected. Unplug and replug its USB cable to use another source.\n");
    }

    void StatusMonitor::reportFrontEnd() {
        if (!_frontEndDebug) return;
        // The registers the Windows driver polls most on the front-end (see PROTOCOL.md,
        // "Runtime polling"); their meaning is not decoded yet. Read only: interrupt flags are
        // cleared by writes, which this never does. RX 0x0F looks like a bank select, so it is
        // shown too (the bootstrap leaves it at 00).
        constexpr struct { uint8_t address; const char *name; uint8_t regs[16]; int count; } BLOCKS[] = {
            {0x2C, "master", {0x05, 0x0A}, 2},
            // RX 0xCF bit 5: input HDCP-encrypted (the driver checks it on RX interrupt 0x09 bit 2).
            {0x58, "rx", {0x0F, 0x09, 0x10, 0x12, 0x13, 0x14, 0x15, 0x19, 0x1A, 0x1B, 0x1D, 0x23, 0x48, 0x99, 0x9A, 0xCF}, 16},
            // TX 0x41 bit 0: the port's HDCP authentication with its sink is done (per the driver).
            {0x44, "tx0", {0x03, 0x07, 0x28, 0x41}, 4},
            {0x45, "tx1", {0x03, 0x07, 0x28, 0x41}, 4},
            {0x46, "tx2", {0x03, 0x07, 0x28, 0x41}, 4},
            {0x47, "tx3", {0x03, 0x07, 0x28, 0x41}, 4},
        };
        std::string line;
        for (const auto &block : BLOCKS) {
            line += line.empty() ? "" : " |";
            line += " ";
            line += block.name;
            for (int i = 0; i < block.count; i++) {
                uint8_t value = 0;
                if (_bridge.i2cRead(block.address, block.regs[i], &value, 1) != LIBUSB_SUCCESS) {
                    _bridge.drainResponses();
                    return;
                }
                char entry[16];
                snprintf(entry, sizeof(entry), " %02X=%02X", block.regs[i], value);
                line += entry;
            }
        }
        if (line == _lastFrontEnd) return;
        _lastFrontEnd = line;
        printf("Front-end:%s\n", line.c_str());
    }

    int StatusMonitor::readAdvStatus(lgx2::AdvStatusRegisters &regs) {
        // 7-bit I2C addresses of the ADV7604 maps, as programmed by both bootstraps (IO 0xF3-0xFE).
        constexpr uint8_t ADV_IO = 0x20;
        constexpr uint8_t ADV_CP = 0x22;
        constexpr uint8_t ADV_HDMI = 0x34;

        int res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x0C, &regs.io0c, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x12, &regs.io12, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x6A, &regs.io6a, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x6F, &regs.io6f, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_CP, 0xB1, &regs.cpB1, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x02, &regs.io02, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x60, &regs.io60, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(_infoFrameAddress, 0x01, &regs.avi01, 1)) != LIBUSB_SUCCESS) return res;
        // Block reads stay at or below 13 bytes, the largest read seen in the bootstrap capture.
        if ((res = _bridge.i2cRead(ADV_HDMI, 0x05, regs.hdmi05to0c, sizeof(regs.hdmi05to0c))) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_HDMI, 0x20, regs.hdmi20to25, sizeof(regs.hdmi20to25))) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_HDMI, 0x2A, regs.hdmi2ato35, sizeof(regs.hdmi2ato35))) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_HDMI, 0x3B, &regs.hdmi3b, 1)) != LIBUSB_SUCCESS) return res;
        if (!_hasFrontEnd) return LIBUSB_SUCCESS;
        constexpr uint8_t FRONT_END_RX = 0x58;
        regs.haveFrontEnd = true;
        return _bridge.i2cRead(FRONT_END_RX, 0xCF, &regs.frontEndRxCF, 1);
    }
}
