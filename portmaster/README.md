## Notes

Thanks to [pablocpas](https://github.com/pablocpas/OpenCMR2) for OpenCMR2, a native port of the decompiled Colin McRae Rally 2.0 that runs the original stages, cars and menus with SDL and OpenGL.

The game files are not included. Copy the folders `Game`, `FrontEnd`, `Sounds`, `CountrySpecific` and `SetupRep` from your own copy of Colin McRae Rally 2.0 for PC to `ports/opencmr2/gamedata/`. The menus, text and co-driver follow the edition you copy. For a Polish edition set `language = Polish` and `sku = POLAND` in the `[install]` section of `ports/opencmr2/conf/opencmr2.ini`.

## Controls

| Key | Action |
|--|--|
| D-Pad | Move in menus, steer |
| Left Stick | Steer |
| A | Confirm / Handbrake |
| B | Back / Look back |
| Y | Change camera |
| L1 | Gear down |
| R1 | Gear up |
| L2 | Brake |
| R2 | Accelerate |
| Start | Pause and resume |
| Start + Select | Quit the game |

## Compile

```bash
git clone https://github.com/pablocpas/OpenCMR2.git
cd OpenCMR2
git checkout 6e201df71a1c347b2101bb11f54ddc422756cc52
git am ../patches/*.patch
```

The build uses an Ubuntu 20.04 arm64 root filesystem as sysroot (glibc 2.31), a host `spirv-cross` and `clang`.

```bash
docker run --platform linux/arm64 --name r36s-root ubuntu:20.04 bash -c "apt-get update && apt-get install -y libc6-dev libstdc++-10-dev libgcc-10-dev libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libudev-dev libasound2-dev libdbus-1-dev"
mkdir sysroot && docker export r36s-root | tar -x -C sysroot
export R36S_SYSROOT=$PWD/sysroot
cmake -S . -B build/r36s -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/linux-aarch64-r36s.cmake -DCMAKE_BUILD_TYPE=Release -DOPENCMR2_VR=OFF -DOPENCMR2_RENDERER_GLES=ON -DOPENCMR2_VENDORED_SDL=ON -DSDL_KMSDRM=ON -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_VULKAN=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON "-DCMAKE_INSTALL_RPATH=\$ORIGIN:\$ORIGIN/libs.aarch64"
ninja -C build/r36s opencmr2
```

The `libSDL3.so.0` in `libs.aarch64` is the sdl3-sdl2-backend build, which uses the device's own SDL2.

```bash
git clone --recursive https://github.com/bmdhacks/SDL.git -b sdl2-backend
git clone https://github.com/KhronosGroup/SPIRV-Cross.git
cd SDL
git checkout 6057d79baf8321bf190479a699655f06cc2a962f
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=../OpenCMR2/cmake/linux-aarch64-r36s.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-march=armv8-a" -DSDL_SDL2_BACKEND=ON -DSDL_SPIRV_CROSS_DIR=$PWD/../SPIRV-Cross -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_PIPEWIRE=OFF -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF -DSDL_SNDIO=OFF -DSDL_OSS=OFF -DSDL_JACK=OFF -DSDL_OFFSCREEN=OFF -DSDL_DUMMYVIDEO=OFF -DSDL_DUMMYAUDIO=OFF -DSDL_DISKAUDIO=OFF -DSDL_VULKAN=OFF -DSDL_GPU=ON -DSDL_RENDER_GPU=ON -DSDL_UNIX_CONSOLE_BUILD=ON
ninja -C build
```
