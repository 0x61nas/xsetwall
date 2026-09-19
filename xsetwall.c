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
constexpr const unsigned char WHITE_FLAG = 1 << 1;
constexpr const unsigned char CUSTOM_SCALE = 1 << 2;
constexpr const unsigned char ALIGN_LEFT = 1 << 3;
constexpr const unsigned char ALIGN_RIGHT = 1 << 4;
constexpr const unsigned char ALIGN_TOP = 1 << 5;
constexpr const unsigned char ALIGN_BOTTOM = 1 << 6;
// https://man.freebsd.org/cgi/man.cgi?query=sysexits&sektion=3&apropos=0&manpath=FreeBSD+15.0-CURRENT
constexpr const size_t EX_USAGE = 64;
constexpr const size_t EX_DATAERR = 65;
constexpr const size_t EX_SOFTWARE = 70;
constexpr const size_t EX_OSERR = 71;

// **** Static functions ****
static void die(const char* s, const size_t code) { fputs(s, stderr); fputc('\n', stderr); exit(code); }
static void die_with_usage(const char* cmd) {
    char buff[256];
    const int n = snprintf(buff, sizeof(buff), "usage: %s [-w] [-s custom-scale] <image>\n", cmd);
    if (n < 0) die("failed to format help message", EX_SOFTWARE);
    die(buff, EX_USAGE); // NOTE(anas): we die here so no need to free anything :)
}
static void die_with_help(const char* cmd) {
    char buff[2024];
    const int n = snprintf(
        buff,
        sizeof(buff),
        "usage: %s [-w|--white-border] [-s|--scale <custom-scale>] <image>\n"
        "\n"
        "Options:\n"
        "  -w, --white-border       Add a white border around the image\n"
        "  -s, --scale <scale>      Set a custom image scale\n"
        "  -l, --align-left         Align the image to the left\n"
        "  -r, --align-right        Align the image to the right\n"
        "  -t, --align-top          Align the image to the top\n"
        "  -b, --align-bottom       Align the image to the bottom\n"
        "  -h, --help               Show this help message\n"
        "  -v, --version            Show version information\n",
        cmd
    );

    if (n < 0) die("failed to format help message", EX_SOFTWARE);

    fputs(buff, stdout);
    exit(EXIT_SUCCESS);
}
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
    const char* MY_CMD = argv[THE_ZERO];
    if (argc < 2) {
        die_with_usage(MY_CMD);
    }

    unsigned char flags = 0;
    float scale = 0.0f;
    char* image_path = NULL;
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];    
        if (strcmp(arg, "-v") == THE_ZERO || strcmp(arg, "--version") == THE_ZERO) {
            fprintf(stdout, "%s %s\n", MY_NAME, XSETWALL_VERSION);
            return 0;
        }
        if (strcmp(arg, "-w") == THE_ZERO || strcmp(arg, "--white-borders") == THE_ZERO) flags |= WHITE_FLAG;
        else if ((strcmp(arg, "-s") == THE_ZERO || strcmp(arg, "--scale") == THE_ZERO) && i < argc + 2) {
            char* end;
            scale = strtof(argv[++i], &end);
            if (*end != '\0') die("invalid scale value", EX_DATAERR);
            flags |= CUSTOM_SCALE;
        } else if (strcmp(arg, "-l") == THE_ZERO || strcmp(arg, "--align-left") == THE_ZERO) flags |= ALIGN_LEFT; 
        else if (strcmp(arg, "-r") == THE_ZERO || strcmp(arg, "--align-right") == THE_ZERO) flags |= ALIGN_RIGHT; 
        else if (strcmp(arg, "-t") == THE_ZERO || strcmp(arg, "--align-top") == THE_ZERO) flags |= ALIGN_TOP; 
        else if (strcmp(arg, "-b") == THE_ZERO || strcmp(arg, "--align-bottom") == THE_ZERO) flags |= ALIGN_BOTTOM; 
        else if (strcmp(arg, "-h") == THE_ZERO || strcmp(arg, "--help") == THE_ZERO) die_with_help(MY_CMD);
        else image_path = (char*)arg;
    }

    if (!image_path) die_with_usage(MY_CMD);

    // load the target image
    int width, height, channels;
    unsigned char *src = stbi_load(image_path, &width, &height, &channels, 3);

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

    // if the WHITE_FLAG is set we flush the image white
    if ((flags & WHITE_FLAG) == WHITE_FLAG) {
        const unsigned long white = WhitePixel(display, screen);

        for (int y = 0; y < image->height; y++) {
            for (int x = 0; x < image->width; x++) {
                XPutPixel(image, x, y, white);
            }
        }
    }

    // calculate the scale
    if ((flags & CUSTOM_SCALE) != CUSTOM_SCALE) scale = fminf(
        (float)sw / (float)width,
        (float)sh / (float)height
    );

    const int scaled_width = (int)((float)width * scale);
    const int scaled_height = (int)((float)height * scale);

    const int x_offset = (sw - scaled_width) / ((flags & ALIGN_LEFT) == ALIGN_LEFT ? sw : (flags & ALIGN_RIGHT) == ALIGN_RIGHT ? 1 : 2);
    const int y_offset = (sh - scaled_height) / ((flags & ALIGN_TOP) == ALIGN_TOP ? sh : (flags & ALIGN_BOTTOM) == ALIGN_BOTTOM ? 1 : 2);

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
// Stay Silly :3
