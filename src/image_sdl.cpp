/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>

#include "debug.h"
#include "image.h"

Image::Image() {
}

/**
 * Creates a new image.  Scale is stored to allow drawing using U4
 * (320x200) coordinates, regardless of the actual image scale.
 * Indexed is true for palette based images, or false for RGB images.
 * Image type determines whether to create a hardware (i.e. video ram)
 * or software (i.e. normal ram) image.
 */
Image *Image::create(int w, int h, int scale, int indexed, Image::Type type) {
    Uint32 rmask, gmask, bmask, amask;
    Uint32 flags;
    Image *im = new Image;

    im->w = w;
    im->h = h;
    im->scale = scale;
    im->indexed = indexed;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    if (type == Image::HARDWARE)
        flags = SDL_HWSURFACE;
    else
        flags = SDL_SWSURFACE;
    
    if (indexed)
        im->surface = SDL_CreateRGBSurface(flags, w, h, 8, rmask, gmask, bmask, amask);
    else
        im->surface = SDL_CreateRGBSurface(flags, w, h, 32, rmask, gmask, bmask, amask);

    if (!im->surface) {
        delete im;
        return NULL;
    }

    return im;
}

/**
 * Frees the image.
 */
Image::~Image() {
    SDL_FreeSurface(surface);
}

/**
 * Sets the palette
 */
void Image::setPalette(const RGBA *colors, unsigned n_colors) {
    ASSERT(indexed, "imageSetPalette called on non-indexed image");
    
    SDL_Color *sdlcolors = new SDL_Color[n_colors];
    for (unsigned i = 0; i < n_colors; i++) {
        sdlcolors[i].r = colors[i].r;
        sdlcolors[i].g = colors[i].g;
        sdlcolors[i].b = colors[i].b;
    }

    SDL_SetColors(surface, sdlcolors, 0, n_colors);

    delete [] sdlcolors;
}

/**
 * Copies the palette from another image.
 */
void Image::setPaletteFromImage(const Image *src) {
    ASSERT(indexed && src->indexed, "imageSetPaletteFromImage called on non-indexed image");
    memcpy(surface->format->palette->colors, 
           src->surface->format->palette->colors, 
           sizeof(SDL_Color) * src->surface->format->palette->ncolors);
}

bool Image::getTransparentIndex(unsigned int &index) const {
    if (!indexed || (surface->flags & SDL_SRCCOLORKEY) == 0)
        return false;
        
    index = surface->format->colorkey;
    return true;
}

void Image::setTransparentIndex(unsigned int index) {

    SDL_SetAlpha(surface, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);

    if (indexed) {
        SDL_SetColorKey(surface, SDL_SRCCOLORKEY, index);
    } else {
        int x, y;
        Uint8 t_r, t_g, t_b;

        SDL_GetRGB(index, surface->format, &t_r, &t_g, &t_b);

        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                unsigned int r, g, b, a;
                getPixel(x, y, r, g, b, a);
                if (r == t_r &&
                    g == t_g &&
                    b == t_b) {
                    putPixel(x, y, r, g, b, IM_TRANSPARENT);
                }
            }
        }
    }
}

/**
 * Sets the color of a single pixel.
 */
void Image::putPixel(int x, int y, int r, int g, int b, int a) {
    putPixelIndex(x, y, SDL_MapRGBA(surface->format, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a));
}

/**
 * Sets the palette index of a single pixel.  If the image is in
 * indexed mode, then the index is simply the palette entry number.
 * If the image is RGB, it is a packed RGB triplet.
 */
void Image::putPixelIndex(int x, int y, unsigned int index) {
    int bpp;
    Uint8 *p;

    bpp = surface->format->BytesPerPixel;
    p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = index;
        break;

    case 2:
        *(Uint16 *)p = index;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (index >> 16) & 0xff;
            p[1] = (index >> 8) & 0xff;
            p[2] = index & 0xff;
        } else {
            p[0] = index & 0xff;
            p[1] = (index >> 8) & 0xff;
            p[2] = (index >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = index;
        break;
    }
}

/**
 * Sets the color of a "U4" scale pixel, which may be more than one
 * actual pixel.
 */
void Image::putPixelScaled(int x, int y, int r, int g, int b, int a) {
    int xs, ys;

    for (xs = 0; xs < scale; xs++) {
        for (ys = 0; ys < scale; ys++) {
            putPixel(x * scale + xs, y * scale + ys, r, g, b, a);
        }
    }
}

/**
 * Fills a rectangle in the image with a given color.
 */
void Image::fillRect(int x, int y, int w, int h, int r, int g, int b) {
    SDL_Rect dest;
    Uint32 pixel;

    pixel = SDL_MapRGB(surface->format, (Uint8)r, (Uint8)g, (Uint8)b);
    dest.x = x;
    dest.y = y;
    dest.w = w;
    dest.h = h;

    SDL_FillRect(surface, &dest, pixel);
}

/**
 * Gets the color of a single pixel.
 */
void Image::getPixel(int x, int y, unsigned int &r, unsigned int &g, unsigned int &b, unsigned int &a) const {
    unsigned int index;
    Uint8 r1, g1, b1, a1;

    getPixelIndex(x, y, index);

    SDL_GetRGBA(index, surface->format, &r1, &g1, &b1, &a1);
    r = r1;
    g = g1;
    b = b1;
    a = a1;
}

/**
 * Gets the palette index of a single pixel.  If the image is in
 * indexed mode, then the index is simply the palette entry number.
 * If the image is RGB, it is a packed RGB triplet.
 */
void Image::getPixelIndex(int x, int y, unsigned int &index) const {
    int bpp = surface->format->BytesPerPixel;

    Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        index = *p;
        break;

    case 2:
        index = *(Uint16 *)p;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
            index = p[0] << 16 | p[1] << 8 | p[2];
        else
            index = p[0] | p[1] << 8 | p[2] << 16;
        break;

    case 4:
        index = *(Uint32 *)p;

    default:
        return;
    }
}

/**
 * Draws the entire image onto the screen at the given offset.
 */
void Image::draw(int x, int y) const {
    SDL_Rect r;

    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    SDL_BlitSurface(surface, NULL, SDL_GetVideoSurface(), &r);
}

/**
 * Draws a piece of the image onto the screen at the given offset.
 * The area of the image to draw is defined by the rectangle rx, ry,
 * rw, rh.
 */
void Image::drawSubRect(int x, int y, int rx, int ry, int rw, int rh) const {
    SDL_Rect src, dest;

    src.x = rx;
    src.y = ry;
    src.w = rw;
    src.h = rh;

    dest.x = x;
    dest.y = y;
    /* dest w & h unused */

    SDL_BlitSurface(surface, &src, SDL_GetVideoSurface(), &dest);
}

/**
 * Draws a piece of the image onto the screen at the given offset, inverted.
 * The area of the image to draw is defined by the rectangle rx, ry,
 * rw, rh.
 */
void Image::drawSubRectInverted(int x, int y, int rx, int ry, int rw, int rh) const {
    int i;
    SDL_Rect src, dest;

    for (i = 0; i < rh; i++) {
        src.x = rx;
        src.y = ry + i;
        src.w = rw;
        src.h = 1;

        dest.x = x;
        dest.y = y + rh - i - 1;
        /* dest w & h unused */

        SDL_BlitSurface(surface, &src, SDL_GetVideoSurface(), &dest);
    }
}