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

#pragma pack(pop)

#endif
