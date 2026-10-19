# LGX2 Userspace driver (with LGX support)
**Before continuing! This project aims to provide support for the LGX and LGX2 USB capture cards. The software is not extensively tested and the effects it has on the capture cards is also not fully understood. If you use the LGX and LGX2 on Windows using the official driver, using this userspace driver may prevent such a set up to continue working.**

**Please please please understand that you are risking your LGX or LGX2 by using this software.**

This software is little more than a POC and no guarantees of functionality are given and it may even be dangerous to use this software. Please
consider contacting AverMedia for a supported Linux driver.

This project contains a userspace driver for the [AverMedia LGX2 (GC551)](https://avermedia.com/LGX2) as well support for the [AverMedia LGX (GC550)](https://avermedia.com/LGX).

It can be used to display the captured video and audio in a standalone window or
to forward the captured video and audio to a virtual video capture device.

There is even an incredibly unnecessary Windows build. Instructions on how to use this can be found in [WINDOWS.md](WINDOWS.md). Spoiler, it's not recommended. 

## LGX (GC550) Issues
The LGX (GC550) will not be correctly configured by the official Windows driver after using this userspace driver on Linux (or the Windows build).

If you intend on using this userspace driver for your LGX, please understand that you will no longer be able to use the official driver to use the
device.

To enable LGX (GC550) support, add `-DENABLE_LGX_GC550_SUPPORT=ON` to the `cmake` invocation.

**It cannot be stressed enough, that this currently will make your LGX (GC550) unusable on Windows using the official driver.**

*Update (untested on Windows):* the cause appears to have been found and removed. Older versions replayed a recorded
login to the card's anti-clone chip, which always fails and permanently locks the chip after a few runs; the official
driver then rejects the card. The bootstrap no longer sends it. On a GC550, every run prints the chip's attempt counters
(`Anti-clone chip attempt counters ...`): `FF` means untouched, `00` means locked. A pristine card stays at `FF` across
runs, but whether it still works with the official driver afterwards has not been confirmed yet, so the warning above
stands until it has. See PROTOCOL.md, "GC550 (LGX) bootstrap".

## Building
To build the project, you will need:
* CMake
* libusb
* SDL3
* V4L2Loopback (at run time, for `lgx2d` and the viewer's `-d` option; see *Setting up a V4L2 loopback device*)
* PipeWire development files (optional: audio output from `lgx2d`)

### Ubuntu 24.04+
The following packages need to be installed:

```bash
sudo apt install cmake libusb-1.0-0-dev libsdl3-dev v4l2loopback-dkms v4l2loopback-utils libpipewire-0.3-dev
```
### Arch Linux
The following packages need to be installed:

```bash
sudo pacman -S cmake libusb sdl3 v4l2loopback-dkms v4l2loopback-utils pipewire
```

### Fedora
The following packages need to be installed:

```bash
sudo dnf install cmake libusb1-devel SDL3-devel pipewire-devel
sudo dnf install akmod-v4l2loopback v4l2loopback  # from RPM Fusion
```

### Build Command
With the dependencies installed for your environment it should be possible to build the application.

Execute the following commands to build in the root of the project:

```bash
cmake -S . -B build
cmake --build build
```

### Build options

| Option | Default | Description |
|---|---|---|
| `ENABLE_LGX_GC550_SUPPORT` | `OFF` | Compile in support for the LGX (GC550). **Warning:** using this driver with a GC550 will prevent the official Windows driver from working on that device. See the LGX (GC550) Issues section. |
| `LGX2_VERBOSE_STATS` | `OFF` | Print per-second frame rate and frame size statistics to stdout. Useful for diagnosing capture performance issues. |

Pass options to cmake with `-D`, for example:

```bash
cmake -S . -B build -DLGX2_VERBOSE_STATS=ON
cmake --build build
```

### Runtime environment variables

| Variable | Description |
|---|---|
| `LGX2_NO_STATUS_POLL` | Set to any value to disable the HDMI input status poll. By default the card (LGX2 or LGX) is polled every 50 ms and changes are printed, e.g. `HDMI input: 1920x1080p @ 60.00 Hz (148.50 MHz, HDMI, YCbCr 4:2:2; receiver input colour space auto)` or `HDMI input: no cable`. The poll also mutes audio and blanks video while there is no signal, or while the source sends HDCP-protected video (reported as `HDMI input: HDCP-protected source (not captured)`; protected video is never captured, matching the official driver); without it, nothing is muted. |
| `LGX2_FRONTEND_DEBUG` | LGX2 (GC551) only, diagnostic: also print the HDMI 2.0 front-end's (IT6664) status registers whenever they change. Read only. See PROTOCOL.md, "IT6664 front-end and HDCP (GC551)". |
| `LGX2_DUMP_FILE` | Path to write the first 128 raw USB chunks to, for offline stream analysis. |

## Setup
The userspace driver will require read and write access to the LGX/LGX2. 

On a Linux system, this can be granted to the user by adding the following rules to Udev.

```bash
sudo cp 999-avermedia.rules /etc/udev/rules.d/999-avermedia.rules
sudo udevadm control --reload-rules
```

After the rule has been added, unplug and re-plug in the LGX/LGX2.

## Running
Once udev has been configured to grant read and write permission to the device it
will be possible to run the application by executing `lgx2userspace`. On Linux this views the
output of the capture daemon by default (see *Watching lgx2d's output* below); add `-D` to
capture from the device directly.

If you are using the LGX GC550, use the command line option `x` - `./lgx2userspace -x`.

The application will take a few seconds to run as it streams setup information to the device, 
and it will eventually display a window that will then start to display captured frames.

In my limited testing, certain devices may need to be re-plugged in after the application has 
started. For example, a Nintendo Switch will not recognise the LGX/LGX2 as an output source until
it is undocked and re-docked.

### Common issues
On occasion the LGX or LGX2 will fail to retrieve full frames. It may be required to unplug and re-plug the HDMI cable
into the input of the device.

**HDCP-protected sources are not captured.** Some sources encrypt their output with HDCP; a MacBook does this even for
its desktop. As with the official driver, such video is never captured: you will see an "HDCP PROTECTED" screen and
`HDMI input: HDCP-protected source (not captured)`. On the LGX2 (GC551), its HDMI front-end then stays locked in this
state even after you connect a different source: **unplug and replug the LGX2's USB cable** before using another source.
(The LGX (GC550) has no such front-end and should recover as soon as the protected source is disconnected.)

If you attempt to run the application and it exits immediately, look at the error output, it may help diagnose the issue,
which will likely be that you haven't installed the udev rules required to give your user permission to access the LGX or
LGX2 without root access.

### Options when running
When using the default SDL renderer for video output, it is possible to toggle fullscreen
by pressing `F` and to exit fullscreen by pressing `G`.

### Gathering diagnostic information
To help diagnose problems that may be fixed in the future, when submitting an issue
please try to give as much information about your setup as possible and in the case of
inadequate video or audio output, run the userspace driver with the `-v` switch enabled.

With the `-v` switch enabled, the driver will output information regarding time taken to
process frame data and time taken to render both audio and video.

This information could be valuable when identifying issues so please try your best to include it
in any issues you raise, thank you!

## Setting up a V4L2 loopback device
Both `lgx2d` (the capture daemon) and the viewer's `-d` option output video through a
[v4l2loopback](https://github.com/v4l2loopback/v4l2loopback) device: a virtual webcam that OBS,
browsers and other software can open.

### Installing v4l2loopback
v4l2loopback is a kernel module, built for your kernel by the package:

* Ubuntu: `sudo apt install v4l2loopback-dkms v4l2loopback-utils`
* Arch Linux: `sudo pacman -S v4l2loopback-dkms v4l2loopback-utils`
* Fedora: `sudo dnf install akmod-v4l2loopback v4l2loopback` (from
  [RPM Fusion](https://rpmfusion.org/Configuration); the module is built on first install, which
  takes a minute; with Secure Boot enabled the module must be signed, which the akmod normally
  handles)

### Creating the device
If the module is not loaded yet, load it with an easy to identify device number:

```bash
sudo modprobe v4l2loopback video_nr=99 exclusive_caps=1 card_label="LGX2"
```

`exclusive_caps=1` makes the device look like an ordinary webcam to OBS and browsers.

**If the module is already loaded** (OBS loads it for its Virtual Camera, for example), `modprobe`
silently ignores these options and no new device appears. Add a device to the running module
instead:

```bash
sudo v4l2loopback-ctl add -n LGX2 -x 1 /dev/video99
```

Check it exists (`v4l2-ctl --list-devices` also shows its name):

```bash
ls /dev/video99
```

### Keeping the device across reboots
Devices created as above disappear on reboot. To create one at boot, have the module load with
your options, for example in `/etc/modules-load.d/v4l2loopback.conf`:

```
v4l2loopback
```

and in `/etc/modprobe.d/v4l2loopback.conf`:

```
options v4l2loopback video_nr=99 exclusive_caps=1 card_label="LGX2"
```

If you also use OBS's Virtual Camera, create one device for each, so OBS does not load the
module itself with only its own device:

```
options v4l2loopback devices=2 video_nr=99,98 exclusive_caps=1,1 card_label="LGX2,OBS Virtual Camera"
```

## Running the capture daemon (lgx2d)
`lgx2d` captures without a window, for use by OBS and other software. It waits for the LGX2 to be
plugged in, bootstraps it, captures until it is unplugged, and then waits again, so it can be
left running. While there is no picture it keeps sending a card saying why ("NO CAPTURE DEVICE",
"NO HDMI CABLE", "NO SIGNAL"), so the video device never stops producing frames.

With a loopback device set up (see above):

```bash
./src/lgx2d/lgx2d -d /dev/video99
```

* **Video:** in OBS add a *Video Capture Device (V4L2)* source and select **LGX2**.
* **Audio** is published as a PipeWire source called **LGX2 Audio**: in OBS add an *Audio Input
  Capture (PipeWire)* source and select it. Several applications can record from it at once. It
  is muted while there is no signal. Pass `-A` to disable audio; building without the PipeWire
  development files leaves it out entirely.
* `-p` disables the HDMI status poll (and with it muting and the no-signal cards). Stop with
  Ctrl+C.

**Restarting lgx2d:** the video device persists, but each run of `lgx2d` creates a new
**LGX2 Audio** node. Applications that were recording from the previous one (OBS included) may
not reattach to the new one by themselves; if audio stays silent after a restart, reselect
**LGX2 Audio** in the application (in OBS, open the source's properties and choose it again).

While capturing, `lgx2d` uses roughly 10% of one CPU core for 1080p; with the LGX2 attached but
no HDMI signal about 1%, and with no LGX2 attached well under 1%.

## Watching lgx2d's output
On Linux, `lgx2userspace` on its own is a viewer for `lgx2d`: it shows the video from the
**LGX2** loopback device and plays **LGX2 Audio**, without touching the LGX2 itself.

v4l2loopback (0.15 and later) lets only one application at a time capture video from a device,
so the viewer and OBS cannot both show the video at once: whichever starts capturing second
fails with "Device or resource busy". Audio has no such limit.

```bash
./lgx2userspace
```

* If no device named **LGX2** is offering video, it exits and asks whether `lgx2d` is running.
  Use `-i /dev/video99` to name the device yourself.
* `-D` captures from the LGX2 directly instead (as on other platforms), without `lgx2d`; stop
  `lgx2d` first, since only one program can own the LGX2. `-d`, `-f` and `-x` imply `-D`.

## Running the viewer with V4L2 output
Without `lgx2d`, run the userspace driver with the `-d` option to specify which V4L2Loopback device to use
(see *Setting up a V4L2 loopback device* above):

```bash
./lgx2userspace -d /dev/video99
```

**NOTE: You may need to unplug and replug in your video source.**

Go to OBS or other streaming software and select the LGX2 V4L2 source. You should now see video from the input device being output.

## Demo
See it in action over at [YouTube](https://www.youtube.com/watch?v=-yzHMbUn-w0).

## Protocol documentation
What is known (and unknown) about the USB bulk-transfer protocol the device uses
to stream video and audio is documented in [PROTOCOL.md](PROTOCOL.md). This
covers marker values, sub-chunk structure, frame assembly, audio packet layout,
and known video source compatibility issues (MacBook colorspace, camera HDMI).

# Attributions
This project uses the hard work of the following projects:

 * [libusb](https://libusb.info/)
 * [V4L2Loopback](https://github.com/umlaeute/v4l2loopback)
 * [SDL3](https://www.libsdl.org/)
 * [Catch2](https://github.com/catchorg/Catch2)
