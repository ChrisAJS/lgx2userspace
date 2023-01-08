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

## Building
To build the project, you will need:
* CMake
* Libusb
* Libpcap
* SDL2
* V4L2Loopback

### Ubuntu 22.04
The following packages need to be installed:

```bash
sudo apt install cmake libusb-dev libsdl2-dev libsdl2-gfx-dev libpulse-dev v4l2loopback-dkms v4l2loopback-utils
```
### Arch Linux (Unconfirmed)
The following packages need to be installed:

```bash
sudo pacman -S cmake libusb sdl2 sdl2_gfx libpulse v4l2loopback-dkms v4l2loopback-utils
```

### Build Command
With the dependencies installed for your environment it should be possible to build the application.

Execute the following commands to build in the root of the project:

```bash
mkdir build && cd build
cmake ..
make
```

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
will be possible to run the application by executing `lgx2userspace`.

If you are using the LGX GC550, use the command line option `x` - `./lgx2userspace -x`.

The application will take a few seconds to run as it streams setup information to the device, 
and it will eventually display a window that will then start to display captured frames.

In my limited testing, certain devices may need to be re-plugged in after the application has 
started. For example, a Nintendo Switch will not recognise the LGX/LGX2 as an output source until
it is undocked and re-docked.

### Common issues
On occasion the LGX or LGX2 will fail to retrieve full frames. It may be required to unplug and re-plug the HDMI cable
into the input of the device.

If you attempt to run the application and it exits immediately, look at the error output, it may help diagnose the issue,
which will likely be that you haven't installed the udev rules required to give your user permission to access the LGX or
LGX2 without root access.

### Options when running
When using the default SDL2 renderer for video output, it is possible to toggle fullscreen
by pressing `F` and to exit fullscreen by pressing `G`.

### Gathering diagnostic information
To help diagnose problems that may be fixed in the future, when submitting an issue
please try to give as much information about your setup as possible and in the case of
inadequate video or audio output, run the userspace driver with the `-v` switch enabled.

With the `-v` switch enabled, the driver will output information regarding time taken to
process frame data and time taken to render both audio and video.

This information could be valuable when identifying issues so please try your best to include it
in any issues you raise, thank you!

## Running with V4L2 Output and Pulseaudio Output
### V4L2 Output setup
To output video to a virtual webcam output source, load the V4L2 Loopback Linux module with an easy to identify device
number:

```bash
sudo modprobe v4l2loopback video_nr=99 exclusive_caps=1 card_label="LGX2"
```

You should now see `/dev/video99` exists:

```bash
ls /dev/video99
/dev/video99
```

### Pulseaudio Setup
Pulseaudio can be configured by issuing the following commands:
```bash
pactl load-module module-null-sink sink_name=lgx2 sink_properties=device.description=LGX2
pactl load-module module-remap-source master=lgx2.monitor source_name=lgx2 source_properties=device.description=LGX2Audio
```
This will create an audio sink called `LGX2 Audio Sink` which can be added to OBS as a Pulseaudio output capture device.

**NOTE: Even if you are using the LGX GC550- leaving the names as lgx2 is required and will not affect behaviour.**
**NOTE: The best way to control the audio volume is to use the gain OBS filter whilst leaving the sink volume at max.**

### Running with configure V4L2 device
Run the userspace driver with the `-d` option to specify which V4L2Loopback device to use:

```bash
./lgx2userspace -d /dev/video99
```

To run using the Pulseaudio sink that you created, use the `-a` switch with the name of the sink:

```bash
./lgx2userspace -a lgx2
```

**NOTE: You may need to unplug and replug in your video source.**

Go to OBS or other streaming software and select the LGX2 V4L2 source. You should now see video from the input device being output.

## Demo
See it in action over at [YouTube](https://www.youtube.com/watch?v=-yzHMbUn-w0).

# Getting in touch
If you need help or want to have a discussion on proposed features or usability, join the [Discord server](https://discord.gg/bSVZBRjb).

# Attributions
This project uses the hard work of the following projects:

 * [libusb](https://libusb.info/)
 * [V4L2Loopback](https://github.com/umlaeute/v4l2loopback)
 * [SDL](https://www.libsdl.org/)
 * [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/)
 * [Catch2](https://github.com/catchorg/Catch2)
 * [GLFW](https://www.glfw.org/)
