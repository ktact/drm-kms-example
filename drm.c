#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <drm/drm_fourcc.h>

#include "drm.h"

int drm_open_device(drm_device_t *dev) {
    char device_path[32];
    int i;
    
    /*
     * Try to open DRM devices (/dev/dri/card0, card1, etc.)
     * DRM devices represent graphics hardware - each card corresponds to a GPU
     * We need read/write access to control display output and create framebuffers
     */
    for (i = 0; i < 8; i++) {
        snprintf(device_path, sizeof(device_path), "/dev/dri/card%d", i);
        dev->fd = open(device_path, O_RDWR | O_CLOEXEC);
        if (dev->fd >= 0) {
            printf("Opened DRM device: %s\n", device_path);
            
            /*
             * Become DRM master to gain exclusive control over display output
             * This is required for mode setting and direct framebuffer access
             * Without master status, we can't change display modes or show content
             */
            if (drmSetMaster(dev->fd)) {
                printf("Failed to become DRM master, continuing anyway\n");
            } else {
                printf("Successfully became DRM master\n");
            }
            
            return 0;
        }
    }
    
    perror("Failed to open any DRM device");
    return -1;
}

int drm_setup_display(drm_device_t *dev) {
    drmModeRes *resources;
    int i;

    /*
     * Get DRM resources - this queries the kernel for available display hardware
     * Resources include connectors (physical ports), encoders (signal converters),
     * and CRTCs (scanout engines that read framebuffers and send to displays)
     */
    resources = drmModeGetResources(dev->fd);
    if (!resources) {
        fprintf(stderr, "Failed to get DRM resources\n");
        return -1;
    }

    printf("Found %d connectors\n", resources->count_connectors);
    
    /*
     * Find a connected display connector (HDMI, DisplayPort, VGA, etc.)
     * Connectors represent physical output ports on the graphics card
     * We need a connected one to actually display our triangle
     */
    for (i = 0; i < resources->count_connectors; i++) {
        dev->connector = drmModeGetConnector(dev->fd, resources->connectors[i]);
        if (dev->connector) {
            printf("Connector %d: connection=%d, modes=%d\n", 
                   i, dev->connector->connection, dev->connector->count_modes);
            
            /*
             * Prefer connectors that are physically connected and have display modes
             * Display modes define resolution, refresh rate, and timing parameters
             */
            if (dev->connector->connection == DRM_MODE_CONNECTED &&
                dev->connector->count_modes > 0) {
                break;
            }
        }
        drmModeFreeConnector(dev->connector);
        dev->connector = NULL;
    }

    /*
     * Fallback: if no connected connector found, use any connector with modes
     * This helps in virtual environments or when display detection fails
     */
    if (!dev->connector) {
        printf("No connected connector found, trying first available connector\n");
        for (i = 0; i < resources->count_connectors; i++) {
            dev->connector = drmModeGetConnector(dev->fd, resources->connectors[i]);
            if (dev->connector && dev->connector->count_modes > 0) {
                printf("Using connector %d with %d modes\n", i, dev->connector->count_modes);
                break;
            }
            drmModeFreeConnector(dev->connector);
            dev->connector = NULL;
        }
    }

    if (!dev->connector) {
        fprintf(stderr, "No usable connector found\n");
        drmModeFreeResources(resources);
        return -1;
    }

    /*
     * Use the first available display mode (usually the preferred/native resolution)
     * This determines our framebuffer size and display timing parameters
     */
    dev->mode = dev->connector->modes[0];
    dev->width = dev->mode.hdisplay;
    dev->height = dev->mode.vdisplay;

    /*
     * Find the encoder associated with our connector
     * Encoders convert digital framebuffer data into the appropriate signal format
     * (TMDS for HDMI/DVI, DisplayPort packets, analog for VGA, etc.)
     */
    for (i = 0; i < resources->count_encoders; i++) {
        dev->encoder = drmModeGetEncoder(dev->fd, resources->encoders[i]);
        if (dev->encoder && dev->encoder->encoder_id == dev->connector->encoder_id) {
            break;
        }
        drmModeFreeEncoder(dev->encoder);
        dev->encoder = NULL;
    }

    if (!dev->encoder) {
        fprintf(stderr, "No encoder found\n");
        drmModeFreeConnector(dev->connector);
        drmModeFreeResources(resources);
        return -1;
    }

    /*
     * Get the CRTC (CRT Controller) associated with our encoder
     * CRTC is the scanout engine that reads our framebuffer line by line
     * and sends the pixel data to the encoder at the correct timing
     */
    dev->crtc = drmModeGetCrtc(dev->fd, dev->encoder->crtc_id);
    if (!dev->crtc) {
        fprintf(stderr, "No CRTC found\n");
        drmModeFreeEncoder(dev->encoder);
        drmModeFreeConnector(dev->connector);
        drmModeFreeResources(resources);
        return -1;
    }

    drmModeFreeResources(resources);
    return 0;
}

int drm_create_framebuffer(drm_device_t *dev) {
    struct drm_mode_create_dumb create_req = {0};
    struct drm_mode_map_dumb map_req = {0};
    
    /*
     * Calculate stride (bytes per row) - 4 bytes per pixel for 32-bit RGBA
     * This ensures proper memory alignment for pixel data access
     */
    dev->stride = dev->width * 4;
    create_req.width = dev->width;
    create_req.height = dev->height;
    create_req.bpp = 32; // 32 bits per pixel (8R + 8G + 8B + 8A)

    /*
     * Create a "dumb buffer" - a simple linear buffer in GPU memory
     * This allocates video memory that can be directly written to by CPU
     * "Dumb" means no acceleration features, just raw memory for pixels
     */
    if (drmIoctl(dev->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req)) {
        perror("Failed to create dumb buffer");
        return -1;
    }

    dev->fb_handle = create_req.handle;
    dev->fb_size = create_req.size;

    /*
     * Register our buffer as a framebuffer object with the display controller
     * This tells DRM that this buffer contains pixel data in the specified format
     * The CRTC will scan out this framebuffer to the display
     */
    if (drmModeAddFB(dev->fd, dev->width, dev->height, 24, 32,
                     dev->stride, dev->fb_handle, &dev->fb_id)) {
        perror("Failed to add framebuffer");
        return -1;
    }

    /*
     * Get memory mapping offset for our buffer
     * This prepares the buffer for CPU access via memory mapping
     */
    map_req.handle = dev->fb_handle;
    if (drmIoctl(dev->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req)) {
        perror("Failed to map dumb buffer");
        return -1;
    }

    /*
     * Map the framebuffer into our process's virtual memory space
     * This allows us to write pixels directly to video memory as if it's regular RAM
     * Any writes here will be visible on the display when the CRTC scans it out
     */
    dev->fb_map = mmap(0, dev->fb_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED, dev->fd, map_req.offset);
    if (dev->fb_map == MAP_FAILED) {
        perror("Failed to mmap framebuffer");
        return -1;
    }

    /*
     * Clear the framebuffer to black (all zeros)
     * This ensures we start with a clean slate for our triangle animation
     */
    memset(dev->fb_map, 0, dev->fb_size);
    return 0;
}

void drm_refresh_display(drm_device_t *dev) {
    /*
     * Ensure all CPU writes to framebuffer are completed before display update
     * This memory barrier prevents reordering that could show partial frames
     */
    __sync_synchronize();

    /*
     * Sync our memory-mapped framebuffer changes to the actual video memory
     * This ensures all pixel writes are committed before the display reads them
     */
    msync(dev->fb_map, dev->fb_size, MS_SYNC);

    /*
     * Tell the CRTC to scan out our framebuffer to the connected display
     * This is the actual "display update" - the hardware reads our pixel data
     * and sends it through the encoder -> connector -> monitor pipeline
     */
    drmModeSetCrtc(dev->fd, dev->crtc->crtc_id, dev->fb_id, 0, 0,
                   &dev->connector->connector_id, 1, &dev->mode);
}


void drm_cleanup(drm_device_t *dev) {
    /*
     * Unmap the framebuffer from our process memory space
     * This releases the virtual memory mapping we created for pixel access
     */
    if (dev->fb_map != MAP_FAILED) {
        munmap(dev->fb_map, dev->fb_size);
    }
    
    /*
     * Remove the framebuffer from DRM's tracking
     * This tells the display controller this buffer is no longer available for scanout
     */
    if (dev->fb_id) {
        drmModeRmFB(dev->fd, dev->fb_id);
    }
    
    /*
     * Destroy the dumb buffer and free the video memory
     * This releases the GPU memory we allocated for our pixel data
     */
    if (dev->fb_handle) {
        struct drm_mode_destroy_dumb destroy_req = {0};
        destroy_req.handle = dev->fb_handle;
        drmIoctl(dev->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    }
    
    /*
     * Free DRM mode objects we allocated during setup
     * These were allocated by drmModeGet* functions and need explicit cleanup
     */
    if (dev->crtc) {
        drmModeFreeCrtc(dev->crtc);
    }
    
    if (dev->encoder) {
        drmModeFreeEncoder(dev->encoder);
    }
    
    if (dev->connector) {
        drmModeFreeConnector(dev->connector);
    }
    
    /*
     * Close the DRM device file descriptor
     * This releases our connection to the graphics hardware
     */
    if (dev->fd >= 0) {
        close(dev->fd);
    }
}
