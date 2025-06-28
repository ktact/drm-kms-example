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
    
    for (i = 0; i < 8; i++) {
        snprintf(device_path, sizeof(device_path), "/dev/dri/card%d", i);
        dev->fd = open(device_path, O_RDWR | O_CLOEXEC);
        if (dev->fd >= 0) {
            printf("Opened DRM device: %s\n", device_path);
            
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

    resources = drmModeGetResources(dev->fd);
    if (!resources) {
        fprintf(stderr, "Failed to get DRM resources\n");
        return -1;
    }

    printf("Found %d connectors\n", resources->count_connectors);
    
    for (i = 0; i < resources->count_connectors; i++) {
        dev->connector = drmModeGetConnector(dev->fd, resources->connectors[i]);
        if (dev->connector) {
            printf("Connector %d: connection=%d, modes=%d\n", 
                   i, dev->connector->connection, dev->connector->count_modes);
            
            if (dev->connector->connection == DRM_MODE_CONNECTED &&
                dev->connector->count_modes > 0) {
                break;
            }
        }
        drmModeFreeConnector(dev->connector);
        dev->connector = NULL;
    }

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

    dev->mode = dev->connector->modes[0];
    dev->width = dev->mode.hdisplay;
    dev->height = dev->mode.vdisplay;

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
    
    dev->stride = dev->width * 4;
    create_req.width = dev->width;
    create_req.height = dev->height;
    create_req.bpp = 32;

    if (drmIoctl(dev->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req)) {
        perror("Failed to create dumb buffer");
        return -1;
    }

    dev->fb_handle = create_req.handle;
    dev->fb_size = create_req.size;

    if (drmModeAddFB(dev->fd, dev->width, dev->height, 24, 32,
                     dev->stride, dev->fb_handle, &dev->fb_id)) {
        perror("Failed to add framebuffer");
        return -1;
    }

    map_req.handle = dev->fb_handle;
    if (drmIoctl(dev->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req)) {
        perror("Failed to map dumb buffer");
        return -1;
    }

    dev->fb_map = mmap(0, dev->fb_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED, dev->fd, map_req.offset);
    if (dev->fb_map == MAP_FAILED) {
        perror("Failed to mmap framebuffer");
        return -1;
    }

    memset(dev->fb_map, 0, dev->fb_size);
    return 0;
}

void drm_refresh_display(drm_device_t *dev) {
    __sync_synchronize();
    msync(dev->fb_map, dev->fb_size, MS_SYNC);
    
    drmModeSetCrtc(dev->fd, dev->crtc->crtc_id, dev->fb_id, 0, 0,
                   &dev->connector->connector_id, 1, &dev->mode);
}


void drm_cleanup(drm_device_t *dev) {
    if (dev->fb_map != MAP_FAILED) {
        munmap(dev->fb_map, dev->fb_size);
    }
    
    if (dev->fb_id) {
        drmModeRmFB(dev->fd, dev->fb_id);
    }
    
    if (dev->fb_handle) {
        struct drm_mode_destroy_dumb destroy_req = {0};
        destroy_req.handle = dev->fb_handle;
        drmIoctl(dev->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    }
    
    if (dev->crtc) {
        drmModeFreeCrtc(dev->crtc);
    }
    
    if (dev->encoder) {
        drmModeFreeEncoder(dev->encoder);
    }
    
    if (dev->connector) {
        drmModeFreeConnector(dev->connector);
    }
    
    if (dev->fd >= 0) {
        close(dev->fd);
    }
}