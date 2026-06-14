# USB Data Protocol — AverMedia LGX2 (GC551)

This document captures what is known, assumed, and still unknown about the USB
bulk-transfer protocol the LGX2 uses to stream captured video and audio. It was
built by observing live transfers, not by reading a specification — treat
everything here accordingly.

---

## Basics

| Property | Value |
|---|---|
| USB endpoint | `IN 0x83` (`LIBUSB_ENDPOINT_IN \| 0x03`) |
| Transfer size | `0x1FC000` bytes = 2,080,768 bytes per transfer |
| Video format | YUY2 (YCbCr 4:2:2), 2 bytes per pixel |
| Resolution | 1920 × 1080 |
| Full frame size | 1,920 × 1,080 × 2 bytes = 4,147,200 bytes = **1,036,800 uint32s** |

The stream is parsed as a sequence of `uint32` values (little-endian). All
marker constants below are uint32 values in that same byte order.

---

## Marker values (confirmed)

| Name | Value (uint32 LE) | Role |
|---|---|---|
| `VIDEO_FRAME_START_MARKER` | `0xC0FFFF00` | Opens a video sub-chunk header |
| `VIDEO_FRAME_END_MARKER` | `0xC1FFFF00` | Closes a video sub-chunk |
| `AUDIO_FRAME_START_MARKER` | `0x58FFFF00` | Opens an audio packet |
| `AUDIO_FRAME_END_MARKER` | `0xAA5555AA` | Closes an audio packet |

None of these values can appear as a legitimate YUY2 pixel pair, so they are
safe to use as framing markers inside raw pixel data.

---

## Video sub-chunk structure (confirmed by hex dump)

Each video sub-chunk is delimited by a 4-word header and a 1-word trailer:

```
[C0FFFF00]  ← VIDEO_FRAME_START_MARKER  (word 0)
[C3xxxxxx]  ← metadata word             (word 1, see below)
[????????]  ← unknown header word        (word 2)
[????????]  ← unknown header word        (word 3)
[  YUY2  ]  ← actual pixel data begins here
   ...
[C1FFFF00]  ← VIDEO_FRAME_END_MARKER    (end of sub-chunk)
```

The parser must skip all 4 header words before accumulating pixel data.

### Observed first-transfer hex dump (start of a video frame)

```
[0]  0xC0FFFF00   VIDEO_FRAME_START_MARKER
[1]  0xC3010101   metadata word
[2]  0x80100000   unknown header word  (Y0=0, Cb=0 — NOT valid video)
[3]  0x80108010   unknown header word  (or first blanking pixel — ambiguous)
[4+] 0x80108010   video data (blanking / overscan black pixels)
```

`0x80108010` decodes as Y0=16, Cb=128, Y1=16, Cr=128 — standard YUV "black"
(Y=16 is the limited-range black level). This is the HDMI vertical blanking
interval and represents real signal content even though it is invisible.

`0x80100000` at word 2 has Y0=0 and Cb=0, which are out-of-range for limited
YUV (below black, strong blue chroma). It is most likely a device artifact
emitted immediately after the header and is not a real video sample.

---

## Frame assembly (confirmed)

A full 1920 × 1080 YUY2 frame is spread across approximately **16 sub-chunks**
carried in approximately **2 USB transfers** (each transfer holds ~8 sub-chunks).

The parser accumulates pixel data across consecutive `C1FFFF00` boundaries until
at least 1,000,000 uint32s have been collected, then emits one video frame. This
threshold is chosen to be safely above half a frame (~518,400) and safely below
one full frame (1,036,800) so that a complete frame always triggers exactly once.

### Probe / alignment

After the device is bootstrapped the host submits a bulk read. The first
transfer whose first 4 bytes equal `C0FFFF00` marks a sub-chunk boundary. The
driver uses this as its alignment point and begins accumulating from there.

If no frame-aligned transfer is received after several attempts, the device
should be replugged (it may have been bootstrapped already or be in a bad state).

---

## Audio packet structure (confirmed)

Audio packets appear in the stream between video sub-chunks:

```
[58FFFF00]  ← AUDIO_FRAME_START_MARKER
[????????]  ← padding word (purpose unknown)
[  PCM   ]  ← raw audio samples
   ...
[AA5555AA]  ← AUDIO_FRAME_END_MARKER
```

Audio packets can span across a USB transfer boundary; in that case the parser
sets a continuation flag (`_inAudio`) and drains the remainder at the start of
the next transfer.

### Audio format (assumed)

HDMI standard audio is 48 kHz, stereo, 16-bit PCM. This has not been confirmed
by decoding the audio data — it is assumed based on the HDMI specification.

---

## Video source compatibility

The device captures and delivers pixel data in **YUY2 (YCbCr 4:2:2)** format
regardless of what the video source actually outputs on the HDMI wire.

### Sources known to work well
- Nintendo Switch (outputs YCbCr over HDMI by default)

### Sources with colorspace issues

**MacBook / Apple computers** — Apple defaults to outputting HDMI in RGB
colorspace (or a non-standard YCbCr range). Because the driver passes the raw
bytes directly to SDL as YUY2, an RGB source will be interpreted incorrectly,
producing the wrong colours (typically a strong orange/amber tint with visible
horizontal scan-line artefacts).

**Sony A7C III (and likely other cameras)** — Camera HDMI outputs typically use
a different YCbCr range or colorspace than consumer game consoles. Video does
come through at reduced quality but with visible hue/saturation shifts.

The fix in all these cases is a colorspace conversion step before rendering —
something the driver does not currently implement.

---

## Known unknowns

Things we do not know and would need further reverse-engineering or a device
specification to answer:

- **Exact meaning of the 4-word sub-chunk header.** Word 1 (`0xC3xxxxxx`) likely
  contains a sequence counter in its lower bytes, but this is unconfirmed. Words
  2 and 3 are completely unknown.

- **Whether the header word count is always 4.** Every observed sub-chunk has had
  a 4-word header. If it varies (e.g., with resolution or frame rate), the parser
  will misalign. A more robust approach would scan for the first video-valid
  uint32 after `C0FFFF00` rather than using a fixed skip count.

- **Exact audio format.** Sample rate, bit depth, channel count, and endianness
  are all assumed from HDMI defaults and have not been verified by decoding the
  PCM data.

- **The "first 4 pixels" artefact.** The very first pixel pair in each rendered
  frame appears slightly wrong (observed as `0x80100000`: below-black luma,
  strong blue chroma). Whether this is our parser including 1 header word it
  shouldn't, a device-side DMA artefact, or an SDL YUY2 decoder edge-case at
  the texture boundary is unresolved.

- **Whether `C0FFFF00` / `C1FFFF00` can appear in real pixel data.** Treated as
  impossible because those YUV values are well outside the valid limited-range
  signal. Has not been rigorously proven for all possible video sources.

- **Other resolutions and frame rates.** All testing was at 1920 × 1080 @ 60 Hz.
  The sub-chunk count, header word count, and accumulation threshold may all
  differ at other resolutions or frame rates.

- **GC550 (LGX) differences.** The GC550 uses a separate bootstrap command
  sequence but presumably a similar stream format. No stream-level testing has
  been done on a GC550.
