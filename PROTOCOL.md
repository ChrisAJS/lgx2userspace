# USB Data Protocol — AverMedia LGX2 (GC551)

This document captures what is known, assumed, and still unknown about the USB
bulk-transfer protocol the LGX2 uses to stream captured video and audio. It was
built by observing live USB transfers and reading the Windows driver INF file
(`avmu3gc550_x64.inf`) — not from a specification. Treat everything accordingly.

---

## Hardware

The device is built around a **Cypress EZ-USB FX3** USB 3.0 controller
(programmable ARM9 core) paired with an **ITE Technology HDMI receiver** IC.
The INF file references chip indices `0:6604/7002` and `1:7604`, which are
believed to be ITE IT6604/IT7002 and IT7604 HDMI receiver variants used in
different board revisions (GC550 vs GC551).

The ITE chip outputs captured video to the FX3 over a **dual-channel 10-bit
parallel bus (BT.1120)**. BT.1120 carries luma (Y) and chroma (CbCr) on
separate 10-bit channels at pixel clock, with SAV (Start of Active Video) and
EAV (End of Active Video) timing reference preambles on both channels
simultaneously. The preamble sequence on each channel is: `3FF 000 000 XY`,
where `XY` encodes field, blanking, and H-sync status.

The FX3 firmware reads the BT.1120 bus, detects these timing references, and
translates them into the `0xCxFFFF00` USB stream markers described below. The
`FF FF` in every marker directly corresponds to the two `3FF` preamble words
(one per channel, upper 8 bits = `0xFF`) arriving simultaneously on the
parallel bus. This connection explains the entire sub-chunk framing structure.

The FX3 firmware is loaded during the bootstrap sequence. The BMP files
shipped alongside the driver (`aver_custom_no_signal.bmp`,
`aver_custom_hdcp_protection.bmp`, `aver_custom_content_protection.bmp`,
`aver_custom_out_of_range.bmp`) are believed to be overlay images uploaded
into the FX3's framebuffer during bootstrap — they appear as embedded data
inside the bootstrap command payloads in `commanddata_lgx2.h`.

---

## Basics

| Property | Value |
|---|---|
| USB endpoint | `IN 0x83` (`LIBUSB_ENDPOINT_IN \| 0x03`) |
| Transfer size | `0x1FC000` bytes = 2,080,768 bytes per transfer |
| Video format | YUY2 (YCbCr 4:2:2), 2 bytes per pixel, limited range |
| Resolution | 1920 × 1080 |
| Full frame size | 1,920 × 1,080 × 2 bytes = 4,147,200 bytes = **1,036,800 uint32s** |
| Audio endianness | Little-endian (confirmed via `AUDIO_ENDIAN=0` in INF) |
| Colour range | Limited range YCbCr (confirmed via `ColorRange=2` in INF) |

The stream is parsed as a sequence of `uint32` values (little-endian). All
marker constants below are uint32 values in that same byte order.

---

## Marker values (confirmed)

| Name | Value (uint32 LE) | Byte order on wire | BT.1120 origin | Role |
|---|---|---|---|---|
| `VIDEO_FRAME_START_MARKER` | `0xC0FFFF00` | `00 FF FF C0` | SAV — `C0` type byte, `FF FF` = dual-channel preamble | Opens a video frame (one per 1080p frame) |
| `VIDEO_FRAME_END_MARKER` | `0xC1FFFF00` | `00 FF FF C1` | EAV — `C1` type byte, `FF FF` = dual-channel preamble | Closes a video frame |
| `AUDIO_FRAME_START_MARKER` | `0x58FFFF00` | `00 FF FF 58` | `58` type byte, `FF FF` = dual-channel preamble | Opens an audio packet |
| `AUDIO_FRAME_END_MARKER` | `0xAA5555AA` | `AA 55 55 AA` | Unknown — may not derive from BT.1120 | Closes an audio packet (see below — never scanned by the driver) |

**Driver-verified detection method** (`avmu3_x64.sys`, marker scanner
`FUN_1400a6fa4`): none of the 32-bit constants above exist anywhere in the
driver binary. Markers are found by scanning raw *bytes* (never assuming
4-byte alignment) for exactly two patterns:

- **4-byte form**: `00 FF FF T` where `T ∈ {C0, C1, 58}` — the standard marker.
- **5-byte form**: `X 00 FF FF T` where `X` is *any* byte and
  `T ∈ {C0, C1, 58}` — a leading garbage byte is absorbed into the marker and
  consumed along with it (the driver tolerates one extra byte before a marker).

`AUDIO_FRAME_END_MARKER` (`0xAA5555AA`) is **never scanned for**. Audio packets
are terminated by their declared length (see audio packet structure below); if
the end marker is present on the wire, it falls inside the declared-length
region.

The `FF FF` present in every video and audio start marker corresponds to
the BT.1120 SAV/EAV preamble: both the Y and C parallel channels simultaneously
transmit `3FF` (10-bit all-ones, upper 8 bits = `0xFF`) as the first word of
every timing reference sequence. The FX3 firmware packs both channel bytes into
a single 32-bit word, producing `FF FF` in positions [1:2]. The leading byte
(`C0`, `C1`, `58`) is the FX3's own packet-type indicator layered on top.

None of these values can appear as a legitimate YUY2 pixel pair in limited-range
content (which constrains Y to 16–235 and Cb/Cr to 16–240), so they are safe
to use as framing markers inside raw pixel data.

---

## Video frame structure (driver-verified)

**Correction to the earlier hex-dump guess:** the header is *not* 4 words. The
driver (`FUN_1400a887c`, `FUN_1400a84f0`) consumes a **byte-counted header
immediately after the marker**, and pixel data begins right after it:

```
[00 FF FF C0]  ← VIDEO_FRAME_START_MARKER   (4 bytes)
[ b0  b1  b2  b3 ]  ← SAV header            (4 bytes, checksum-validated)
[  YUY2  ]        ← pixel data begins here (byte 8 — word index 2)
   ...
[00 FF FF C1]  ← VIDEO_FRAME_END_MARKER    (4 bytes)
[ c0  c1  c2  c3  c4 ]  ← EAV header       (5 bytes, checksum-validated)
```

- **SAV header** (4 bytes, `FUN_1400a8c60`): valid iff
  `b3 == (b0 + b1 + b2 - 0x40) mod 256`. An invalid checksum resets the parser
  to idle state. Byte `b0` is stored and compared against the EAV header's
  first byte; a mismatch increments a desync counter. **The checksum is the
  only validity gate** — verified against a live capture where the checksum holds
  for every real SAV (66/66) but bit 0 of `b2` is *usually set* (b2 distribution
  `{1:1, 128:1, 129:64}`), so it must **not** be required to be clear. The
  earlier "bit 0 of b2 must be clear / bit 7 halves the threshold" reading was
  wrong; on a single-frame-per-pair stream (below) there is no separate
  frame-size threshold to halve.
- **EAV header** (5 bytes, `FUN_1400a8b34`): valid iff
  `c4 == (c0 + c1 + c2 + c3 - 0x3F) mod 256`. Video data ends at the byte
  immediately before the EAV marker (no tail trimming).
- **Audio header** (4 bytes, `FUN_1400a8a88`): a big-endian uint32 giving the
  exact payload length in bytes.

The header format is consistent with an I2C transaction frame relayed over USB
(slave address + data bytes + checksum) — see "Driver parser" below.

### Observed first-transfer hex dump (start of a video frame), reinterpreted

```
[0]  0xC0FFFF00   VIDEO_FRAME_START_MARKER
[1]  0xC3010101   SAV header (bytes 01 01 01 C3 — note: b0=0x01, checksum byte = 0xC3)
[2]  0x80100000   FIRST DATA WORD (bytes 00 00 10 80)
[3]  0x80108010   data (blanking / overscan black pixels)
[4+] 0x80108010   video data
```

`0x80108010` decodes as Y0=16, Cb=128, Y1=16, Cr=128 — standard limited-range
YUV black (Y=16 is the digital black level). This is the HDMI vertical blanking
interval and represents real signal content even though it is invisible.

`0x80100000` at word 2 is **real data, not a header**: its first two bytes are
`00 00` (Y0=0, Cb=0 — below black), i.e. the "first pixel artefact" is a
device-side below-black byte at the start of every frame's payload region. The
driver copies it into the frame buffer verbatim; it is not filtered.

The INF file contains a registry key `STRHEADER_FLAG=0` (stream header flag).
Its exact meaning is unknown but a value of 0 may indicate the baseline header
format observed here. A non-zero value might indicate a different layout (e.g.
at different resolutions or frame rates).

---

## Frame assembly (live-capture-verified)

**Correction to the earlier "16 sub-chunks per frame" model:** a full
1920 × 1080 YUY2 frame is **one SAV→EAV pair**, not ~16. Each pair carries
exactly one complete 4,147,200-byte frame of video data between its SAV and EAV
markers, followed by a single length-declared audio packet (~3,200 bytes). The
EAV-to-EAV period is ≈ **4,159,712 bytes** (frame + audio), giving ~64–65 frames
across a 266 MB capture — matching the observed marker counts (66 SAV / 65 EAV)
and the parsed output (63 byte-identical video frames, 65 audio packets,
desync count 0).

The driver's logic (`FUN_1400a745c`, `FUN_1400a84f0`) is still a state machine
driven by marker class + checksum-validated headers, but on this stream it
reduces to: **flush the video data at each EAV boundary; emit a frame when the
just-flushed span is (within a 0x4000 quantum of) exactly one 1080p frame.**

- Video data between a valid SAV and its matching EAV is flushed to the frame
  buffer (via `ParseAVerData::copyVideoDataToFrameBuffer`, dispatched through a
  4-way vtable selected by header bit fields).
- Frame completion is gated on span size: smaller spans are startup partials
  (the first partial chunk after boot) and larger ones indicate a desync —
  neither is shown. A clean, checksum-valid SAV→EAV pair re-arms emission after
  a gap; an EAV whose `c0` mismatches the SAV's `b0` increments the desync
  counter but still emits (the data is still a real frame).

The INF confirms the streaming mode as `STREAMINGMODE=1` ("put StreamPtr in
Queue"), consistent with the async producer-consumer queue architecture used by
this driver: each completed bulk URB on endpoint IN 0x83 is handed to the
parser (`FUN_1400a6360` → `FUN_1400a745c`) independently.

### Probe / alignment

After the device is bootstrapped the host submits a bulk read. The parser does
not require the transfer to begin on a marker: it scans from byte 0 (after
priming its straddle buffer, see below) and simply waits in idle state until a
valid SAV marker + header pair appears. Alignment therefore happens naturally at
the first frame boundary inside the stream.

If no frame-aligned transfer is received after several attempts, the device
should be replugged (it may have been bootstrapped already or be in a bad state).

---

## Audio packet structure (driver-verified)

One audio packet appears in the stream after each video frame's EAV:

```
[00 FF FF 58]  ← AUDIO_FRAME_START_MARKER (4 bytes)
[ L3 L2 L1 L0 ] ← big-endian uint32 payload length (4 bytes)
[  PCM   ]     ← exactly `length` bytes of audio samples (little-endian, confirmed)
```

The driver never scans for an end marker: the payload is exactly `length`
bytes long (`FUN_1400a8a88`), and any observed `AA5555AA` trailer sits inside
or immediately after that region. If the payload extends past the end of a USB
transfer, the remainder is tracked in a byte counter and drained from the start
of the next transfer; on an audio→video transition the unflushed tail is copied
into the audio buffer before video processing resumes.

### Audio format (partially confirmed)

- **Endianness**: little-endian (confirmed via `AUDIO_ENDIAN=0` in INF)
- **Sample rate**: assumed 48 kHz (HDMI standard, unconfirmed)
- **Bit depth / channels**: assumed 16-bit stereo (HDMI standard, unconfirmed)

---

## Driver parser internals (avmu3_x64.sys, reverse-engineered)

All addresses below are in the x64 driver at image base `0x140000000`. The
parser is a class (strings call it `ParseAVerData`) operating on one context
struct; field offsets cited are relative to that struct.

| Function | Address | Role |
|---|---|---|
| URB completion → parser entry | `FUN_1400a6360` | Hands each completed bulk IN transfer to the parser |
| Main parse loop / state machine | `FUN_1400a745c` | Processes one USB transfer end-to-end |
| Marker scanner (raw byte scan) | `FUN_1400a6fa4` | Finds `00 FF FF T` / `X 00 FF FF T`, T ∈ {C0, C1, 58} |
| Transfer-start straddle scan | `FUN_1400a6f20` | Reassembles markers across transfer boundaries (below) |
| Marker-class state mapper | `FUN_1400a6d50` | Maps (current state, marker byte) → new state class |
| Sub-chunk header dispatch | `FUN_1400a887c` | SAV→4B header, EAV→5B header, audio→4B length header |
| SAV header decoder | `FUN_1400a8c60` | Checksum + bit fields (below) |
| EAV header decoder | `FUN_1400a8b34` | Checksum + SAV/EAV byte-0 match check |
| Audio header decoder | `FUN_1400a8a88` | Big-endian uint32 length |
| Data-range computation | `FUN_1400a84f0` | Computes payload start/end offsets, frame counter logic |
| Frame-buffer copy | `FUN_1400a697c` | `copyVideoDataToFrameBuffer`, 4-way vtable dispatch |

### State machine

States stored at context+0x800250 (current) / 0x800254 (pending):

```
 0 idle ──C0──▶ 1 SAV seen ──valid 4B header──▶ 2 video data
 6 audio ─C0──▶ 1          2 video ──C1──▶ 3 EAV seen ──valid 5B header──▶ 4 post-EAV (blanking)
 7 audio-done ─C0──▶ 1     4 post-EAV ──0x58──▶ 5 audio-seen ──len header──▶ 6 audio data
 any invalid checksum ──▶ 0 idle (desync, rescan for next SAV)
```

### Frame-sync shenanigans (the interesting parts)

1. **Markers straddle USB transfer boundaries.** At the end of every transfer
   the last 4 bytes are copied into a scratch buffer (context+0x800298); at
   the start of the next transfer its first 4 bytes are appended after them
   (context+0x80029c) and that **8-byte straddle window is scanned** for
   markers (`FUN_1400a6f20`). A marker split across the 0x1FC000-byte
   boundary is therefore reassembled, with its pre-boundary bytes accounted
   against the previous transfer.
2. **Partial-marker tail trimming.** If a transfer ends mid-frame while
   video data is active, trailing bytes matching the marker prefix pattern
   (`00`, `00 FF`, `00 FF FF` — up to 4 bytes) are trimmed off the end of
   the flushed video region before it is copied to the frame buffer, so a
   half-received marker at the transfer tail is never counted as pixel data.
3. **No word alignment assumed.** The scanner walks raw bytes (the 5-byte
   `X 00 FF FF T` form absorbs one arbitrary leading byte), so framing
   survives an off-by-one anywhere in the stream; a desync only recovers at
   the next valid SAV marker + checksum.
4. **Checksum-validated headers** (SAV: `b3 == b0+b1+b2−0x40`; EAV:
   `c4 == c0+c1+c2+c3−0x3F`, all mod 256) act as the real sync guard — the
   marker pattern alone is not trusted. The SAV header's byte 0 must match the
   EAV header's byte 0 or a desync counter (context+0x8002c8) increments.
5. **Tolerated anomalies:** a SAV marker arriving while already in video data
   increments a counter (context+0x8002c4) instead of desyncing; an audio
   payload crossing a transfer boundary is continued via a remaining-byte
   counter; frame completion requires the accumulated byte count to land on a
   16 KiB quantum below the threshold.
6. **First 4 bytes / last 4 bytes** of every transfer are always captured into
   scratch buffers (context+0x80029c / 0x800298) even outside marker
   detection — these double as the straddle buffer and an "I2C receive
   result" capture area (the relayed I2C traffic on this stream is handled by
   a plain memcpy helper, `decodeI2CRecvResult`, despite its name).

### Replication notes for a userspace parser

To behave like the Windows driver: scan raw bytes for both marker forms;
require the checksum-validated header after each marker (SAV 4B, EAV 5B, audio
4B big-endian length); copy video payloads per SAV→EAV pair (data starts at
byte 8 after a SAV marker, ends immediately before the EAV marker); keep a
4-byte tail carry-over between transfers and scan the straddle window; trim
partial marker tails from video flushes. On this stream each pair is one full
frame, so emit a frame when the just-flushed span is within a 0x4000 quantum of
exactly one 1080p frame (rejecting startup partials and desynced oversized
spans) — verified byte-identical against a live capture.

### Command & timing model (driver-verified)

The driver is USBDI2-based (all USB traffic is USBD requests). The answer to
"does the host command the device *when* to send data?" is **no** — data flow
is device-driven:

- A **pool of outstanding bulk IN URBs** is maintained: submit
  (`FUN_1400a5f3c`) → completion (`FUN_1400a6360`) → parse
  (`FUN_1400a745c`) → immediately re-queue. An outstanding-URB counter
  (context+0x800320) is decremented on teardown; when it reaches zero an event
  is set (`FUN_1400a5f14`) that wakes a worker thread to refill the pool. This
  matches `STREAMINGMODE=1` ("put StreamPtr in Queue").
- **Neither the URB completion path nor the parser issues any I2C command.**
  The FX3 firmware continuously serialises BT.1120 into sub-chunks and pushes
  them onto bulk IN at pixel-clock pace; USB bulk NAK back-pressure paces the
  host naturally. Nothing on the wire says "send frame N now".
- **Commands are one-shot configuration** via the I2C HAL
  (`CRXADV7604::HAL_I2CWriteByte` = `FUN_140030c1c`,
  `HAL_I2CReadBlock` = `FUN_140030160`):
  - *Bootstrap* = zero-terminated tables of 2–3 byte
    `(channel, register, value)` records (e.g. `DAT_1400cce30`,
    `DAT_1400ceda0`, `DAT_1400d0030`) walked by small loop functions — these
    are the "command data" payloads of `commanddata_lgx2.h`. They program the
    ITE receiver (input/output format, BT.1120 dual-channel output, timing);
    once configured, streaming is continuous.
  - *Mode changes* use per-resolution register tables
    (`CVX8897::REG_1920_1080_60P_422_To_...`, `TvParmFlagChanged`).
  - *Status reads* (HdmiSyncInfo / Vsync / Hsync / AVI info frames) are
    reporting only (Ks reference clock, format negotiation).
- The `"cmd fail triggler on"` string is an **error latch**, not a stream
  trigger: after >10 consecutive I2C command failures the HAL latches
  (context+0x87c) and stops sending further commands.

**Replication implication:** keep N large read URBs outstanding and parse
continuously — no per-frame or per-transfer command is required. The only
commands that affect *timing* are the bootstrap writes that put the ITE chip
into continuous BT.1120 output mode.

### Stall & desync behaviour (driver-verified)

There is **no "resync" command** anywhere in the driver — a string sweep
confirms every sync-related symbol is either a status *read*
(HdmiSyncInfo / Vsync / Hsync info) or Ks stream-pointer state logging
(`VSTATE_SyncWait` / `VSTATE_SyncChecking`, referenced only from a name table
at `DAT_14010e620` used by the Ks state machine in `FUN_140060264`). The
driver never tells the device "you're getting garbage, resync" — it simply
never creates the condition where that would be needed:

- **Reads are fully decoupled from consumption.** URB completions parse and
  re-queue on system-thread context (not any UI/event loop); a worker thread
  refills the pool when it drains. A stalled consumer only slows *downstream*
  processing — data queues up in the stream pointer and whole frames get
  dropped, but USB reads never stop. The FX3 therefore sees no read gap:
  bulk IN NAK back-pressure just pauses the stream instead of overflowing.
- **Dropping is safe; stalling is not.** Every video frame starts with a
  SAV marker, so discarding whole URBs re-syncs cleanly at the next frame.
  What breaks a userspace parser is a *gap in the byte stream* (no URB
  posted while the FX3's internal buffer overflows):
  - The straddle reassembly / partial-marker trim assumes transfers
    concatenate with no missing bytes — a marker split by a gap is never
    reassembled.
  - Audio packets are length-declared: losing data mid-packet desyncs the
    "audio remaining" counter, and in audio state the parser counts bytes
    instead of scanning for markers, so it consumes video data as audio until
    the bogus counter expires — many frames of garbage.
- **Replication fix:** (1) keep N async read URBs outstanding with a
  dedicated reader thread / libusb transfer callbacks so parsing never stops;
  (2) treat a detected gap (read-latency spike, dropped bytes) as a parser
  reset — return to idle, zero the audio-remaining counter, and require one
  clean SAV→EAV pair (checksums valid, EAV c0 matches SAV b0) before
  accepting frames again. This mirrors what the driver's desync counter
  (context+0x8002c8, incremented on SAV/EAV mismatch) tracks.

---

## Video source compatibility

The device hardcodes **limited-range YCbCr** (`ColorRange=2` in the INF) and
performs no colourspace conversion. The captured stream is delivered as-is.

### Sources known to work well
- Nintendo Switch — outputs limited-range YCbCr over HDMI by default

### Sources with colourspace issues

**MacBook / Apple computers** — Apple defaults to outputting HDMI as full-range
RGB (or full-range YCbCr depending on the display negotiation). Because the
device interprets all incoming data as limited-range YCbCr, full-range RGB
sources produce strongly wrong colours (typically an orange/amber tint). This is
not a driver bug — the device simply does not support RGB input at the USB
stream level. A colourspace conversion step in the driver before rendering would
fix it, but is not currently implemented.

**Sony A7C III (and likely other cameras)** — Camera HDMI outputs vary in
colourspace and range by model and settings. The A7C III produces recognisable
video with visible hue/saturation shifts, consistent with a range or matrix
mismatch.

The INF notes the device supports `VGA` mode (RGB24), `YUV422`, and `YUV444`
as a compile-time choice (the `VGA` registry key is commented out, leaving
YUV422 as the active format). It is not known whether switching the capture mode
via the driver API or device firmware would allow correct RGB ingestion.

---

## Known unknowns

- ~~**Exact meaning of the sub-chunk header.**~~ **Answered by the driver:**
  the header is a checksum-validated I2C-style frame (SAV: 4 bytes,
  `b3 == b0+b1+b2−0x40`; EAV: 5 bytes, `c4 == c0+c1+c2+c3−0x3F`), not an
  opaque metadata word. The per-byte meaning beyond the checksum is still
  unknown; on a live capture only the checksum gates validity (bit 0 of `b2` is
  usually *set*, so no separate "validity flag" bit exists).

- **Whether the header byte count is always 4/5.** Every observed sub-chunk
  matches the driver's fixed sizes (SAV 4B, EAV 5B, audio 4B length).
  `STRHEADER_FLAG=0` in the INF may be the control for this. If it varies
  (e.g., with resolution or frame rate), the parser will misalign.

- **Exact audio sample rate and bit depth.** Endianness is confirmed
  little-endian; sample rate and bit depth are still assumed from HDMI defaults.

- ~~**The "first pixels" artefact.**~~ **Explained by the driver:** payload
  data starts immediately after the 4-byte SAV header (byte 8). The first data
  word of a frame is below-black luma (e.g. `0x80100000`, Y=0) — vertical
  blanking / overscan content rather than active picture. Since there is one
  SAV per frame, this is one leading pixel pair per *frame* (not per sub-chunk).
  The Windows driver copies it verbatim into the frame buffer; a userspace
  parser can drop the first byte (or first pixel pair) of each frame to match
  the visible output, or keep it to match the driver's raw behaviour.

- **Other resolutions and frame rates.** All testing was at 1920 × 1080 @ 60 Hz.
  The bytes-per-SAV→EAV-pair, header layout, and audio packet size may all
  differ at other resolutions (the one-frame-per-pair structure is verified only
  for 1080p).

- **GC550 (LGX) differences.** The INF shares most settings between GC550 and
  GC551 but some keys differ (e.g. `USB3ANALOG` vs `USB3ANALOG_GC551`). No
  stream-level testing has been done on a GC550.

- **STRHEADER_FLAG.** The INF sets this to 0 for both GC550 and GC551. Its
  precise effect on the USB stream header layout is unknown.
