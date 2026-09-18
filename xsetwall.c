#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// converts the 0–255 RGB values into the appropriate positions based on the visual format
static unsigned long pack_pixel(
    const unsigned char r,
    const unsigned char g,
    const unsigned char b,
    const Visual *visual
) {
    unsigned long pixel = 0;

    const unsigned long rm = visual->red_mask;
    const unsigned long gm = visual->green_mask;
    const unsigned long bm = visual->blue_mask;

    const int rs = __builtin_ctzl(rm);
    const int gs = __builtin_ctzl(gm);
    const int bs = __builtin_ctzl(bm);

    const int rb = __builtin_popcountl(rm);
    const int gb = __builtin_popcountl(gm);
    const int bb = __builtin_popcountl(bm);

    const unsigned long rv = ((unsigned long)r * ((1UL << rb) - 1)) / 255;
    const unsigned long gv = ((unsigned long)g * ((1UL << gb) - 1)) / 255;
    const unsigned long bv = ((unsigned long)b * ((1UL << bb) - 1)) / 255;

    pixel |= (rv << rs) & rm;
    pixel |= (gv << gs) & gm;
    pixel |= (bv << bs) & bm;

    return pixel;
}

int main(const int argc, const char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <image>\n", argv[0]);
        return 1;
    }

    int width, height, channels;
    unsigned char *src = stbi_load(argv[1], &width, &height, &channels, 3);

    if (!src) {
        fprintf(stderr, "failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "failed to open X display\n");
        stbi_image_free(src);
        return 1;
    }

    const int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);

    if (visual->class != TrueColor) {
        fprintf(stderr, "only TrueColor visuals are supported\n");
        XCloseDisplay(display);
        stbi_image_free(src);
        return 1;
    }

    const int sw = DisplayWidth(display, screen);
    const int sh = DisplayHeight(display, screen);

    Pixmap pixmap = XCreatePixmap(display, root, sw, sh, depth);

    const GC gc = XCreateGC(display, pixmap, 0, NULL);

    XImage *image = XCreateImage(
        display,
        visual,
        depth,
        ZPixmap,
        0,
        NULL,
        sw,
        sh,
        32,
        0
    );

    if (!image) {
        fprintf(stderr, "failed to create XImage\n");
        XFreeGC(display, gc);
        XFreePixmap(display, pixmap);
        XCloseDisplay(display);
        stbi_image_free(src);
        return 1;
    }

    image->data = calloc(1, image->bytes_per_line * sh);

    if (!image->data) {
        fprintf(stderr, "out of memory\n");
        image->data = NULL;
        XDestroyImage(image);
        XFreeGC(display, gc);
        XFreePixmap(display, pixmap);
        XCloseDisplay(display);
        stbi_image_free(src);
        return 1;
    }

    // write the pixels to the XImage
    for (int y = 0; y < sh; y++) {
        const int sy = (long)y * height / sh;

        for (int x = 0; x < sw; x++) {
            const int sx = (long)x * width / sw;
            unsigned char *p = src + (sy * width + sx) * 3;

            const unsigned long pixel = pack_pixel(p[0], p[1], p[2], visual);

            XPutPixel(image, x, y, pixel);
        }
    }

    XPutImage(
        display,
        pixmap,
        gc,
        image,
        0, 0,
        0, 0,
        sw, sh
    );

    XSetWindowBackgroundPixmap(display, root, pixmap);
    XClearWindow(display, root);
    // Don't forget to flush :p
    XFlush(display);

    // The root window owns the pixmap now. Keep it around until
    // the next wallpaper is installed.
    XDestroyImage(image);
    XFreeGC(display, gc);
    stbi_image_free(src);

    XCloseDisplay(display);
    return 0;
}
