# LGX2 Userspace driver
This software is little more than a POC and no guarantees of functionality are given and it may even be dangerous to use this software. Please
consider contacting AverMedia for a supported Linux driver.

This project contains a userspace driver for the [AverMedia LGX2 (GC551)](https://avermedia.com/LGX2).

It can be used to display the captured video and audio in a standalone window or
to forward the captured video and audio to a virtual video capture device.

## Building
To build the project, you will need:
* CMake
* Libusb
* SDL2
* V4L2Loopback

Execute the following commands to build in the root of the project:

```bash
mkdir build
cd build
cmake ..
make
```

## Setup
The userspace driver will require read and write access to the LGX2. On a Linux
system, this can be granted to the user by adding the following rule to Udev.

```bash
sudo echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="07ca", ATTRS{idProduct}=="0551", MODE="0666"' > /etc/udev/rules.d/999-avermedia.rules
sudo udevadm control --reload-rules
```

After the rule has been added, unplug and re-plug in the LGX2.

## Running
Once udev has been configured to grant read and write permission to the device it
will be possible to run the application by executing `lgx2userspace`.

The application will take a few seconds to run as it streams setup information to the device, 
and it will eventually display a window that will then start to display captured frames.

In my limited testing, certain devices may need to be re-plugged in after the application has 
started. For example, a Nintendo Switch will not recognise the LGX2 as an output source until
it is undocked and re-docked.

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
