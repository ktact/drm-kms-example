#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "triangle.h"

void triangle_clear_framebuffer(drm_device_t *dev) {
    memset(dev->fb_map, 0, dev->fb_size);
}

void triangle_put_pixel(drm_device_t *dev, int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)dev->width && y >= 0 && y < (int)dev->height) {
        uint32_t *pixel = (uint32_t *)((char *)dev->fb_map + y * dev->stride + x * 4);
        *pixel = color;
    }
}

void triangle_draw_line(drm_device_t *dev, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        triangle_put_pixel(dev, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void triangle_draw(drm_device_t *dev, float cx, float cy, float angle, float size) {
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    
    float x1 = 0;
    float y1 = -size;
    float x2 = -size * 0.866f;
    float y2 = size / 2;
    float x3 = size * 0.866f;
    float y3 = size / 2;
    
    int rx1 = (int)(cx + x1 * cos_a - y1 * sin_a);
    int ry1 = (int)(cy + x1 * sin_a + y1 * cos_a);
    int rx2 = (int)(cx + x2 * cos_a - y2 * sin_a);
    int ry2 = (int)(cy + x2 * sin_a + y2 * cos_a);
    int rx3 = (int)(cx + x3 * cos_a - y3 * sin_a);
    int ry3 = (int)(cy + x3 * sin_a + y3 * cos_a);

    uint32_t red = 0x00FF0000;
    uint32_t green = 0x0000FF00;
    uint32_t blue = 0x000000FF;

    triangle_draw_line(dev, rx1, ry1, rx2, ry2, red);
    triangle_draw_line(dev, rx2, ry2, rx3, ry3, green);
    triangle_draw_line(dev, rx3, ry3, rx1, ry1, blue);
}