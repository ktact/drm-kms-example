#ifndef DRM_H
#define DRM_H

#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

typedef struct {
    int fd;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeCrtc *crtc;
    drmModeModeInfo mode;
    uint32_t fb_id;
    uint32_t fb_handle;
    void *fb_map;
    size_t fb_size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} drm_device_t;

int drm_open_device(drm_device_t *dev);
int drm_setup_display(drm_device_t *dev);
int drm_create_framebuffer(drm_device_t *dev);
void drm_cleanup(drm_device_t *dev);
void drm_refresh_display(drm_device_t *dev);

#endif