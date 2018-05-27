/* NetHack 3.6    tileset.c    $NHDT-Date: 1501463811 2017/07/31 01:16:51 $ $NHDT-Branch: NetHack-3.6.0 $:$NHDT-Revision: 1.0 $ */
/* Copyright (c) Ray Chason, 2016. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "tileset.h"

static void FDECL(define_palette, (struct TileSetImage *));
static void FDECL(get_tile_map, (const char *));
static unsigned FDECL(gcd, (unsigned, unsigned));
static void FDECL(split_tiles, (const struct TileSetImage *));

static struct TileImage *tiles;
static unsigned num_tiles;
static struct TileImage blank_tile; /* for graceful undefined tile handling */

static boolean have_palette;
static struct Pixel palette[256];

boolean
read_tiles(filename, true_color)
const char *filename;
boolean true_color;
{
    struct TileSetImage image;

    if (!read_tile_image(&image, filename, true_color)) {
        return FALSE;
    }

    /* Save the palette if present */
    have_palette = image.indexes != NULL;
    memcpy(palette, image.palette, sizeof(palette));

    /* Set the tile dimensions if the user has not done so */
    if (iflags.wc_tile_width == 0) {
        iflags.wc_tile_width = image.tile_width;
    }
    if (iflags.wc_tile_height == 0) {
        iflags.wc_tile_height = image.tile_height;
    }

    /* Split the image into tiles */
    split_tiles(&image);

    free_tile_image(&image);
    return TRUE;
}

boolean
read_tile_image(image, filename, true_color)
struct TileSetImage *image;
const char *filename;
boolean true_color;
{
    static const unsigned char png_sig[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
    };
    FILE *fp;
    char header[16];
    boolean ok;

    /* Fill the image structure with known values */
    image->width = 0;
    image->height = 0;
    image->pixels = NULL;       /* custodial, returned */
    image->indexes = NULL;      /* custodial, returned */
    image->image_desc = NULL;   /* custodial, returned */
    image->tile_width = 0;
    image->tile_height = 0;

    if (filename == NULL || filename[0] == '\0') {
        filename = "nhtiles.bmp";
    }

    /* Identify the image type */
    fp = fopen(filename, "rb");
    if (!fp)
        goto error;
    memset(header, 0, sizeof(header));
    fread(header, 1, sizeof(header), fp);
    fclose(fp), fp = (FILE *) 0;

    /* Call the loader appropriate for the image */
    if (memcmp(header, "BM", 2) == 0) {
        ok = read_bmp_tiles(filename, image);
    } else if (memcmp(header, "GIF87a", 6) == 0
           ||  memcmp(header, "GIF89a", 6) == 0) {
        ok = read_gif_tiles(filename, image);
    } else if (memcmp(header, png_sig, sizeof(png_sig)) == 0) {
        ok = read_png_tiles(filename, image);
    } else if (memcmp(header, "/* XPM */", 9) == 0) {
        ok = read_xpm_tiles(filename, image);
    } else {
        ok = FALSE;
    }
    if (!ok)
        goto error;

    /* Provide a palette, if possible */
    define_palette(image);

    /* Reject if the interface cannot handle direct color and the image does
       not use a palette */
    if (!true_color && image->indexes == NULL)
        goto error;

    /* Parse the tile map */
    get_tile_map(image->image_desc);

    /* Set defaults for tile metadata */
    if (image->tile_width == 0) {
        image->tile_width = image->width / 40;
    }
    if (image->tile_height == 0) {
        image->tile_height = image->tile_width;
    }

    return TRUE;

error:
    if (fp)
        fclose(fp);
    free_tile_image(image);
    return FALSE;
}

static void
define_palette(image)
struct TileSetImage *image;
{
    unsigned x, y, ncolors, color;
    size_t i;

    if (image->indexes != NULL) return; /* already has a palette */

    image->indexes = (unsigned char *) alloc(image->width * image->height);

    ncolors = 0;
    i = 0;
    for (y = 0; y < image->height; ++y) {
        for (x = 0; x < image->width; ++x) {
            struct Pixel p1, p2;

            /* Find the color in the palette as it exists */
            p1 = image->pixels[i];
            for (color = 0; color < ncolors; ++color) {
                p2 = image->palette[color];
                if (p1.r == p2.r && p1.g == p2.g
                &&  p1.b == p2.b && p1.a == p2.a) {
                    break;
                }
            }
            if (color >= 256) {
                /* More than 256 unique colors */
                free(image->indexes);
                image->indexes = NULL;
                return;
            }
            if (color >= ncolors) {
                /* New color */
                image->palette[ncolors++] = p1;
            }
            image->indexes[i++] = color;
        }
    }
}

const struct Pixel *
get_palette()
{
    return have_palette ? palette : NULL;
}

/* TODO: derive tile_map from image_desc */
static void
get_tile_map(image_desc)
const char *image_desc;
{
    return;
}

void
free_tiles()
{
    unsigned i;

    if (tiles) {
        for (i = 0; i < num_tiles; ++i) {
            free((genericptr_t) tiles[i].pixels);
            free((genericptr_t) tiles[i].indexes);
        }
        free((genericptr_t) tiles), tiles = NULL;
    }
    num_tiles = 0;

    if (blank_tile.pixels)
        free((genericptr_t) blank_tile.pixels), blank_tile.pixels = NULL;
    if (blank_tile.indexes)
        free((genericptr_t) blank_tile.indexes), blank_tile.indexes = NULL;
}

void
free_tile_image(image)
struct TileSetImage *image;
{
    if (image->pixels)
        free((genericptr_t) image->pixels), image->pixels = NULL;
    if (image->indexes)
        free((genericptr_t) image->indexes), image->indexes = NULL;
    if (image->image_desc)
        free((genericptr_t) image->image_desc), image->image_desc = NULL;
}

const struct TileImage *
get_tile(tile_index)
unsigned tile_index;
{
    if (tile_index >= num_tiles)
        return &blank_tile;
    return &tiles[tile_index];
}

/* Note that any tile returned by this function must be freed */
struct TileImage *
stretch_tile(inp_tile, out_width, out_height)
const struct TileImage *inp_tile;
unsigned out_width, out_height;
{
    unsigned x_scale_inp, x_scale_out, y_scale_inp, y_scale_out;
    unsigned divisor;
    unsigned size;
    struct TileImage *out_tile;
    unsigned x_inp, y_inp, x2, y2, x_out, y_out;
    unsigned pos;

    /* Derive the scale factors */
    divisor = gcd(out_width, inp_tile->width);
    x_scale_inp = inp_tile->width / divisor;
    x_scale_out = out_width / divisor;
    divisor = gcd(out_height, inp_tile->height);
    y_scale_inp = inp_tile->height / divisor;
    y_scale_out = out_height / divisor;

    /* Derive the stretched tile */
    out_tile = (struct TileImage *) alloc(sizeof(struct TileImage));
    out_tile->width = out_width;
    out_tile->height = out_height;
    size = out_width * out_height;
    if (inp_tile->indexes != NULL) {
        out_tile->indexes = (unsigned char *) alloc(size);
    } else {
        out_tile->indexes = NULL;
    }
    out_tile->pixels = (struct Pixel *) alloc(size * sizeof(struct Pixel));
    divisor = x_scale_inp * y_scale_inp;
    for (y_out = 0; y_out < out_height; ++y_out) {
        for (x_out = 0; x_out < out_width; ++x_out) {
            unsigned r, g, b, a;

            /* Derive output pixels by blending input pixels */
            r = 0;
            g = 0;
            b = 0;
            a = 0;
            for (y2 = 0; y2 < y_scale_inp; ++y2) {
                y_inp = (y_out * y_scale_inp + y2) / y_scale_out;
                for (x2 = 0; x2 < x_scale_inp; ++x2) {
                    x_inp = (x_out * x_scale_inp + x2) / x_scale_out;
                    pos = y_inp * inp_tile->width + x_inp;
                    r += inp_tile->pixels[pos].r;
                    g += inp_tile->pixels[pos].g;
                    b += inp_tile->pixels[pos].b;
                    a += inp_tile->pixels[pos].a;
                }
            }

            pos = y_out * out_width + x_out;
            out_tile->pixels[pos].r = r / divisor;
            out_tile->pixels[pos].g = g / divisor;
            out_tile->pixels[pos].b = b / divisor;
            out_tile->pixels[pos].a = a / divisor;

            /* If the output device uses a palette, we can't blend; just pick
               a subset of the pixels */
            if (out_tile->indexes != NULL) {
                x_inp = x_out * x_scale_inp / x_scale_out;
                y_inp = y_out * y_scale_inp / y_scale_out;
                out_tile->indexes[pos] =
                        inp_tile->indexes[y_inp * inp_tile->width + x_inp];
            }
        }
    }
    return out_tile;
}

/* Free a tile returned by stretch_tile */
/* Do NOT use this with tiles returned by get_tile */
void
free_tile(tile)
struct TileImage *tile;
{
    if (tile != NULL) {
        free(tile->indexes);
        free(tile->pixels);
        free(tile);
    }
}

/* Return the greatest common divisor */
static unsigned
gcd(a, b)
unsigned a, b;
{
    while (TRUE) {
        if (b == 0) return a;
        a %= b;
        if (a == 0) return b;
        b %= a;
    }
}

static void
split_tiles(image)
const struct TileSetImage *image;
{
    unsigned tile_rows, tile_cols;
    size_t tile_size, i, j;
    unsigned x1, y1, x2, y2;

    /* Get the number of tiles */
    tile_rows = image->height / image->tile_height;
    tile_cols = image->width / image->tile_width;
    num_tiles = tile_rows * tile_cols;
    tile_size = (size_t) image->tile_height * (size_t) image->tile_width;

    /* Allocate the tile array */
    tiles = (struct TileImage *) alloc(num_tiles * sizeof(tiles[0]));
    memset(tiles, 0, num_tiles * sizeof(tiles[0]));

    /* Copy the pixels into the tile structures */
    for (y1 = 0; y1 < tile_rows; ++y1) {
        for (x1 = 0; x1 < tile_cols; ++x1) {
            struct TileImage *tile = &tiles[y1 * tile_cols + x1];

            tile->width = image->tile_width;
            tile->height = image->tile_height;
            tile->pixels = (struct Pixel *)
                    alloc(tile_size * sizeof (struct Pixel));
            if (image->indexes != NULL) {
                tile->indexes = (unsigned char *) alloc(tile_size);
            }
            for (y2 = 0; y2 < image->tile_height; ++y2) {
                for (x2 = 0; x2 < image->tile_width; ++x2) {
                    unsigned x = x1 * image->tile_width + x2;
                    unsigned y = y1 * image->tile_height + y2;

                    i = y * image->width + x;
                    j = y2 * tile->width + x2;
                    tile->pixels[j] = image->pixels[i];
                    if (image->indexes != NULL) {
                        tile->indexes[j] = image->indexes[i];
                    }
                }
            }
        }
    }

    /* Create a blank tile for use when the tile index is invalid */
    blank_tile.width = image->tile_width;
    blank_tile.height = image->tile_height;
    blank_tile.pixels = (struct Pixel *)
            alloc(tile_size * sizeof (struct Pixel));
    for (i = 0; i < tile_size; ++i) {
        blank_tile.pixels[i].r = 0;
        blank_tile.pixels[i].g = 0;
        blank_tile.pixels[i].b = 0;
        blank_tile.pixels[i].a = 255;
    }
    if (image->indexes) {
        blank_tile.indexes = (unsigned char *) alloc(tile_size);
        memset(blank_tile.indexes, 0, tile_size);
    }
}
