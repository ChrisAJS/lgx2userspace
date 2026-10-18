#include "StatusMonitor.h"

#include <cstdio>

namespace libusb {

    namespace {
        lgx2::InputState inputStateOf(const lgx2::SignalStatus &status) {
            if (status.signalPresent()) return lgx2::InputState::Signal;
            return status.cableDetected ? lgx2::InputState::NoSignal : lgx2::InputState::NoCable;
        }
    }

    StatusMonitor::StatusMonitor(CommandBridge &bridge) : _bridge{bridge} {
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
                }
            }

            lock.lock();
            _wake.wait_for(lock, POLL_INTERVAL, [this] { return _stopping.load(); });
        }
    }

    int StatusMonitor::readAdvStatus(lgx2::AdvStatusRegisters &regs) {
        // 7-bit I2C addresses of the ADV7604 maps, as programmed by the bootstrap.
        constexpr uint8_t ADV_IO = 0x20;
        constexpr uint8_t ADV_CP = 0x22;
        constexpr uint8_t ADV_HDMI = 0x34;
        constexpr uint8_t ADV_INFOFRAME = 0x3E;

        int res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x0C, &regs.io0c, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x12, &regs.io12, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x6A, &regs.io6a, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x6F, &regs.io6f, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_CP, 0xB1, &regs.cpB1, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x02, &regs.io02, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_IO, 0x60, &regs.io60, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_INFOFRAME, 0x01, &regs.avi01, 1)) != LIBUSB_SUCCESS) return res;
        // Block reads stay at or below 13 bytes, the largest read seen in the bootstrap capture.
        if ((res = _bridge.i2cRead(ADV_HDMI, 0x05, regs.hdmi05to0c, sizeof(regs.hdmi05to0c))) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_HDMI, 0x20, regs.hdmi20to25, sizeof(regs.hdmi20to25))) != LIBUSB_SUCCESS) return res;
        if ((res = _bridge.i2cRead(ADV_HDMI, 0x2A, regs.hdmi2ato35, sizeof(regs.hdmi2ato35))) != LIBUSB_SUCCESS) return res;
        return _bridge.i2cRead(ADV_HDMI, 0x3B, &regs.hdmi3b, 1);
    }
}
