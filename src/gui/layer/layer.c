#include <string.h>
#include <unistd.h>
#include <xbook/kmalloc.h>
#include <xbook/debug.h>
#include <xbook/msgpool.h>
#include <xbook/task.h>

#include <arch/page.h>

#include <gui/layer.h>
#include <gui/screen.h>
#include <gui/color.h>
#include <gui/draw.h>
#include <gui/mouse.h>

#define DEBUG_LOCAL 0

LIST_HEAD(layer_list_head);
LIST_HEAD(layer_show_list_head);
int top_layer_z = -1;    /* 顶层图层的Z轴 */
layer_t *layer_topest = NULL;   /* topest layer */
layer_t *layer_focused = NULL;   /* focused layer */
uint16_t *layer_map = NULL; /* layer z map */
int layer_next_id = 0;  /* next layer id */

/* window top layer, keep window as topest */
int layer_win_top = 0;  

/**
 * create_layer - 创建一个图层
 * @width: 图层宽度
 * @height: 图层高度
 * 
 * @成功返回图层的指针，失败返回NULL
 */
layer_t *create_layer(int width, int height)
{
    size_t bufsz = width * height * sizeof(GUI_COLOR);
    bufsz += PAGE_SIZE;
    GUI_COLOR *buffer = kmalloc(bufsz);
    if (buffer == NULL)
        return NULL;

    memset(buffer, 0, width * height * sizeof(GUI_COLOR));
    layer_t *layer = kmalloc(sizeof(layer_t));

    if (layer == NULL) {
        kfree(buffer);
        return NULL;
    }

    memset(layer, 0, sizeof(layer_t));
    layer->id = layer_next_id;
    layer_next_id++;
    layer->buffer = buffer;
    layer->width = width;
    layer->height = height;
    layer->z = -1;          /* 不显示的图层 */
    layer->extension = NULL;
    INIT_LIST_HEAD(&layer->list);

    /* 添加到链表末尾 */
    list_add_tail(&layer->global_list, &layer_list_head);

    INIT_LIST_HEAD(&layer->widget_list_head);

    return layer;
}

layer_t *layer_find_by_id(int id)
{
    layer_t *l;
    list_for_each_owner (l, &layer_list_head, global_list) {
        if (l->id == id)
            return l;
    }
    return NULL;
}

static void layer_refresh_map(int left, int top, int right, int buttom, int z0)
{
    /* 在图层中的位置 */
    int layer_left, layer_top, layer_right, layer_buttom;

    /* 在屏幕上的位置 */
    int screen_x, screen_y;

    /* 在图层上的位置 */
    int layer_x, layer_y;

    /* 修复需要刷新的区域 */
    if (left < 0)
        left = 0;
	if (top < 0)
        top = 0;
	if (right > gui_screen.width)
        right = gui_screen.width;
	if (buttom > gui_screen.height)
        buttom = gui_screen.height;
    
    layer_t *layer;
    GUI_COLOR color;
    /* 刷新高度为[Z0-top]区间的图层 */
    list_for_each_owner (layer, &layer_show_list_head, list) {
        if (layer->z >= z0) {
            /* 获取刷新范围 */
            layer_left = left - layer->x;
            layer_top = top - layer->y;
            layer_right = right - layer->x;
            layer_buttom = buttom - layer->y;
            /* 修复范围 */
            if (layer_left < 0)
                layer_left = 0;
            if (layer_top < 0)
                layer_top = 0;
            if (layer_right > layer->width) 
                layer_right = layer->width;
            if (layer_buttom > layer->height)
                layer_buttom = layer->height;
            
            for(layer_y = layer_top; layer_y < layer_buttom; layer_y++){
                screen_y = layer->y + layer_y;
                for(layer_x = layer_left; layer_x < layer_right; layer_x++){
                    screen_x = layer->x + layer_x;
                    /* 获取图层中的颜色 */
                    color = layer->buffer[layer_y * layer->width + layer_x];
                    if (color & 0xff000000) {   /* 不是全透明的，就把高度写入到地图中 */
                        layer_map[(screen_y * gui_screen.width + screen_x)] = layer->z;
                    }
                    /* 写入到显存 */
                    //gui_screen.output_pixel(screen_x, screen_y, gui_screen.gui_to_screen_color(color));
                }
            }
        }
    }
}

/**
 * layer_set_z - 设置图层的Z轴深度
 * @layer: 图层
 * @z: z轴
 * 
 * Z轴轴序：
 *      可显示图层的轴序从0开始排序，一直递增。
 *      设置图层Z轴时，如果小于0，就把图层隐藏。
 *      如果图层没有存在于链表中，就是插入一个新图层。
 *      如果图层已经在链表中，那么就是调整一个图层的位置。
 *      如果设置图层小于0，为负，那么就是要隐藏图层
 * 
 * 调整Z序时需要刷新图层：
 *      当调整到更低的图层时，如果是隐藏图层，那么就刷新最底层到原图层Z的下一层。
 *      不是隐藏图层的话，刷新当前图层上一层到原来图层。
 *      当调整到更高的图层时，就只刷新新图层的高度那一层。
 * @return: 成功返回0，失败返回-1
 */
void layer_set_z(layer_t *layer, int z)
{
    layer_t *tmp = NULL;
    layer_t *old_layer = NULL;
    int old_z = layer->z;                
    /* 已经存在与现实链表中，就说明只是调整一下高度而已。 */
    if (list_find(&layer->list, &layer_show_list_head)) {
#if DEBUG_LOCAL == 1    
        printk("layer z:%d set new z:%d\n", layer->z, z);
#endif
        /* 设置为正，就是要调整高度 */
        if (z >= 0) {
            /* 修复Z轴 */
            if (z > top_layer_z) {
#if DEBUG_LOCAL == 1                
                printk("layer z:%d set new z:%d but above top %d\n", layer->z, z, top_layer_z);
#endif        
                z = top_layer_z;
                
            }
            /* 如果要调整到最高的图层位置 */
            if (z == top_layer_z) {
#if DEBUG_LOCAL == 1                
                printk("layer z:%d set new z:%d same with top %d\n", layer->z, z, top_layer_z);
#endif
                /* 先从链表中移除 */
                list_del_init(&layer->list);

                /* 把图层后面的图层往下面降 */
                list_for_each_owner (tmp, &layer_show_list_head, list) {
                    if (tmp->z > layer->z) {
                        tmp->z--;
                    }
                }
                layer->z = z;
                /* 添加到链表末尾 */
                list_add_tail(&layer->list, &layer_show_list_head);

                /* 刷新新图层[z, z] */
                layer_refresh_map(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z);
                layer_refresh_by_z(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z, z);

            } else {    /* 不是最高图层，那么就和其它图层交换 */
                
                if (z > layer->z) { /* 如果新高度比原来的高度高 */
#if DEBUG_LOCAL == 1
                    printk("layer z:%d < new z:%d \n", layer->z, z, top_layer_z);
#endif
                    /* 先从链表中移除 */
                    list_del_init(&layer->list);

                    /* 把位于旧图层高度和新图层高度之间（不包括旧图层，但包括新图层高度）的图层下降1层 */
                    list_for_each_owner (tmp, &layer_show_list_head, list) {
                        if (tmp->z > layer->z && tmp->z <= z) {
                            if (tmp->z == z) {  /* 记录原来为与新图层这个位置的图层 */
                                old_layer = tmp;
                            }
                            tmp->z--; /* 等上一步判断图层高度后，再减小图层高度 */
                        }
                    }
                    /* 添加到新图层高度的位置 */
                    if (old_layer) {
#if DEBUG_LOCAL == 1
                        printk("find old layer:%x z%d\n", old_layer, old_layer->z + 1);
#endif                        
                        layer->z = z;
                        list_add_after(&layer->list, &old_layer->list);

                        /* 刷新新图层[z, z] */
                        layer_refresh_map(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z);
                        layer_refresh_by_z(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z, z);
                
                    } else {
                        printk("[error ] not found the old layer on %d\n", z);
                    }
                } else if (z < layer->z) { /* 如果新高度比原来的高度低 */
#if DEBUG_LOCAL == 1                
                    printk("layer z:%d > new z:%d \n", layer->z, z, top_layer_z);
#endif
                    /* 先从链表中移除 */
                    list_del_init(&layer->list);
                        
                    /* 把位于旧图层高度和新图层高度之间（不包括旧图层，但包括新图层高度）的图层上升1层 */
                    list_for_each_owner (tmp, &layer_show_list_head, list) {
                        if (tmp->z < layer->z && tmp->z >= z) {
                            if (tmp->z == z) {  /* 记录原来为与新图层这个位置的图层 */
                                old_layer = tmp;
                            }
                            tmp->z++; /* 等上一步判断图层高度后，再增加图层高度 */
                        }
                    }
                    /* 添加到新图层高度的位置 */
                    if (old_layer) {
#if DEBUG_LOCAL == 1
                        printk("find old layer:%x z%d\n", old_layer, old_layer->z - 1);
#endif                        
                        layer->z = z;
                        list_add_before(&layer->list, &old_layer->list);

                        /* 刷新新图层[z + 1, old z] */
                        layer_refresh_map(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z + 1);
                        layer_refresh_by_z(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z + 1, old_z);
                    } else {
                        printk("[error ] not found the old layer on %d\n", z);
                    }
                }
            }
        } else { /* 小于0就是要隐藏起来的图层 */
#if DEBUG_LOCAL == 1
            printk("layer z:%d will be hided.\n", layer->z);
#endif
            /* 先从链表中移除 */
            list_del_init(&layer->list);
            if (top_layer_z > old_z) {  /* 旧图层必须在顶图层下面 */
                /* 把位于当前图层后面的图层的高度都向下降1 */
                list_for_each_owner (tmp, &layer_show_list_head, list) {
                    if (tmp->z > layer->z) {
                        tmp->z--;
                    }
                }   
            }
            
            /* 由于隐藏了一个图层，那么，图层顶层的高度就需要减1 */
            top_layer_z--;

            /* 刷新图层, [0, layer->z - 1] */
            layer_refresh_map(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, 0);
            layer_refresh_by_z(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, 0, old_z - 1);

            layer->z = -1;  /* 隐藏图层后，高度变为-1 */
        }
    } else {    /* 插入新图层 */
        /* 设置为正，就要显示，那么会添加到显示队列 */
        if (z >= 0) {
            /* 修复Z轴 */
            if (z > top_layer_z) {
                top_layer_z++;      /* 图层顶增加 */
                z = top_layer_z;
#if DEBUG_LOCAL == 1
                printk("insert a layer at top z %d\n", z);
#endif
            } else {
                top_layer_z++;      /* 图层顶增加 */
            }

            /* 如果新高度就是最高的图层，就直接插入到图层队列末尾 */
            if (z == top_layer_z) {
#if DEBUG_LOCAL == 1
                printk("add a layer %d to tail\n", z);
#endif
                layer->z = z;
                /* 直接添加到显示队列 */
                list_add_tail(&layer->list, &layer_show_list_head);

                /* 刷新新图层[z, z] */
                layer_refresh_map(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z);
                layer_refresh_by_z(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z, z);

            } else {
#if DEBUG_LOCAL == 1
                printk("add a layer %d to midlle or head\n", z);
#endif                
                /* 查找和当前图层一样高度的图层 */
                list_for_each_owner(tmp, &layer_show_list_head, list) {
                    if (tmp->z == z) {
                        old_layer = tmp;
                        break;
                    }
                }
                tmp = NULL;
                if (old_layer) {    /* 找到一个旧图层 */
                    /* 把后面的图层的高度都+1 */
                    list_for_each_owner(tmp, &layer_show_list_head, list) {
                        if (tmp->z >= old_layer->z) { 
                            tmp->z++;
                        }
                    }
                    
                    layer->z = z;
                    /* 插入到旧图层前面 */
                    list_add_before(&layer->list, &old_layer->list);
                        
                } else {    /* 没有找到旧图层 */
                    printk("[error ] not found old layer!\n");
                }
                /* 刷新新图层[z, z] */
                layer_refresh_map(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z);
                layer_refresh_by_z(layer->x, layer->y, layer->x + layer->width, layer->y + layer->height, z, z);

            }
        }
        /* 小于0就是要隐藏起来的图层，但是由于不在图层链表中，就不处理 */
    }
}

/**
 * destroy_layer - 销毁图层
 * @layer: 图层
 * 
 * @成功返回0，失败返回-1
 */
int destroy_layer(layer_t *layer)
{
    if (layer == NULL)
        return -1;
    /* 先从链表中删除 */
    list_del_init(&layer->global_list);
    /* 释放缓冲区 */
    kfree(layer->buffer);
    /* 释放图层 */
    kfree(layer);
    return 0;
}

/**
 * print_layers - 打印所有图层信息
 * 
 */
void print_layers()
{
#if DEBUG_LOCAL == 1
    printk("layer top z:%d\n", top_layer_z);
#endif
    layer_t *layer;
    list_for_each_owner (layer, &layer_show_list_head, list) {
#if DEBUG_LOCAL == 1
        printk("layer addr:%x buffer:%x width:%d height:%d x:%d y:%d z:%d\n",
            layer, layer->buffer, layer->width, layer->height, layer->x, layer->y, layer->z);
#endif
    }
}

/**
 * layer_refresh_by_z - 刷新图层
 * 
 * 刷新某个区域的[z0-z1]之间的图层，相当于在1一个3d空间中刷新
 * 某个矩形区域。（有点儿抽象哦，铁汁~）
 * 
 */
void layer_refresh_by_z(int left, int top, int right, int buttom, int z0, int z1)
{
    /* 在图层中的位置 */
    int layer_left, layer_top, layer_right, layer_buttom;

    /* 在屏幕上的位置 */
    int screen_x, screen_y;

    /* 在图层上的位置 */
    int layer_x, layer_y;

    /* 修复需要刷新的区域 */
    if (left < 0)
        left = 0;
	if (top < 0)
        top = 0;
	if (right > gui_screen.width)
        right = gui_screen.width;
	if (buttom > gui_screen.height)
        buttom = gui_screen.height;
    
    layer_t *layer;
    GUI_COLOR color;
    /* 刷新高度为[Z0-Z1]区间的图层 */
    list_for_each_owner (layer, &layer_show_list_head, list) {
        if (layer->z >= z0 && layer->z <= z1) {
            /* 获取刷新范围 */
            layer_left = left - layer->x;
            layer_top = top - layer->y;
            layer_right = right - layer->x;
            layer_buttom = buttom - layer->y;
            /* 修复范围 */
            if (layer_left < 0)
                layer_left = 0;
            if (layer_top < 0)
                layer_top = 0;
            if (layer_right > layer->width) 
                layer_right = layer->width;
            if (layer_buttom > layer->height)
                layer_buttom = layer->height;
            
            for(layer_y = layer_top; layer_y < layer_buttom; layer_y++){
                screen_y = layer->y + layer_y;
                for(layer_x = layer_left; layer_x < layer_right; layer_x++){
                    screen_x = layer->x + layer_x;
                    /* 照着map中的z进行刷新 */			
                    if (layer_map[(screen_y * gui_screen.width + screen_x)] == layer->z) {
                        /* 获取图层中的颜色 */
                        color = layer->buffer[layer_y * layer->width + layer_x];
                        /* 写入到显存 */
                        gui_screen.output_pixel(screen_x, screen_y, gui_screen.gui_to_screen_color(color));
                    }
                }
            }
        }
    }
}


/**
 * layer_refresh - 刷新图层
 * 
 * 刷新某个区域的图层
 * 
 */
void layer_refresh(layer_t *layer, int left, int top, int right, int buttom)
{
    if (layer->z >= 0) {
        layer_refresh_map(layer->x + left, layer->y + top, layer->x + right,
            layer->y + buttom, layer->z);
        layer_refresh_by_z(layer->x + left, layer->y + top, layer->x + right,
            layer->y + buttom, layer->z, layer->z);
    }
}

void layer_refresh_all()
{
    /* 从底刷新到顶 */
    layer_t *layer;
    list_for_each_owner (layer, &layer_show_list_head, list) {
        layer_refresh(layer, 0, 0, layer->width, layer->height);
    }
}

/**
 * layer_set_xy - 设置图层的位置
 * @layer: 图层
 * @x: 横坐标
 * @y: 纵坐标
 */
void layer_set_xy(layer_t *layer, int x, int y)
{
    int old_x = layer->x;
    int old_y = layer->y;
    /* 显示中的图层才可以刷新 */
    layer->x = x;
    layer->y = y;
    if (layer->z >= 0) {
        /* 刷新原来位置 */
        layer_refresh_map(old_x, old_y, old_x + layer->width, old_y + layer->height, 0);
        /* 刷新新位置 */
        layer_refresh_map(x, y, x + layer->width, y + layer->height, layer->z);

        /* 刷新原来位置 */
        layer_refresh_by_z(old_x, old_y, old_x + layer->width, old_y + layer->height, 0, layer->z - 1);
        /* 刷新新位置 */
        layer_refresh_by_z(x, y, x + layer->width, y + layer->height, layer->z, layer->z);
    }
}

layer_t *layer_get_by_z(int z)
{
    if (z > top_layer_z || z < 0) {
        return NULL;
    }
    layer_t *layer;
    list_for_each_owner (layer, &layer_show_list_head, list) {
        if (layer->z == z)
            return layer;
    }
    return NULL;
}
int layer_get_win_top()
{
    return layer_win_top;
}

int layer_set_win_top(int top)
{
    layer_win_top = top;
    return 0;
}

void layer_set_focus(layer_t *layer)
{
    layer_focused = layer;
}

layer_t *layer_get_focus()
{
    return layer_focused;
}

int sys_layer_set_focus(int ly)
{
    printk("[gui]: set layer focus %d\n", ly);
    layer_t *layer = layer_find_by_id(ly);
    if (layer == NULL)
        return -1;
    
    layer_set_focus(layer);
    return 0;
}

int sys_layer_get_focus()
{
    return layer_get_focus()->id;
}

int gui_dispatch_mouse_msg(g_msg_t *msg)
{
    layer_t *layer;
    int local_mx, local_my;
    int val = -1;
    /* 查看点击的位置，看是否是一个 */
    list_for_each_owner_reverse (layer, &layer_show_list_head, list) {
        if (layer == layer_topest)
            continue;
        if (layer->extension == NULL)
            continue;
        local_mx = msg->data0 - layer->x;
        local_my = msg->data1 - layer->y;
        if (local_mx >= 0 && local_mx < layer->width && 
            local_my >= 0 && local_my < layer->height) {
            
            /* 发送消息 */
            g_msg_t m;
            memset(&m, 0, sizeof(g_msg_t));
            m.id        = msg->id;
            m.target    = layer->id;
            m.data0     = local_mx;
            m.data1     = local_my;
            m.data2     = msg->data0;
            m.data3     = msg->data1;
            
            task_t *task = (task_t *)layer->extension;
            val = msgpool_try_push(task->gmsgpool, &m);
            break;
        }
    }

    return val;
}

int gui_dispatch_key_msg(g_msg_t *msg)
{
    layer_t *layer = layer_focused;
    int val = -1;
    /* 发送给聚焦图层 */
    if (layer) {
        if (layer->extension == NULL)
            return -1;
        
        /* 发送消息 */
        g_msg_t m;
        memset(&m, 0, sizeof(g_msg_t));
        m.id        = msg->id;
        m.target    = layer->id;
        m.data0     = msg->data0;
        m.data1     = msg->data1;
        task_t *task = (task_t *)layer->extension;
        val = msgpool_try_push(task->gmsgpool, &m);
    }
    return val;
}

int gui_init_layer()
{
    /* 分配地图空间 */
    layer_map = kmalloc(gui_screen.width * gui_screen.height * sizeof(uint16_t));
    if (layer_map == NULL) {
        return -1;
    }
    memset(layer_map, 0, gui_screen.width * gui_screen.height * sizeof(uint16_t));

    INIT_LIST_HEAD(&layer_show_list_head);
    INIT_LIST_HEAD(&layer_list_head);

    layer_topest = NULL;
    layer_focused = NULL;
    if (init_mouse_layer() < 0) {
        kfree(layer_map);
        return -1;
    }

    /* 桌面窗口以及系统窗口之类的都在用户态去实现 */
    /*layer_t *layer = create_layer(gui_screen.width, gui_screen.height);
    if (layer) {
        layer_set_z(layer, 0);
        layer_draw_rect_fill(layer, 0,0,gui_screen.width, gui_screen.height, COLOR_GREEN);
        layer_flush(layer);
    }*/
    //spin("gui");

    return 0;
}
