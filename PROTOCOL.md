# USB Data Protocol — AverMedia LGX2 (GC551)

This document captures what is known, assumed, and still unknown about the USB
bulk-transfer protocol the LGX2 uses to stream captured video and audio. It was
built by observing live USB transfers and reading the Windows driver INF file
(`avmu3gc550_x64.inf`) — not from a specification. Treat everything accordingly.

---

## Hardware

The device is built around a **Cypress EZ-USB FX3** USB 3.0 controller
(programmable ARM9 core). **Correction:** the capture-side HDMI receiver is an
**Analog Devices ADV7604**, not an ITE part — the driver's class names are
`CRXADV7604::*` (118 methods, incl. `ADIAPI_*`/`HAL_*` from ADI's reference
code), and the bootstrap stream programs the ADV7604's I2C map addresses and
EDID RAM (see "Bootstrap command stream" below). The INF's `1:7604` is the
ADV7604; `0:6604/7002` likely refers to the GC550's ITE IT6603/6604
(`CVampVDecoderIT6603::*` in the driver). The GC551 also carries a
**PCA9539A** 16-bit GPIO expander (`CPCA9539A::*`) and a separate 4K-capable
HDMI 2.0 front-end (passthrough path), also found in the bootstrap stream.

The receiver outputs captured video to the FX3 over a **dual-channel 10-bit
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

**Correction:** the bootstrap does not upload FX3 firmware or images. The BMP
files shipped alongside the driver (`aver_custom_no_signal.bmp`,
`aver_custom_out_of_range.bmp`, `aver_custom_hdcp_protection.bmp`,
`aver_custom_content_protection.bmp`) are **never sent to the device**.
`FUN_140021c3c` loads them host-side from `\SystemRoot\System32\drivers\`,
accepts them only if they are exactly 640 × 480, and converts each into three
640 × 480 × 2 (YUY2) kernel buffers (`FUN_140020840`). The same function
allocates a 0x465000-byte (1920 × 1200 × 2) buffer pre-filled with
limited-range black `0x80108010`. The "no signal" / "out of range" / HDCP
screens are therefore synthesised by the Windows driver into the Ks stream; a
userspace replacement has to draw its own. The bootstrap stream contains no
bitmap — its largest single write is 32 bytes (an EDID block).

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

## Bootstrap command stream (decoded from `commanddata_lgx2.cpp`)

The bootstrap string is a **replayed USB capture of the Windows driver**, not a
hand-written init table: ~9,900 commands covering chip init *plus* the
driver's subsequent interrupt/status polling (e.g. the ADV7604 IO-map
interrupt registers 0x44–0x82 are read-and-cleared ~50 times). A few reads even
carry the sniffed response inline (`<13,"0008…"`, which `std::stoi` ignores).

### Wire format (USB→I2C bridge on bulk OUT 0x01 / IN)

Tokens: `>hex` = bulk OUT write of those bytes; `<N` = bulk IN read of N bytes.
Addresses are **7-bit I2C**.

| Bytes | Meaning |
|---|---|
| `01 AA LL RR.. DD..` | I2C write: slave `AA`, `LL`-byte register address `RR`, then data (auto-increment; up to 32 bytes seen) |
| `02 AA LL NN RR..` + `<NN` | I2C read: `NN` bytes from register `RR` of slave `AA` |
| `0E 02 07 FE 02 00` + `<512` | Non-I2C bridge command, first in the stream; reads 512 bytes (meaning unknown — device/firmware info?) |
| `14` | Non-I2C bridge command, single byte, issued once mid-stream (cmd #7732, right after the passthrough chip's TX blocks are reconfigured). Unknown — candidate for "start/arm video path" |

(The GC550 stream in `commanddata_lgx.cpp` uses additional opcodes `05`–`08`,
not decoded.)

### I2C devices addressed

| 7-bit addr | Device | Evidence |
|---|---|---|
| `0x74` | PCA9539A GPIO expander | regs 2/3 output, 6/7 config; output 0x02 toggled `71→31→71` = **reset pulse** (bit 6) to a chip |
| `0x20` | ADV7604 IO map | writes IO 0xF3–0xFE = sub-map addresses (8-bit form): AVLINK 0x84, CEC 0x80, INFOFRAME 0x7C, ESDP 0x70, DPP 0x78, AFE 0x4C, REP 0x64, EDID 0x6C, HDMI 0x68, TEST 0x52, CP 0x44, VDP 0x48 |
| `0x22 / 0x26 / 0x32 / 0x34 / 0x36 / 0x3E` | ADV7604 CP / AFE / Repeater(KSV) / HDMI / EDID / InfoFrame maps | as programmed above (8-bit ÷ 2) |
| `0x2C` | Master of an ITE-style HDMI 2.0 front-end (4K passthrough) | regs 00–03 read as vendor/device ID, reg 0x0F bank select, `FF=C3,A5` unlock; programs sub-addresses: `F0=B1` → enables `0x58`, `F1=97` → enables `0x4B` |
| `0x58` | Its HDMI RX block | `4B=D9` → maps EDID SRAM at `0x6C` (0xD8 ÷ 2, bit 0 = enable) |
| `0x4B` | Its TX/router block | `2C..2F = 89 8B 8D 8F` → enables four TX blocks at `0x44–0x47` |
| `0x44–0x47` | Four TX/port blocks | `0x44/0x47` and `0x45/0x46` receive identical config sequences in pairs |
| `0x6C` | Front-end EDID SRAM | receives the 4K EDID (below); bytes 0x7F/0xFF (checksums) are not written — the chip computes them |
| `0x10` | Probably the Lattice ECP3 FPGA (`CLFE3::*`, "FPGA version = 0x%02X") | reg 00 read first, then small config |
| `0x65` | Unknown (possibly `CVX8897`) | only `94=6D` and `18=04 01 20 23 8C`, twice |

The exact front-end part number is not identified (an ITE IT666x-family 1→N
HDMI 2.0 splitter with down-scaler fits the topology — unconfirmed).

### Sequence overview

1. `0E` bridge query; `0x65` setup; PCA9539A reset pulse.
2. ADV7604 map-address setup → **EDID #1** written to ADV EDID RAM, then
   ADI's eval default **EDID #2** (from the ADI reference `HardwareInit`), then
   reset + map setup + EDID #1 again; HPD / termination via repeater map.
3. ADV7604 mode: IO `00=02` (VID_STD), `01=06` (PRIM_MODE = HDMI-graphics,
   V_FREQ 60 Hz), `02=F8/FC` (input colour-space auto, output config),
   `03=80` (output bus format), `05=2C` (incl. AV-code insertion — the SAV/EAV
   codes that become the stream markers), `06=A0`, `15=A0` (pixel/sync
   outputs un-tristated), plus CP picture adjust (`3A–3E`).
4. FPGA (`0x10`) and HDMI 2.0 front-end (`0x2C/0x58/0x4B/0x44–47`) init,
   **EDID #3** (4K) to `0x6C`, TX blocks configured, `14` bridge command.
5. Remainder (from cmd ~2,400 on): the driver's **periodic status polling**,
   see "Runtime polling" below. Replaying it once at startup is a frozen
   snapshot of a loop, not initialisation.

### EDIDs written

| # | Target | Mfg / name | Preferred timing | Notes |
|---|---|---|---|---|
| 1 | ADV7604 (`0x36`) | `AVX` / "AVER GC551", 2018 | 1920×1080p60 (148.5 MHz) | CEA SVDs: *1080p60, 1080p50, 1080i60/50, 720p60/50, 480p, 576p, 480i, 1080p24/25/30, 640×480; audio LPCM 2ch |
| 2 | ADV7604 (`0x36`) | `ATV` / "ADI HDMI TV", 2010 | 720×480p | ADI reference default, immediately overwritten by #1 |
| 3 | Front-end (`0x6C`) | `AVX` / "AVER GC551", serial "5VXF413" | **3840×2160p60 (594 MHz)** | HDMI Forum VSDB (`D8 5D C4`), 4K60 + 1080p60 etc. — this is the EDID the HDMI **source** sees |

**Resolution is not requested — it is advertised.** No explicit timing
registers (1920/1080/2200/1125) are programmed anywhere; the source picks a
mode from the EDID (preferred 1080p60 on the capture side), the ADV7604
auto-detects it in HDMI mode, and the driver reads the result back
(HdmiSyncInfo / CP measurement registers). The `CVX8897::REG_*_To_*` tables
(60p→30p etc.) are not exercised in this capture.

### Runtime polling (driver-verified)

Yes, the Windows driver keeps a side channel busy for its whole lifetime. It
does **not** use a USB interrupt endpoint (none is referenced; the ADV7604's
INT pin is not visible over USB as far as we know). Every status check is a
host-initiated I2C register read/write on the same bridge (bulk OUT 0x01 /
bulk IN 0x01), interleaved with the video stream on IN 0x83. Each chip
class runs its own kernel thread woken by a periodic `KeSetTimerEx`:

| Thread / class | Timer | Per tick |
|---|---|---|
| `CRXADV7604::ThreadProc` (`FUN_140038b40`, started by `StartTimer` `FUN_140037548` from `PowerChipSel`) | first at 500 ms, then **every 200 ms** | AVI InfoFrame read (`FUN_140036438`: InfoFrame map 0xE0–0xE2 + 0x00–0x1B, 31 reads) → `ADIAPI_TvMain` → `ADIAPI_TvIsr` (+ `ADIAPI_TvTask` if ≥20 ms since the last one, so in practice every tick) → reads the `HDCPHandshake` registry setting |
| Front-end class, vtable `0x1400f97a0` (`FUN_140091090` → `FUN_14009167c`) | first at 50 ms, then **every 50 ms** | walks 4 port blocks (`FUN_14008c8dc` ×4 = TX blocks `0x44–0x47`); 0x58 RX status |
| Class with vtable `0x1400fa140` (`FUN_1400920d8` → `FUN_1400930dc`) | first at 500 ms, then every **1000 ms** | not decoded (the constructor stores `0x2C`/`0x58`-like address bytes) |
| Class with vtable `0x1400f44d0` (`FUN_1400728cc` → `FUN_1400740c8`) | first at 500 ms, then every **1000 ms** | not decoded |
| `FUN_140017c70` thread, started when the video pin starts | 1 s wait loop | toggles an in-memory flag only (LED/indicator state); no I/O seen |

There are also two 50 ms periodic timers (`FUN_14002310c`, context
+0x758/+0x798) armed when the video pin starts; what waits on them was not
traced (probably host-side frame pacing, e.g. for the synthesised no-signal
frames — unverified).

**Measured in the sniff** (52 ADV ticks): ~115 I2C transactions per 200 ms
in steady state (≈ **575/s**, each one or two bulk transfers). Per tick on
average: front-end RX `0x58` 34 reads + 4 writes (≈4 polls per ADV tick,
which fits the 50 ms timer), ADV InfoFrame 30, ADV IO 20 reads + 9 writes,
ADV HDMI 9, ADV CP 6.5, `0x2C` 4, TX blocks `0x45/0x46` ~5 each, FPGA `0x10`
~1.

**What the ADV7604 poll checks** — it matches the ADI reference ISR and the
Linux `adv7604.c` detect logic register for register:

| Register(s) | Meaning (ADV7604) |
|---|---|
| IO 0x60 / 0x65 / 0x6A / 0x6F (raw status), clears at 0x62/0x67/0x6C/0x71, 0x76/0x7B/0x80 | HDMI-level raw status + interrupt clears. **0x6F bit 4 = +5V cable detect, port A** ("is something plugged in"); **0x6A = TMDS clock detect / TMDS PLL lock** |
| IO 0x43 / 0x5C, clears at 0x44 / 0x5D | Interrupt status groups 1 and 2 (`NEW_VIDEO_DETECTED`, `NEW_5V_DETECTED`, … events seen as driver strings) |
| IO 0x12 bit 0, CP 0xB5 (`&0xD0`), CP 0xB1 bit 7 | CP lock / SSPD lock / STDI valid: Linux's `no_lock_cp` / `no_lock_sspd` / `no_lock_stdi`. Together these are the "**is there a usable signal**" test |
| CP 0xB1–0xB4 (n=2 reads) | STDI block length / lines per field: the **resolution and frame-rate measurement** (`CheckTheFrameRate Blines`) |
| HDMI 0x00, 0x04, 0x05, 0x18, 0x5B–0x5D, 0x36 (n=5) | HDMI port / TMDS / mode status, measured timing and TMDS frequency |
| InfoFrame map 0x00–0x1B, 0xE0–0xE2 | AVI InfoFrame (colour space, VIC) checked for changes |

On a detected change, `ADIAPI_TvTask` reconfigures the receiver ("The Rx has
been reconfigured for the detected Resolution", `AssertHpdAndTerm`,
`ConfigureAAFilters`, `ConfigureDriveStrength`) and the front-end thread
handles the passthrough ports.

**Userspace status poll.** `UsbStream` now runs a read-only 200 ms poll
(`statusPollLoop`, GC551 only, disable with `LGX2_NO_STATUS_POLL`). It reads IO
0x0C/0x12/0x6A/0x6F, CP 0xB1, HDMI 0x05–0x0C, 0x20–0x25, 0x2A–0x35 and 0x3B
(9 bridge reads, block reads ≤ 13 bytes), decodes them with `SignalStatus`
(Linux `no_signal()` + `query_dv_timings` logic) and prints a line when the
cable, signal or timing changes. It writes nothing, so it cannot clear
interrupts or reconfigure anything — it observes only. First hardware
testing: command reads issued right after bootstrap time out, and video
takes ~3–5 s to start flowing properly, so the first poll was briefly delayed
10 s. With a 100 ms per-transfer timeout every poll still timed out after
that delay. The real cause was the truncated bootstrap capture (below), and
the delay was removed once that was fixed.

**How the driver sends runtime commands (driver-verified).** The bridge
command dispatcher `FUN_1400199cc` accepts opcodes
`01–10, 14, 15, 16, 29, 2A`. Under a mutex it does a synchronous bulk OUT of
the request on one pipe and then a synchronous bulk IN of the reply on
another, both via `FUN_14001720c` (URB function 0x09, bulk/interrupt). That
is the same request format and endpoints as the bootstrap, so there is no
separate runtime channel and no vendor control requests. Each transfer is
submitted through `FUN_1400157c4`, which waits **up to 5 s**
(`KeWaitForSingleObject` timeout −50,000,000 × 100 ns) before cancelling.
Error recovery aborts the pipe (URB function 0x02, `FUN_14001458c`). The
userspace poll now uses the same 5 s timeout and reports how long each poll
took.

**Truncated bootstrap capture (hardware-verified).** The LGX2 command string
ends with a read request (`>02220102A8`, 2 bytes from CP 0xA8) and no `<2`
after it, because the sniff was cut off mid-transaction. Its 2-byte reply
stays queued in IN 0x81, and the next command read gets it instead of its
own answer. A 1-byte read then failed with `LIBUSB_ERROR_OVERFLOW` (seen on
hardware). On Linux this probably left the endpoint unusable, which matches
every later poll timing out. Fix: drain IN 0x81 after the bootstrap replay,
and always receive command replies into a full 1024-byte (wMaxPacketSize)
buffer and check the length in software. The GC550 string ends with a proper
`<2` read.

**Verified on hardware after the fix:** the read before streaming succeeds
("cable connected, no signal", 6 ms). Mid-stream polls also succeed, with a
**full 9-read poll taking ~6 ms while video streams** on IN 0x83, and they
report `1920x1080p @ 59.90 Hz (148.25 MHz, HDMI)`. So the bridge answers
promptly while streaming. The earlier timeouts came from the stale reply,
not from streaming. (59.90 rather than 60.00 Hz is the TMDS frequency
register's 0.25 MHz resolution: 148.25 MHz / (2200 × 1125) = 59.90.) Source
unplug/replug is detected and reported correctly. The poll now starts
immediately when streaming starts, with no delay.

**Other bridge opcodes (driver-verified senders).** `0E a b c Lh Ll`
reads `Lh·256+Ll` bytes from a memory address. The bootstrap's
`0E 02 07 FE 02 00` reads 512 bytes, probably board/config EEPROM (the
driver later tests capability bits in a 0x20-byte config block). `0D` is
the matching write, and `15`/`16` are single-byte variants of `0D`/`0E`.
`05`/`06`/`07` (GC550 only) are a 1-byte-prefixed write / read / write with
8 ms stalls. `09 x` and `0B a b c` are short writes. `08` and `14` take no
arguments and are reached only through a virtual method table, so their
meaning is not yet traced.

**Detecting an already-bootstrapped board (userspace).** The configuration
survives a USB reset (including the `libusb_reset_device` at shutdown) and is
only lost on unplug. Re-running the bootstrap on a configured board is what
produced garbage output. The bootstrap sets the PCA9539A (0x74)
configuration registers 0x06/0x07 to `00 09`. Their power-on default is
`FF FF` (all inputs), and the expander is bus-powered, so a USB reset leaves
it alone. `UsbStream::detectLgx2BoardState` reads them before bootstrapping:
- `FF FF` means fresh, so run the bootstrap.
- `00 09` plus ADV7604 IO 0xFA (EDID map address) = `0x6C` means already
  configured. Skip the bootstrap, skip the frame-boundary probe, and let the
  FrameBuilder re-sync mid-stream.
- Anything else, or a failed read, falls back to the old IN 0x83 data check
  and its error message.

**Hardware result: reuse works intermittently.** In one test the first reuse
after a clean session gave good video. The next reuse gave **audio but no
video**, and neither an HDMI replug nor an app restart fixed it (a restart
takes the reuse path again). Two dumps of that state show every byte is
present, in the wrong order.

### Swapped DMA buffer order (hardware-verified)

The stream arrives as **16 KiB FX3 DMA buffers**. A frame period at 1080p is
exactly **254 buffers** (4,161,536 bytes):
- SAV (8 bytes, always at the start of a buffer)
- 4,147,200 bytes of video
- EAV (9 bytes, padded to 12)
- the 3,200-byte audio packet
- **11,108 bytes of `AA 55 55 AA` filler up to the buffer boundary**

That last item corrects an earlier belief: `AA 55 55 AA` is padding after the
audio payload, not an audio end marker.

In the bad state, **every pair of buffers arrives swapped**, on a 32 KiB grid.
One likely mechanism: the buffers alternate between two producers
(ping-pong), and a buffer lost when a previous session stopped flips the
consumer's phase. That fits the state persisting until power-off, and only
some sessions ending in it. To the parser it looks like this:

```
SAV(N)                   first buffer of a swapped pair
  18,432 B               = 16,376 B of frame N's start + 2,056 B of frame N-1's end
EAV(N-1) + audio + filler
  4,128,768 B            frame N, still pair-swapped
SAV(N+1) …
```

Every SAV→EAV span is 18,432 bytes, so the size gate rejects every frame
while audio still parses (audio-only). EAV c0 ≠ SAV b0 because the EAV
belongs to the previous frame. The EAV header's c2..c3 = `04 38` is 1080
lines. The frame counter (b0/c0) runs `… fe ff 01 …` and skips `00`.

Evidence:
- An image-smoothness search over reorderings picked "swap 16 KiB pairs":
  mean vertical luma step 0.79 vs 3.60 as delivered, with no other candidate
  close.
- Un-swapping every pair on a grid aligned to the stream start turns all 63
  SAV→EAV spans into exactly 4,147,200 bytes with c0 == b0, and the frames
  render cleanly.
- A first attempt that reassembled frames around the "late" EAV instead
  produced ghosted, striped images (two offset copies). It was reverted.

No "resync" command is known; `08`/`14` are still unidentified.

**Fix: `utils::BufferReorderer`, in front of the FrameBuilder.**
- It scans the raw stream for SAVs. A checksum-valid EAV for an earlier frame
  (c0 ≠ b0) within 32 KiB after a SAV means "swapped, pair grid anchored at
  this SAV"; none means normal.
- In swapped mode it emits each 32 KiB pair second-half-first; normal mode
  passes data through.
- It holds back about 32 KiB so each SAV's verdict is known before its bytes
  are emitted, and drops data before the first decided SAV.
- A gap re-detects the order. Once an order is established, changing it needs
  two consecutive agreeing SAVs, so a SAV-like pattern inside audio data
  cannot flip it. Output after any change is flagged gapped so the
  FrameBuilder re-syncs.
- It assumes an even number of buffers per frame period (true at 1080p);
  other resolutions are untested.

Verified offline:
- The swapped dump through BufferReorderer + FrameBuilder gives 61/61 frames
  byte-identical to the independently un-swapped stream. The first frames
  are lost while the order is decided; the capture's first SAV looks normal.
- The normal-order stream gives 63/63 identical, so it passes through
  unchanged.
- Unit tests cover normal, swapped, a gap, a mid-stream change, the counter
  wrap, a fake SAV in audio, and a swapped 1080p stream through the
  FrameBuilder.

Not yet confirmed on hardware with a live cold-start (normal) stream.

**Endpoints (device descriptor + driver pipe mapping).** The GC551 has one
configuration with one vendor-specific interface and six bulk endpoints, in
this pipe order: 0 = `0x01` OUT, 1 = `0x81` IN, 2 = `0x02` OUT, 3 = `0x82` IN
(bMaxBurst 15), 4 = `0x03` OUT, 5 = `0x83` IN (bMaxBurst 15). On a
single-interface device the driver's channel setup (`FUN_1400196e0`) sets
command OUT = pipe 0 (`0x01`), command IN = pipe 1 (`0x81`) and stream =
pipe 5 (`0x83`). So runtime commands use the **same endpoint pair as the
bootstrap**. The EP2 pair is not used by this path (another branch, for
devices with a second interface, uses pipes 0/3).

**Implication for userspace.** `UsbStream::streamSetupCommands` replays the
whole capture once, polling included, and then sends nothing. That is enough
for a source that is already connected and stays at one mode (the tested
case). With no loop running, nothing re-asserts HPD or reconfigures the
ADV7604 or front-end on hot-plug or a mode change. That *may* explain
hot-plug and "already bootstrapped" failures (untested). Replicating it
needs a command channel that is safe to use while the IN 0x83 stream is
running, plus a ~200 ms poll that reads at minimum IO 0x6F, IO 0x6A, IO 0x12,
CP 0xB1/0xB5 and the HDMI timing registers.

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
