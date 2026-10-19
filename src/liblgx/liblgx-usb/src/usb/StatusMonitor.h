#ifndef LGX2USERSPACE_STATUSMONITOR_H
#define LGX2USERSPACE_STATUSMONITOR_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "lgxdevice.h"
#include "SignalStatus.h"
#include "CommandBridge.h"

namespace libusb {
    // Periodic read-only HDMI input status poll of the ADV7604, modelled on the Windows driver's
    // poll threads. It issues I2C reads through the CommandBridge while the stream runs on
    // IN 0x83, logs changes in cable/signal/resolution, and publishes the InputState that drives
    // audio muting and the no-signal screen.
    class StatusMonitor {
    public:
        // 50 ms matches the Windows driver's fastest poll (its front-end thread) and bounds how
        // much garbage audio can play after a disconnect; one poll takes ~6 ms.
        static constexpr auto POLL_INTERVAL = std::chrono::milliseconds(50);
        static constexpr int MAX_CONSECUTIVE_FAILURES = 10;

        StatusMonitor(CommandBridge &bridge, lgx2::DeviceType deviceType);
        ~StatusMonitor();

        // One status read before streaming starts (also a diagnostic: the bootstrap's own reads
        // succeed before IN 0x83 starts, so this tells apart "request format wrong" from "bridge
        // does not answer while streaming" when the periodic poll fails).
        void probeOnce();

        void start();
        void stop();

        // Signal until known, and again if the poll gives up, so a broken poll never mutes audio
        // permanently.
        lgx2::InputState inputState() const;

    private:
        CommandBridge &_bridge;
        // 7-bit I2C address of the ADV7604's InfoFrame map, the one map the two bootstraps place
        // differently.
        uint8_t _infoFrameAddress;
        std::atomic<lgx2::InputState> _inputState{lgx2::InputState::Signal};

        std::thread _thread;
        std::mutex _mutex;
        std::condition_variable _wake;
        std::atomic<bool> _stopping{false};

        void pollLoop();
        int readAdvStatus(lgx2::AdvStatusRegisters &regs);
    };
}

#endif //LGX2USERSPACE_STATUSMONITOR_H
