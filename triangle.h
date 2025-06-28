#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "drm.h"

void triangle_clear_framebuffer(drm_device_t *dev);
void triangle_put_pixel(drm_device_t *dev, int x, int y, uint32_t color);
void triangle_draw_line(drm_device_t *dev, int x0, int y0, int x1, int y1, uint32_t color);
void triangle_draw(drm_device_t *dev, float cx, float cy, float angle, float size);

#endif