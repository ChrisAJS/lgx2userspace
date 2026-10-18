#ifndef LGX2USERSPACE_PIPEWIREAUDIOSOURCE_H
#define LGX2USERSPACE_PIPEWIREAUDIOSOURCE_H

#include <cstdint>
#include <functional>
#include <string>

struct pw_thread_loop;
struct pw_stream;

namespace pipewire {
    // Records from a PipeWire node (by default lgx2d's "LGX2 Audio" source) as 48 kHz 16-bit
    // stereo, handing each buffer to a callback on PipeWire's thread.
    class PipeWireAudioSource {
    public:
        using Callback = std::function<void(const uint8_t *pcm, uint32_t byteLength)>;

        explicit PipeWireAudioSource(Callback callback, std::string target = "lgx2_audio");
        ~PipeWireAudioSource();
        PipeWireAudioSource(const PipeWireAudioSource &) = delete;
        PipeWireAudioSource &operator=(const PipeWireAudioSource &) = delete;

        void start();
        void stop();

    private:
        Callback _callback;
        std::string _target;
        pw_thread_loop *_loop{nullptr};
        pw_stream *_stream{nullptr};

        static void onProcess(void *userData);
    };
}

#endif //LGX2USERSPACE_PIPEWIREAUDIOSOURCE_H
