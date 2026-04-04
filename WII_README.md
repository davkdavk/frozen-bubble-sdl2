# Build Instructions for Wii

This directory contains the source code port of Frozen Bubble to the Nintendo Wii.

## Controls

| Action | Wiimote | Classic Controller |
|--------|---------|-------------------|
| Move Menu Up | D-pad Up / 1 | D-pad Up / L-Stick Up |
| Move Menu Down | D-pad Down / 2 | D-pad Down / L-Stick Down |
| Move Menu Left | D-pad Left | D-pad Left |
| Move Menu Right | D-pad Right | D-pad Right |
| Select | A / + | A / + |
| Back/Cancel | B / - | B / - |
| Pause Game | Home | Home |
| Fire Bubble | A | A |
| Aim Left | D-pad Left | D-pad Left |
| Aim Right | D-pad Right | D-pad Right |

## Prerequisites

- devkitPro with devkitPPC
- libogc
- libfat (for SD card access)
- libwiiuse (for Wii Remote support)

## Building

1. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

2. Configure with CMake:
   ```bash
   cmake -DCMAKE_TOOLCHAIN_FILE=../CMakeLists.wii ..
   ```

3. Build:
   ```bash
   make
   ```

4. The DOL file will be created at `build/frozen-bubble-wii.dol`

## Installation

Copy the following to your SD card:
- `frozen-bubble-wii.dol` → `/apps/frozenbubble/boot.dol`
- `share/` folder → `/apps/frozenbubble/share/`

## Configuration

- Data is loaded from `sd:/apps/frozenbubble/share/`
- Resolution: 640x480 (original game resolution)
- Aspect ratio: 4:3 (Wii will scale to TV)
