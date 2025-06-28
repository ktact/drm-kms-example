#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "drm.h"
#include "triangle.h"

static volatile int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int main() {
    drm_device_t dev = {0};
    dev.fd = -1;
    dev.fb_map = MAP_FAILED;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (drm_open_device(&dev)) {
        return 1;
    }

    if (drm_setup_display(&dev)) {
        drm_cleanup(&dev);
        return 1;
    }

    if (drm_create_framebuffer(&dev)) {
        drm_cleanup(&dev);
        return 1;
    }

    printf("Animated triangle rendering started (%dx%d). Press Ctrl+C to exit.\n", dev.width, dev.height);
    
    int is_virtual_console = (getenv("DISPLAY") == NULL && getenv("WAYLAND_DISPLAY") == NULL);
    
    if (is_virtual_console) {
        printf("Virtual console detected - attempting direct DRM display...\n");
    }
    
    if (drmModeSetCrtc(dev.fd, dev.crtc->crtc_id, dev.fb_id, 0, 0,
                       &dev.connector->connector_id, 1, &dev.mode) == 0) {
        printf("Triangle animation displayed directly on screen!\n");
        if (is_virtual_console) {
            printf("You should see the animated triangle on your display now.\n");
        }
    } else {
        printf("Warning: Failed to set CRTC - %s\n", strerror(errno));
        if (is_virtual_console) {
            printf("Try running as root: sudo ./triangle\n");
            printf("Or switch to a virtual console (Ctrl+Alt+F3) and run there.\n");
        }
        printf("Continuing with framebuffer rendering only.\n");
    }

    struct timespec start_time, current_time, last_frame;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    last_frame = start_time;
    
    const long target_frame_time = 16667;
    
    while (running) {
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        float elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                       (current_time.tv_nsec - start_time.tv_nsec) / 1e9f;
        
        float angle = elapsed * 0.8f;
        float cx = dev.width / 2 + 300 * cosf(elapsed * 0.3f);
        float cy = dev.height / 2 + 150 * sinf(elapsed * 0.2f);
        float size = 200 + 100 * sinf(elapsed * 0.5f);
        
        triangle_clear_framebuffer(&dev);
        triangle_draw(&dev, cx, cy, angle, size);
        
        drm_refresh_display(&dev);
        
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long frame_time = (current_time.tv_sec - last_frame.tv_sec) * 1000000L + 
                         (current_time.tv_nsec - last_frame.tv_nsec) / 1000L;
        
        if (frame_time < target_frame_time) {
            usleep(target_frame_time - frame_time);
        }
        last_frame = current_time;
    }

    printf("\nAnimation stopped.\n");
    drm_cleanup(&dev);
    return 0;
}