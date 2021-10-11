# LGX2 Userspace driver
This software is little more than a POC and no guarantees of functionality are given and it may even be dangerous to use this software. Please
consider contacting AverMedia for a supported Linux driver.

This project contains a userspace driver for the [AverMedia LGX2 (GC551)](https://avermedia.com/LGX2).

It can be used to display the captured video and audio in a standalone window or
to forward the captured video and audio to a virtual video capture device.

Currently, only supporting 1920x1080@30-60fps.

**NOTE: Currently only 1920x1080@60fps is supported though capture is at 1920x1080@30fps**

## 60fps output vs 30fps output
The userspace driver will **very** occasionally get into a mode of synchronisation with the LGX2 whereby 60fps output
can be captured and displayed at 60fps.

The working theory is that the LGX2 has 2 buffers it fills as it captures a single frame of data.

As one buffer is filled, the other starts filling. Meaning when the first part of a frame is retrieved, the second part
will be available after another 8ms or so.

Reading the buffers, processing the video and audio and then retrieving the next frame all within 16.7ms to get 60fps output
is challenging, even for a 11th Gen Intel Core i9.

### Potential mitigations / investigations
#### Buffering frames
If on average, frames come in at 60fps, adopting an approach similar to HTTP live streaming may mean that consisent
60fps capture could be possible- whereby several frames are captured before output begins. This is at the cost of latency
which is a very nice thing to **not** have, but latency can be mitigated by using the HDMI pass-thru.

#### Reading frames from a high-priority thread
Right now the architecture of the userspace driver means the following flow happens in sequence:
* Read retrieved data from URBs
* Re-submit URBs for reading frame data
* Process latest frame data
  * Extract video data
  * Extract audio data
* Render frame
* Render audio
* _Repeat_

In reality, the first packet of frame data will  

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

The application will take a few seconds to run as it streams setup information to the device
an it will eventually display a window that will then start to display capture information.

In my limited testing, certain devices may need to be re-plugged in after the application has 
started. For example, a Nintendo Switch will not recognise the LGX2 as an output source until
it is undocked and re-docked.

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
pactl load-module module-null-sink sink_name=lgx2 sink_properties=device.description="LGX2 Audio Sink"
pactl load-module module-remap-source master=lgx2.monitor source_name=lgx2 source_properties=device.description="LGX2 Audio"
```
This will create an audio sink called `LGX2 Audio Sink` which can be added to OBS as a Pulseaudio output capture device.

**NOTE: The best way to control the audio volume is to use the gain OBS filter whilst leaving the sink volume at max.**

### Running with configure V4L2 device
Simply run the userspace driver with the V4L2 device:

```bash
./lgx2userspace /dev/video99
```

**NOTE: You may need to unplug and replug in your video source.**

Go to OBS or other streaming software and select the LGX2 V4L2 source. You should now see video from the input device being output.

## Demo
See it in action over at [YouTube](https://www.youtube.com/watch?v=-yzHMbUn-w0).
