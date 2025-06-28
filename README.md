# DRM/KMS Triangle Demo

## Purpose

This program demonstrates how to use the **DRM/KMS (Direct Rendering Manager/Kernel Mode Setting) API** in a simple, educational way. It serves as a minimal example for:

- Opening and configuring DRM devices
- Creating and managing framebuffers
- Implementing basic graphics rendering directly to hardware
- Understanding low-level display control in Linux

The program renders an animated triangle that rotates, moves in orbital patterns, and changes size - all using direct hardware access without any graphics libraries or window managers.

## How to Use

### Prerequisites

- Linux system with DRM/KMS support
- libdrm development packages installed
- Physical access to keyboard (for console switching)

### Build

```bash
make
```

### Running the Program

This program requires switching between virtual console and desktop session for optimal results:

#### Method 1: Virtual Console (Recommended)

1. **Switch to Virtual Console:**
   - Press `Ctrl+Alt+F3` (or F4, F5, F6)
   - Login with your username and password

2. **Run the Triangle:**
   ```bash
   cd /path/to/drm
   sudo ./triangle
   ```

3. **View Animation:**
   - You should see the animated triangle directly on your display
   - The triangle will rotate, move, and change size continuously

4. **Stop and Return:**
   - Press `Ctrl+C` to stop the animation
   - Press `Ctrl+Alt+F2` to return to your desktop

### Important Note

This program **requires virtual console access** to function properly. Running from a desktop session will not provide the necessary DRM master privileges for display output.

### Why Virtual Console?

- **Direct Hardware Access**: No interference from X11/Wayland compositors
- **DRM Master Privileges**: Easier to obtain on virtual consoles
- **Real Display Output**: Triangle renders directly to your physical screen
- **Full Performance**: No windowing system overhead

## Project Structure

```
├── main.c              # Main program and animation loop
├── drm.h, drm.c        # DRM/KMS API management
├── triangle.h, triangle.c  # Graphics rendering functions
└── Makefile            # Build system
```

## Features

- **Modular Design**: Separated DRM API and graphics code
- **Real-time Animation**: 60 FPS smooth rendering
- **Multiple Movement Patterns**: Rotation, orbital motion, and scaling
- **Environment Detection**: Automatic handling of console vs desktop
- **Educational**: Clean, commented code for learning DRM/KMS

## Technical Details

- **API**: Direct DRM/KMS calls (no OpenGL/Vulkan)
- **Rendering**: Software rasterization using Bresenham line algorithm
- **Display**: 32-bit RGBA framebuffer with memory-mapped access
- **Synchronization**: Manual display refresh and frame timing

---

*This project demonstrates low-level graphics programming on Linux and serves as an educational example of DRM/KMS API usage.*
