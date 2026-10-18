#include "PipeWireAudioOutput.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/ringbuffer.h>

namespace pipewire {

    struct PipeWireAudioOutput::Ring {
        spa_ringbuffer state;
    };

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

    PipeWireAudioOutput::PipeWireAudioOutput(std::string nodeName, std::string description)
            : _nodeName{std::move(nodeName)}, _description{std::move(description)}, _ring{new Ring},
              _ringData(RING_BYTES) {
        spa_ringbuffer_init(&_ring->state);
    }

    PipeWireAudioOutput::~PipeWireAudioOutput() {
        shutdownAudio();
        delete _ring;
    }

    void PipeWireAudioOutput::initialiseAudio() {
        pw_init(nullptr, nullptr);

        _loop = pw_thread_loop_new("lgx2-audio", nullptr);
        if (_loop == nullptr) throw std::runtime_error("PipeWire: could not create a thread loop");

        // A source node that applications record from. Not auto-connected: linking it to a sink
        // would play the capture through the speakers.
        pw_properties *props = pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio",
                PW_KEY_MEDIA_CLASS, "Audio/Source",
                PW_KEY_NODE_NAME, _nodeName.c_str(),
                PW_KEY_NODE_DESCRIPTION, _description.c_str(),
                nullptr);

        _stream = pw_stream_new_simple(pw_thread_loop_get_loop(_loop), _description.c_str(), props,
                                       streamEvents(&PipeWireAudioOutput::onProcess), this);
        if (_stream == nullptr) throw std::runtime_error("PipeWire: could not create a stream");

        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        spa_audio_info_raw info{};
        info.format = SPA_AUDIO_FORMAT_S16_LE;
        info.rate = RATE;
        info.channels = CHANNELS;
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
        const spa_pod *params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};

        int res = pw_stream_connect(_stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                                    static_cast<pw_stream_flags>(PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
                                    params, 1);
        if (res < 0) throw std::runtime_error(std::string("PipeWire: could not connect the stream: ") + strerror(-res));

        if (pw_thread_loop_start(_loop) < 0) throw std::runtime_error("PipeWire: could not start the thread loop");
        printf("Audio output: PipeWire source \"%s\" (%s)\n", _description.c_str(), _nodeName.c_str());
    }

    void PipeWireAudioOutput::audioFrameAvailable(const uint8_t *audio, uint32_t byteLength) {
        uint32_t index;
        int32_t filled = spa_ringbuffer_get_write_index(&_ring->state, &index);
        if (filled < 0 || static_cast<uint32_t>(filled) + byteLength > RING_BYTES) {
            // Nothing is consuming (no application recording): drop rather than overwrite what the
            // reader owns. A consumer that connects trims the stale backlog anyway.
            _droppedPackets.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        spa_ringbuffer_write_data(&_ring->state, _ringData.data(), RING_BYTES, index & (RING_BYTES - 1), audio,
                                  byteLength);
        spa_ringbuffer_write_update(&_ring->state, index + byteLength);
    }

    void PipeWireAudioOutput::onProcess(void *userData) {
        // PipeWire's real-time thread: no locks, allocation or printing.
        auto *self = static_cast<PipeWireAudioOutput *>(userData);
        pw_buffer *pwBuffer = pw_stream_dequeue_buffer(self->_stream);
        if (pwBuffer == nullptr) return;

        spa_data &data = pwBuffer->buffer->datas[0];
        auto *dst = static_cast<uint8_t *>(data.data);
        if (dst == nullptr) {
            pw_stream_queue_buffer(self->_stream, pwBuffer);
            return;
        }

        uint32_t wanted = data.maxsize / STRIDE * STRIDE;
        if (pwBuffer->requested > 0) {
            wanted = static_cast<uint32_t>(std::min<uint64_t>(pwBuffer->requested * STRIDE, wanted));
        }

        uint32_t index;
        int32_t filled = spa_ringbuffer_get_read_index(&self->_ring->state, &index);
        if (filled < 0) filled = 0;

        if (static_cast<uint32_t>(filled) > MAX_BYTES) {
            // Too far behind (e.g. a backlog from before this consumer connected): skip to the
            // newest TARGET_BYTES.
            uint32_t skip = (static_cast<uint32_t>(filled) - TARGET_BYTES) / STRIDE * STRIDE;
            index += skip;
            filled -= static_cast<int32_t>(skip);
            spa_ringbuffer_read_update(&self->_ring->state, index);
            self->_trims.fetch_add(1, std::memory_order_relaxed);
        }

        uint32_t copied = 0;
        if (!self->_prebuffering || static_cast<uint32_t>(filled) >= TARGET_BYTES) {
            self->_prebuffering = false;
            copied = std::min(static_cast<uint32_t>(filled), wanted) / STRIDE * STRIDE;
            spa_ringbuffer_read_data(&self->_ring->state, self->_ringData.data(), RING_BYTES,
                                     index & (RING_BYTES - 1), dst, copied);
            spa_ringbuffer_read_update(&self->_ring->state, index + copied);
            if (copied < wanted) {
                // Ran dry: play silence and build the buffer back up before resuming.
                self->_prebuffering = true;
                self->_underruns.fetch_add(1, std::memory_order_relaxed);
            }
        }
        if (copied < wanted) memset(dst + copied, 0, wanted - copied);

        data.chunk->offset = 0;
        data.chunk->stride = STRIDE;
        data.chunk->size = wanted;
        pw_stream_queue_buffer(self->_stream, pwBuffer);
    }

    void PipeWireAudioOutput::render() {
    }

    void PipeWireAudioOutput::shutdownAudio() {
        if (_loop == nullptr) return;
        pw_thread_loop_stop(_loop);
        if (_stream != nullptr) pw_stream_destroy(_stream);
        pw_thread_loop_destroy(_loop);
        _stream = nullptr;
        _loop = nullptr;
        pw_deinit();
        printf("PipeWire audio: %llu underruns, %llu latency trims, %llu packets dropped with no consumer\n",
               static_cast<unsigned long long>(_underruns.load()), static_cast<unsigned long long>(_trims.load()),
               static_cast<unsigned long long>(_droppedPackets.load()));
    }
}
