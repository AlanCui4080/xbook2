#ifndef _LIB_XTK_SPIRIT_H
#define _LIB_XTK_SPIRIT_H

#include <uview.h>
#include "xtk_image.h"

typedef unsigned int xtk_color_t;

typedef enum {
    XTK_ALIGN_LEFT = 0,
    XTK_ALIGN_RIGHT,
    XTK_ALIGN_TOP,
    XTK_ALIGN_BOTTOM,
    XTK_ALIGN_CENTER,
} xtk_align_t;

typedef struct {
    /* border */
    xtk_color_t border_color;
    /* background */
    xtk_color_t background_color;
    xtk_align_t background_align;
    /* font */
    xtk_color_t color;
    xtk_align_t align;
} xtk_style_t;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    xtk_image_t *background_image;
    char *text;
    xtk_style_t style;
} xtk_spirit_t;

xtk_spirit_t *xtk_spirit_create(int x, int y, int width, int height);
int xtk_spirit_destroy(xtk_spirit_t *spilit);
int xtk_spirit_set_pos(xtk_spirit_t *spilit, int x, int y);
int xtk_spirit_set_size(xtk_spirit_t *spilit, int width, int height);
int xtk_spirit_set_text(xtk_spirit_t *spilit, char *text);
int xtk_spirit_auto(xtk_spirit_t *spilit);
int xtk_spirit_set_background_image(xtk_spirit_t *spilit, char *filename);
int xtk_spirit_to_bitmap(xtk_spirit_t *spilit, uview_bitmap_t *bmp);

#endif /* _LIB_XTK_SPIRIT_H */