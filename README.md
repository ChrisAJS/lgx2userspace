# LGX2 Userspace driver
This software is little more than a POC and no guarantees of functionality are given and it may even be dangerous to use this software. Please
consider contacting AverMedia for a supported Linux driver.

This project contains a userspace driver for the [AverMedia LGX2 (GC551)](https://avermedia.com/LGX2).

It can be used to display the captured video and audio in a standalone window or
to forward the captured video and audio to a virtual video capture device.

**NOTE: Currently only 1920x1080@60fps is supported though capture is at 1920x1080@30fps**

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

