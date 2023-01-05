# Lgx2userspace Driver on Windows
How did we end up here?

It's possible (though not recommended) to run the lgx2userspace driver on Windows, however for it to work correctly
some third-party software needs to be used.

**Please note, this is almost certainly not going to be a great experience compared to the official driver, but it's an option.**

## Building
The following steps describe cross-compiling from Linux for Windows.

If you have a C++ IDE that understands CMake files (e.g. CLion), then it should be as simple as importing the project
and building the lgx2userspace target.

If you are building from Linux however...

The Windows variant can be built once you have installed MingW32 on your machine and also Conan (via `pip install conan`).

Then, if you are lucky the following commands should _just work_.

```bash
mkdir winbuild
cd winbuild
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../TC-Mingw.cmake ..
# Maybe invoke cmake 2-3 times
make
```

If you get errors the first time, try invoking cmake again. The CMakeLists are written by a complete newcomer so there
appears to be some issues when starting fresh.

The build process does involve downloading libusb and SDL2 from Conan compatible repositories and also compilation of
libusb and SDL2, so it can take a few minutes depending on your machine.

Once the build is complete, the lgx2userspace.exe file can be used.

## Configuring a USB driver
In order for the userspace driver to work, a proxy USB driver must be associated with the target device.

The simplest way to achieve this is to use a piece of software called [Zadig](https://zadig.akeo.ie/).

Zadig allows you to generate and install a USB driver for a given USB Vendor ID (**07ca**) and Product ID (**4710** and **0551** for LGX and LGX2 respectively).

Download and install Zadig and open it up.

In the row marked 'USB ID', in the first box enter **07ca** and **4710** in the second box if you are using the LGX and **0551** if you are using the LGX2.

Then tap 'Install driver'. This can take a minute or two.

Upon success, unplug the LGX or LGX2 if they were plugged in, then plug the device back in.

Go to Device manager and inspect the LGX/LGX2 device. 

If the device is using the official driver, you will need to right click and select 'Update driver'.

On the window that opens, select 'Browse my computer for drivers'.

Finally select 'Let me pick from a list of available drivers on my computer' which should reveal the official driver and
the one you have just created.

Select the driver you have just created and tap next and finish.

You are now ready to use the lgx2userspace driver.

## Building on a Windows machine
### Prerequisites
The project uses CMake, Conan and a C++ toolchain to build the `lgx2userspace.exe` binary.

As Conan is a python project, you will need to install python, which can be downloaded from [Python Releases for Windows](https://www.python.org/downloads/windows).

CMake can be downloaded from the [CMake Github Releases page](https://github.com/Kitware/CMake/releases).

You will need gcc and g++ in order to build the project, which can be installed as part of the [msys2 toolset](https://www.msys2.org/).

### Building
With CMake and Conan installed, building _should_ be as simple as cloning the project and navigating to it using cmd.exe:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**NOTE: This will download and build all the dependencies and will take a while depending on your machine.**





## Other info
The icon for the app was made using the [Android asset studio](https://romannurik.github.io/AndroidAssetStudio/icons-launcher.html).
