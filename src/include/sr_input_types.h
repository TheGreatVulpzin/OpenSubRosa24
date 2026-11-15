/* Mouse state and camera controls. */

#ifndef SR_INPUT_TYPES_H

#define SR_INPUT_TYPES_H

#include "sr_types.h"

#pragma pack(push, 1)

/* input_mouse[]: record of 0x2c bytes. */
typedef union InputMouse { struct {
    int x;                               /* +0x0000 Absolute horizontal coordinate on a 1024 pixel canvas */
    int y;                               /* +0x0004 Absolute vertical coordinate on a 640 pixel canvas */
    int dx;                              /* +0x0008 Relative horizontal movement */
    int dy;                              /* +0x000c Relative vertical movement */
    int left;                            /* +0x0010 Left button state */
    int right;                           /* +0x0014 Right button state */
    int left_released;                   /* +0x0018 Left button release transition */
    int right_released;                  /* +0x001c Right button release transition */
    int wheel_up;                        /* +0x0020 Positive upward wheel events */
    int wheel_down;                      /* +0x0024 Positive downward wheel events */
    uint32_t buttons;                    /* +0x0028 Low eight mouse button bits */
}; uint8_t lf_raw[0x2c]; } InputMouse;
SR_STATIC_ASSERT(sizeof(InputMouse) == 0x2c, "InputMouse");
SR_STATIC_ASSERT(offsetof(InputMouse, x) == 0x0, "InputMouse.x");
SR_STATIC_ASSERT(offsetof(InputMouse, y) == 0x4, "InputMouse.y");
SR_STATIC_ASSERT(offsetof(InputMouse, dx) == 0x8, "InputMouse.dx");
SR_STATIC_ASSERT(offsetof(InputMouse, dy) == 0xc, "InputMouse.dy");
SR_STATIC_ASSERT(offsetof(InputMouse, left) == 0x10, "InputMouse.left");
SR_STATIC_ASSERT(offsetof(InputMouse, right) == 0x14, "InputMouse.right");
SR_STATIC_ASSERT(offsetof(InputMouse, left_released) == 0x18, "InputMouse.left_released");
SR_STATIC_ASSERT(offsetof(InputMouse, right_released) == 0x1c, "InputMouse.right_released");
SR_STATIC_ASSERT(offsetof(InputMouse, wheel_up) == 0x20, "InputMouse.wheel_up");
SR_STATIC_ASSERT(offsetof(InputMouse, wheel_down) == 0x24, "InputMouse.wheel_down");
SR_STATIC_ASSERT(offsetof(InputMouse, buttons) == 0x28, "InputMouse.buttons");

/* input_camera[]: record of 0x1c bytes. */
typedef union InputCamera { struct {
    vec3 pos;                            /* +0x0000 Camera translation */
    float yaw;                           /* +0x000c Horizontal camera angle */
    float pitch;                         /* +0x0010 Vertical camera angle */
    float zoom;                          /* +0x0014 Smoothed camera zoom or distance */
    float zoom_target;                   /* +0x0018 Requested camera zoom or distance */
}; uint8_t lf_raw[0x1c]; } InputCamera;
SR_STATIC_ASSERT(sizeof(InputCamera) == 0x1c, "InputCamera");
SR_STATIC_ASSERT(offsetof(InputCamera, pos) == 0x0, "InputCamera.pos");
SR_STATIC_ASSERT(offsetof(InputCamera, yaw) == 0xc, "InputCamera.yaw");
SR_STATIC_ASSERT(offsetof(InputCamera, pitch) == 0x10, "InputCamera.pitch");
SR_STATIC_ASSERT(offsetof(InputCamera, zoom) == 0x14, "InputCamera.zoom");
SR_STATIC_ASSERT(offsetof(InputCamera, zoom_target) == 0x18, "InputCamera.zoom_target");

/* menu_info[]: record of 0x60 bytes. */
typedef union MenuInfo { struct {
    char text[64];                       /* +0x0000 setmenuiteminfo copies the text of an info line of a menu item */
    float x;                             /* +0x0040 drawmenuitems draws the info text at x and y */
    float y;                             /* +0x0044 drawmenuitems draws the info text at x and y */
    float size;                          /* +0x0048 drawmenuitems text size */
    uint32_t flags;                      /* +0x004c drawmenuitems passes the flags with 0x60 to drawtext */
    float color[4];                      /* +0x0050 drawmenuitems red green blue alpha of the text */
}; uint8_t lf_raw[0x60]; } MenuInfo;
SR_STATIC_ASSERT(sizeof(MenuInfo) == 0x60, "MenuInfo");
SR_STATIC_ASSERT(offsetof(MenuInfo, text) == 0x0, "MenuInfo.text");
SR_STATIC_ASSERT(offsetof(MenuInfo, x) == 0x40, "MenuInfo.x");
SR_STATIC_ASSERT(offsetof(MenuInfo, y) == 0x44, "MenuInfo.y");
SR_STATIC_ASSERT(offsetof(MenuInfo, size) == 0x48, "MenuInfo.size");
SR_STATIC_ASSERT(offsetof(MenuInfo, flags) == 0x4c, "MenuInfo.flags");
SR_STATIC_ASSERT(offsetof(MenuInfo, color) == 0x50, "MenuInfo.color");

/* menu_item[]: record of 0x284 bytes. */
typedef union MenuItem { struct {
    int kind;                            /* +0x0000 checkmenuitems and drawmenuitems: 0 button; 1 and 2 text field; 3 bit toggle; 4 choice; 5 slider */
    char text[64];                       /* +0x0004 createmenuitem copies the label; createmenuitemempty clears 0x40 bytes */
    float label_x;                       /* +0x0044 createmenuitem and createmenubutton: x of the label */
    float label_y;                       /* +0x0048 createmenuitem and createmenubutton: y of the label */
    float label_size;                    /* +0x004c createmenuitem and createmenubutton: text size of the label */
    uint32_t label_flags;                /* +0x0050 drawmenuitems passes it with 0x60 to drawtext; createmenubutton writes 5 */
    int num_info;                        /* +0x0054 setmenuiteminfo adds up to four info lines */
    MenuInfo info[4];                    /* +0x0058 setmenuiteminfo fills info[num_info] */
    float x;                             /* +0x01d8 checkmenuitems tests the mouse from x to x + width */
    float y;                             /* +0x01dc checkmenuitems tests the mouse from y to y + height */
    float text_size;                     /* +0x01e0 drawmenuitems text size of a field */
    uint32_t text_flags;                 /* +0x01e4 createmenubutton align flags; drawmenuitems passes them to drawbitmap and drawtext */
    float color[4];                      /* +0x01e8 createmenuitem and createmenubutton red green blue alpha; setmenuitem option 20 halves red green blue */
    float width;                         /* +0x01f8 checkmenuitems hit box width */
    float height;                        /* +0x01fc checkmenuitems hit box height */
    int key;                             /* +0x0200 checkmenuitems fires the item when this key goes down */
    int key2;                            /* +0x0204 checkmenuitems: when not 0 this key must be down too */
    int repeat;                          /* +0x0208 checkmenuitems fires while the key stays down and clears pressed first */
    int disabled;                        /* +0x020c checkmenuitems: no hover and no key */
    int hover;                           /* +0x0210 checkmenuitems: the mouse is over the item */
    int hidden;                          /* +0x0214 drawmenuitems skips the item; setmenuitem option 23 sets it */
    int lit;                             /* +0x0218 checkmenuitems: hover with a press or a set toggle or the chosen choice */
    float fade;                          /* +0x021c checkmenuitems moves it 1/8 of the way to -1 when lit and to 1 when not */
    int pressed;                         /* +0x0220 checkmenuitems sets it on a click or a key; the callback runs while it is set */
    int pressed_previous;                /* +0x0224 checkmenuitems copies pressed at the end */
    uint32_t mask;                       /* +0x0228 the bit of a toggle or the value of a choice */
    int field_type;                      /* +0x022c drawmenuitems prints the value: 1 and 2 int; 3 float; other a string */
    int length;                          /* +0x0230 createmenuitem writes strlen of the label; setmenuitem writes the size of a text field */
    int password;                        /* +0x0234 drawmenuitems shows a star for each character; setmenuitem option 17 sets it */
    int slider_min;                      /* +0x0238 drawmenuitems and menuslider */
    int slider_max;                      /* +0x023c drawmenuitems and menuslider */
    int slider_step;                     /* +0x0240 menuslider */
    int links[4];                        /* +0x0244 -1 at creation; setmenuitem option 13; drawmenuitems draws the item alone when links[0] is -1 */
    float spare[8];                      /* +0x0254 createmenuitem writes 0 1 1 1 1 0 0 0 and setmenuitem option 18 copies 0x20 bytes; no function reads it */
    int cursor;                          /* +0x0274 checkmenuitems copies it to the cursor of the font when the mouse is over the item */
    int spare_278;                       /* +0x0278 setmenuitem option 16 writes it; no function reads it */
    int* value;                          /* +0x027c the int of a toggle or a choice or a slider; the buffer of a text field */
    void* callback;                      /* +0x0280 checkmenuitems calls it while the item is pressed */
}; uint8_t lf_raw[0x284]; } MenuItem;
SR_STATIC_ASSERT(sizeof(MenuItem) == 0x284, "MenuItem");
SR_STATIC_ASSERT(offsetof(MenuItem, kind) == 0x0, "MenuItem.kind");
SR_STATIC_ASSERT(offsetof(MenuItem, text) == 0x4, "MenuItem.text");
SR_STATIC_ASSERT(offsetof(MenuItem, label_x) == 0x44, "MenuItem.label_x");
SR_STATIC_ASSERT(offsetof(MenuItem, label_y) == 0x48, "MenuItem.label_y");
SR_STATIC_ASSERT(offsetof(MenuItem, label_size) == 0x4c, "MenuItem.label_size");
SR_STATIC_ASSERT(offsetof(MenuItem, label_flags) == 0x50, "MenuItem.label_flags");
SR_STATIC_ASSERT(offsetof(MenuItem, num_info) == 0x54, "MenuItem.num_info");
SR_STATIC_ASSERT(offsetof(MenuItem, info) == 0x58, "MenuItem.info");
SR_STATIC_ASSERT(offsetof(MenuItem, x) == 0x1d8, "MenuItem.x");
SR_STATIC_ASSERT(offsetof(MenuItem, y) == 0x1dc, "MenuItem.y");
SR_STATIC_ASSERT(offsetof(MenuItem, text_size) == 0x1e0, "MenuItem.text_size");
SR_STATIC_ASSERT(offsetof(MenuItem, text_flags) == 0x1e4, "MenuItem.text_flags");
SR_STATIC_ASSERT(offsetof(MenuItem, color) == 0x1e8, "MenuItem.color");
SR_STATIC_ASSERT(offsetof(MenuItem, width) == 0x1f8, "MenuItem.width");
SR_STATIC_ASSERT(offsetof(MenuItem, height) == 0x1fc, "MenuItem.height");
SR_STATIC_ASSERT(offsetof(MenuItem, key) == 0x200, "MenuItem.key");
SR_STATIC_ASSERT(offsetof(MenuItem, key2) == 0x204, "MenuItem.key2");
SR_STATIC_ASSERT(offsetof(MenuItem, repeat) == 0x208, "MenuItem.repeat");
SR_STATIC_ASSERT(offsetof(MenuItem, disabled) == 0x20c, "MenuItem.disabled");
SR_STATIC_ASSERT(offsetof(MenuItem, hover) == 0x210, "MenuItem.hover");
SR_STATIC_ASSERT(offsetof(MenuItem, hidden) == 0x214, "MenuItem.hidden");
SR_STATIC_ASSERT(offsetof(MenuItem, lit) == 0x218, "MenuItem.lit");
SR_STATIC_ASSERT(offsetof(MenuItem, fade) == 0x21c, "MenuItem.fade");
SR_STATIC_ASSERT(offsetof(MenuItem, pressed) == 0x220, "MenuItem.pressed");
SR_STATIC_ASSERT(offsetof(MenuItem, pressed_previous) == 0x224, "MenuItem.pressed_previous");
SR_STATIC_ASSERT(offsetof(MenuItem, mask) == 0x228, "MenuItem.mask");
SR_STATIC_ASSERT(offsetof(MenuItem, field_type) == 0x22c, "MenuItem.field_type");
SR_STATIC_ASSERT(offsetof(MenuItem, length) == 0x230, "MenuItem.length");
SR_STATIC_ASSERT(offsetof(MenuItem, password) == 0x234, "MenuItem.password");
SR_STATIC_ASSERT(offsetof(MenuItem, slider_min) == 0x238, "MenuItem.slider_min");
SR_STATIC_ASSERT(offsetof(MenuItem, slider_max) == 0x23c, "MenuItem.slider_max");
SR_STATIC_ASSERT(offsetof(MenuItem, slider_step) == 0x240, "MenuItem.slider_step");
SR_STATIC_ASSERT(offsetof(MenuItem, links) == 0x244, "MenuItem.links");
SR_STATIC_ASSERT(offsetof(MenuItem, spare) == 0x254, "MenuItem.spare");
SR_STATIC_ASSERT(offsetof(MenuItem, cursor) == 0x274, "MenuItem.cursor");
SR_STATIC_ASSERT(offsetof(MenuItem, spare_278) == 0x278, "MenuItem.spare_278");
SR_STATIC_ASSERT(offsetof(MenuItem, value) == 0x27c, "MenuItem.value");
SR_STATIC_ASSERT(offsetof(MenuItem, callback) == 0x280, "MenuItem.callback");

#pragma pack(pop)

#endif
