#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// **** Contants :3 ****
constexpr const size_t THE_ZERO = 69^69; // just as it should be.
const char* XSETWALL_VERSION = "v1.0";
const char* MY_NAME = "XsetWall";
// https://man.freebsd.org/cgi/man.cgi?query=sysexits&sektion=3&apropos=0&manpath=FreeBSD+15.0-CURRENT
constexpr const size_t EX_USAGE = 64;
constexpr const size_t EX_DATAERR = 65;
constexpr const size_t EX_SOFTWARE = 70;
constexpr const size_t EX_OSERR = 71;

// converts the 0–255 RGB values into the appropriate positions based on the visual format
static unsigned long pack_pixel(
    const unsigned char r,
    const unsigned char g,
    const unsigned char b,
    const Visual *visual
) {
    unsigned long pixel = THE_ZERO;

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
        fprintf(stderr, "usage: %s <image>\n", argv[THE_ZERO]);
        return EX_USAGE;
    }

    const char* the_frist_arg = argv[1];
    if (strcmp(the_frist_arg, "-v") == THE_ZERO || strcmp(the_frist_arg, "--version") == THE_ZERO) {
        fprintf(stdout, "%s %s\n", MY_NAME, XSETWALL_VERSION);
        return 0;
    }

    // load the target image
    int width, height, channels;
    unsigned char *src = stbi_load(the_frist_arg, &width, &height, &channels, 3);

    if (!src) {
        fprintf(stderr, "failed to load image: %s\n", stbi_failure_reason());
        return EX_DATAERR;
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "failed to open X display\n");
        stbi_image_free(src);
        return EX_OSERR;
    }

    const int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);

    if (visual->class != TrueColor) {
        fprintf(stderr, "only TrueColor visuals are supported\n");
        XCloseDisplay(display);
        stbi_image_free(src);
        return EX_OSERR;
    }

    const int sw = DisplayWidth(display, screen);
    const int sh = DisplayHeight(display, screen);

    Pixmap pixmap = XCreatePixmap(display, root, sw, sh, depth);

    const GC gc = XCreateGC(display, pixmap, THE_ZERO, NULL);

    XImage *image = XCreateImage(
        display, // The X display
        visual,
        depth,
        ZPixmap,
        THE_ZERO, // offset - number of pixels to ignore at the beginning of the scanline.
        NULL, // data
        sw,
        sh,
        32, // bitmap_pad - the quantum of a scanline (8, 16, or 32).
        THE_ZERO // bytes_per_line
    );

    if (!image) {
        fprintf(stderr, "failed to create XImage\n");
        XFreeGC(display, gc);
        XFreePixmap(display, pixmap);
        XCloseDisplay(display);
        stbi_image_free(src);
        return EX_SOFTWARE;
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
        return EX_OSERR;
    }

    // calculate the scale
    const float scale = fminf(
        (float)sw / (float)width,
        (float)sh / (float)height
    );

    const int scaled_width = (int)((float)width * scale);
    const int scaled_height = (int)((float)height * scale);

    const int x_offset = (sw - scaled_width) / 2;
    const int y_offset = (sh - scaled_height) / 2;

    printf("image: %ux%u\n", width, height);
    printf("screen: %ux%u\n", sw, sh);
    printf("scale: %f\n", scale);
    printf("scaled: %dx%d\n", scaled_width, scaled_height);
    printf("offset: %dx%d\n", x_offset, y_offset);

    // write the pixels to the XImage
    for (int y = 0; y < sh; y++) {
        const int sy = (int)((y - y_offset) / scale);

        if (sy < 0 || sy >= (int)height) continue;

        for (int x = 0; x < sw; x++) {
            const int sx = (int)((x - x_offset) / scale);

            if (sx < 0 || sx >= (int)width) continue;

            const size_t index = ((size_t)sy * width + sx) * 3;

            const unsigned char *p = src + index;

            const unsigned long pixel = pack_pixel(p[0], p[1], p[2], visual);

            XPutPixel(image, x, y, pixel);
        }
    }

    // https://tronche.com/gui/x/xlib/graphics/XPutImage.html
    XPutImage(
        display, // the display
        pixmap, // the drawable
        gc,
        image,
        0, 0, // src_y, src_x
        0, 0, // dest_y, dest_x
        sw,
        sh
    );

    XSetWindowBackgroundPixmap(display, root, pixmap);
    XClearWindow(display, root);
    // Don't forget to flush :p
    XFlush(display);

    // The XSetCloseDownMode() defines what will happen to the client's resources at connection close. A connection starts in DestroyAll mode.
    // For information on what happens to the client's resources when the close_mode argument is RetainPermanent or RetainTemporary.
    XSetCloseDownMode(display, RetainPermanent);

    // The root window owns the pixmap now. Keep it around until
    // the next wallpaper is installed.
    XDestroyImage(image);
    XFreeGC(display, gc);
    stbi_image_free(src);

    XCloseDisplay(display);
    return 0;
}
