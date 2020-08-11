#ifndef __GUISRV_DRIVER_SCREEN_H__
#define __GUISRV_DRIVER_SCREEN_H__

#include <gui/color.h>

typedef struct {
    int width;  
    int height;
    
    /* 颜色转换 */
    SCREEN_COLOR   (*gui_to_screen_color)(GUI_COLOR  gui_color);
    GUI_COLOR      (*screen_to_gui_color)(SCREEN_COLOR  screen_color);
    
    int            (*open)(void);
    int	           (*close)(void);

    int            (*output_pixel)(int x, int y, SCREEN_COLOR  color);
    int            (*input_pixel)(int x, int y, SCREEN_COLOR  *color);
    
    int            (*output_hline)(int left, int right, int top, SCREEN_COLOR  color);
    int            (*output_vline)(int left, int top, int bottom, SCREEN_COLOR  color);
    int            (*output_rect_fill)(int left, int top, int right, int bottom, SCREEN_COLOR  color);

} gui_screen_t;

extern gui_screen_t gui_screen;

int gui_init_screen();



#endif  /* __GUISRV_DRIVER_SCREEN_H__ */
