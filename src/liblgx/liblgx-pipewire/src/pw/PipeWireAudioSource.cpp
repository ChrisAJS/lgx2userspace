#include "PipeWireAudioSource.h"

#include <cstring>
#include <stdexcept>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

namespace pipewire {

    namespace {
        const pw_stream_events *streamEvents(void (*process)(void *)) {
            static pw_stream_events events = [process] {
                pw_stream_events e{};
                e.version = PW_VERSION_STREAM_EVENTS;
                e.process = process;
                return e;
            }();
            return &events;
        }
    }

    PipeWireAudioSource::PipeWireAudioSource(Callback callback, std::string target)
            : _callback{std::move(callback)}, _target{std::move(target)} {
    }

    PipeWireAudioSource::~PipeWireAudioSource() {
        stop();
    }

    void PipeWireAudioSource::start() {
        pw_init(nullptr, nullptr);

        _loop = pw_thread_loop_new("lgx2-audio-in", nullptr);
        if (_loop == nullptr) throw std::runtime_error("PipeWire: could not create a thread loop");

        pw_properties *props = pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio",
                PW_KEY_MEDIA_CATEGORY, "Capture",
                PW_KEY_MEDIA_ROLE, "Production",
                PW_KEY_TARGET_OBJECT, _target.c_str(),
                nullptr);

        _stream = pw_stream_new_simple(pw_thread_loop_get_loop(_loop), "lgx2userspace", props,
                                       streamEvents(&PipeWireAudioSource::onProcess), this);
        if (_stream == nullptr) throw std::runtime_error("PipeWire: could not create a stream");

        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        spa_audio_info_raw info{};
        info.format = SPA_AUDIO_FORMAT_S16_LE;
        info.rate = 48000;
        info.channels = 2;
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
        const spa_pod *params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};

        int res = pw_stream_connect(_stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                                    static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                                 PW_STREAM_FLAG_MAP_BUFFERS |
                                                                 PW_STREAM_FLAG_RT_PROCESS),
                                    params, 1);
        if (res < 0) throw std::runtime_error(std::string("PipeWire: could not connect: ") + strerror(-res));

        if (pw_thread_loop_start(_loop) < 0) throw std::runtime_error("PipeWire: could not start the thread loop");
    }

    void PipeWireAudioSource::onProcess(void *userData) {
        auto *self = static_cast<PipeWireAudioSource *>(userData);
        pw_buffer *pwBuffer = pw_stream_dequeue_buffer(self->_stream);
        if (pwBuffer == nullptr) return;

        const spa_data &data = pwBuffer->buffer->datas[0];
        if (data.data != nullptr && data.chunk->size > 0) {
            const auto *pcm = static_cast<const uint8_t *>(data.data) + data.chunk->offset;
            self->_callback(pcm, data.chunk->size);
        }
        pw_stream_queue_buffer(self->_stream, pwBuffer);
    }

    void PipeWireAudioSource::stop() {
        if (_loop == nullptr) return;
        pw_thread_loop_stop(_loop);
        if (_stream != nullptr) pw_stream_destroy(_stream);
        pw_thread_loop_destroy(_loop);
        _stream = nullptr;
        _loop = nullptr;
        pw_deinit();
    }
}
