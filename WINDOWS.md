# Lgx2userspace Driver on Windows
How did we end up here?

If you're here because you can't get the official driver working on Windows 11, [there are steps here explaining how you can get the driver to work](https://youtu.be/pi68dlDiizQ). Shout out to [Mistatcx and their Reddit post](https://www.reddit.com/r/AverMedia/comments/15w84ot/comment/jx1h70s/?context=3).

It's possible (though not recommended) to run the lgx2userspace driver on Windows, however for it to work correctly
some third-party software needs to be used.

**Please note, this is almost certainly not going to be a great experience compared to the official driver, but it's an option.**

## Building
The following steps describe cross-compiling from Linux for Windows.

If you have a C++ IDE that understands CMake files (e.g. CLion), then it should be as simple as importing the project
and building the lgx2userspace target.

### Cross-compiling from Linux

You will need the MinGW toolchain and the MinGW-packaged versions of the dependencies. On Fedora:

```bash
sudo dnf install mingw64-gcc-c++ mingw64-SDL3-devel mingw64-libusb1-devel
```

On Debian/Ubuntu:

```bash
sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 mingw-w64-x86-64-dev
# SDL3 and libusb may need to be built from source or sourced from another package repo
```

With the toolchain and dependencies installed, build with:

```bash
mkdir winbuild && cd winbuild
PKG_CONFIG_LIBDIR=/usr/x86_64-w64-mingw32/sys-root/mingw/lib/pkgconfig \
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../TC-Mingw.cmake ..
make
```

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
The project uses CMake and a C++ toolchain to build the `lgx2userspace.exe` binary. Dependencies are managed via [MSYS2](https://www.msys2.org/).

Install MSYS2, then open an MSYS2 MinGW64 shell and install the required packages:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-SDL3 mingw-w64-x86_64-libusb \
          mingw-w64-x86_64-pkg-config
```

CMake can also be downloaded separately from the [CMake Github Releases page](https://github.com/Kitware/CMake/releases) if preferred.

### Building
From an MSYS2 MinGW64 shell, clone the project and build:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
```





## Other info
The icon for the app was made using the [Android asset studio](https://romannurik.github.io/AndroidAssetStudio/icons-launcher.html).
