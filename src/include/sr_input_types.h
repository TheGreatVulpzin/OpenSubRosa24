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

/* editor_view[]: record of 0x48 bytes. */
typedef union EditorView { struct {
    float x;                             /* +0x0000 buildingedit_setup and leveledit_setup: the viewport of the editor, 32 */
    float y;                             /* +0x0004 the viewport: 32 */
    float width;                         /* +0x0008 the viewport: 768 */
    float height;                        /* +0x000c the viewport: 576 */
    vec3 camera;                         /* +0x0010 the camera place: 12 0 9 for the building editor, 160 0 160 for the level editor */
    float orientation[9];                /* +0x001c the camera turns by pi/2 about its first row: it looks down */
    float zoom;                          /* +0x0040 12 for the building editor, 64 for the level editor */
    float scale;                         /* +0x0044 0.75 */
}; uint8_t lf_raw[0x48]; } EditorView;
SR_STATIC_ASSERT(sizeof(EditorView) == 0x48, "EditorView");
SR_STATIC_ASSERT(offsetof(EditorView, x) == 0x0, "EditorView.x");
SR_STATIC_ASSERT(offsetof(EditorView, y) == 0x4, "EditorView.y");
SR_STATIC_ASSERT(offsetof(EditorView, width) == 0x8, "EditorView.width");
SR_STATIC_ASSERT(offsetof(EditorView, height) == 0xc, "EditorView.height");
SR_STATIC_ASSERT(offsetof(EditorView, camera) == 0x10, "EditorView.camera");
SR_STATIC_ASSERT(offsetof(EditorView, orientation) == 0x1c, "EditorView.orientation");
SR_STATIC_ASSERT(offsetof(EditorView, zoom) == 0x40, "EditorView.zoom");
SR_STATIC_ASSERT(offsetof(EditorView, scale) == 0x44, "EditorView.scale");

/* editor_tile[]: record of 0x24 bytes. */
typedef union EditorTile { struct {
    int kind;                            /* +0x0000 the picker tiles: 0 building texture, 2 building block, 3 game texture, 4 game block */
    float x;                             /* +0x0004  */
    float y;                             /* +0x0008  */
    float width;                         /* +0x000c 64 */
    float height;                        /* +0x0010 64 */
    int* target;                         /* +0x0014 a click on the tile writes its index here */
    int start;                           /* +0x0018 building_editorloop writes 0: the first entry of the list of the tile */
    int count;                           /* +0x001c building_editorloop: the entries of the list of the tile (the material or the block names of the building) */
    int index;                           /* +0x0020 the index of the tile in its list */
}; uint8_t lf_raw[0x24]; } EditorTile;
SR_STATIC_ASSERT(sizeof(EditorTile) == 0x24, "EditorTile");
SR_STATIC_ASSERT(offsetof(EditorTile, kind) == 0x0, "EditorTile.kind");
SR_STATIC_ASSERT(offsetof(EditorTile, x) == 0x4, "EditorTile.x");
SR_STATIC_ASSERT(offsetof(EditorTile, y) == 0x8, "EditorTile.y");
SR_STATIC_ASSERT(offsetof(EditorTile, width) == 0xc, "EditorTile.width");
SR_STATIC_ASSERT(offsetof(EditorTile, height) == 0x10, "EditorTile.height");
SR_STATIC_ASSERT(offsetof(EditorTile, target) == 0x14, "EditorTile.target");
SR_STATIC_ASSERT(offsetof(EditorTile, start) == 0x18, "EditorTile.start");
SR_STATIC_ASSERT(offsetof(EditorTile, count) == 0x1c, "EditorTile.count");
SR_STATIC_ASSERT(offsetof(EditorTile, index) == 0x20, "EditorTile.index");

/* key_event[]: record of 0xc bytes. */
typedef union KeyEvent { struct {
    int scancode;                        /* +0x0000 checksystemmessages: the SDL scancode of a key press */
    int key;                             /* +0x0004 checksystemmessages: the SDL key code (the letter of the key) */
    int mod;                             /* +0x0008 checksystemmessages: the SDL modifier bits. Bits 0 and 1 are the shift keys */
}; uint8_t lf_raw[0xc]; } KeyEvent;
SR_STATIC_ASSERT(sizeof(KeyEvent) == 0xc, "KeyEvent");
SR_STATIC_ASSERT(offsetof(KeyEvent, scancode) == 0x0, "KeyEvent.scancode");
SR_STATIC_ASSERT(offsetof(KeyEvent, key) == 0x4, "KeyEvent.key");
SR_STATIC_ASSERT(offsetof(KeyEvent, mod) == 0x8, "KeyEvent.mod");

/* joystick[]: record of 0x1a4 bytes. */
typedef union Joystick { struct {
    int instance;                        /* +0x0000 setupjoysticks: the SDL instance id */
    uint8_t guid[16];                    /* +0x0004 setupjoysticks: the SDL GUID */
    int num_axes;                        /* +0x0014  */
    float axes[16];                      /* +0x0018 checkjoystick: each axis from -1 to 1 */
    int num_buttons;                     /* +0x0058  */
    int buttons[32];                     /* +0x005c checkjoystick: 1 while the button is down */
    int num_hats;                        /* +0x00dc  */
    int hats[32];                        /* +0x00e0 checkjoystick: 4 flags for each hat: up, down, left, right */
    char name[64];                       /* +0x0160 setupjoysticks: the SDL name */
    void* haptic;                        /* +0x01a0 setupjoysticks: the SDL haptic device, or 0 */
}; uint8_t lf_raw[0x1a4]; } Joystick;
SR_STATIC_ASSERT(sizeof(Joystick) == 0x1a4, "Joystick");
SR_STATIC_ASSERT(offsetof(Joystick, instance) == 0x0, "Joystick.instance");
SR_STATIC_ASSERT(offsetof(Joystick, guid) == 0x4, "Joystick.guid");
SR_STATIC_ASSERT(offsetof(Joystick, num_axes) == 0x14, "Joystick.num_axes");
SR_STATIC_ASSERT(offsetof(Joystick, axes) == 0x18, "Joystick.axes");
SR_STATIC_ASSERT(offsetof(Joystick, num_buttons) == 0x58, "Joystick.num_buttons");
SR_STATIC_ASSERT(offsetof(Joystick, buttons) == 0x5c, "Joystick.buttons");
SR_STATIC_ASSERT(offsetof(Joystick, num_hats) == 0xdc, "Joystick.num_hats");
SR_STATIC_ASSERT(offsetof(Joystick, hats) == 0xe0, "Joystick.hats");
SR_STATIC_ASSERT(offsetof(Joystick, name) == 0x160, "Joystick.name");
SR_STATIC_ASSERT(offsetof(Joystick, haptic) == 0x1a0, "Joystick.haptic");

/* building_editor[]: record of 0xec bytes. */
typedef union BuildingEditor { struct {
    int mode;                            /* +0x0000 buildingedit_render: 0 blocks, 1 walls, 2 areas, 3 markers */
    int unused_04;                       /* +0x0004 buildingedit_setup writes 0 */
    char file[64];                       /* +0x0008 buildingedit_menu copies the name of the building file here */
    int wall_direction;                  /* +0x0048 buildingedit_render: 0 a wall along x, else along z */
    int tool;                            /* +0x004c buildingedit_setup writes 9 */
    int block_kind;                      /* +0x0050 building_editorloop: the block that E and a left click add: 0 a normal block, 1 an interior special block, 2 an outer special block. buildingedit_setup writes 0 */
    int outer_block;                     /* +0x0054 building_editorloop: the outer special block of the block picker. buildingedit_setup writes 0 */
    int inner_block;                     /* +0x0058 building_editorloop: the interior special block of the block picker. buildingedit_setup writes 0 */
    int turn;                            /* +0x005c building_editorloop: the turn (0 to 3) of a new special block. Q adds 1 */
    int cell_field_10;                   /* +0x0060 buildingedit_addblock copies it to the cell */
    int packed_parts[5];                 /* +0x0064 buildingedit_addblock packs them into the cell */
    int outer_slots[8];                  /* +0x0078 the texture slots of the outer layer; buildingedit_addblock copies the first six */
    int inner_slots[8];                  /* +0x0098 the texture slots of the inner layer */
    int extra_low;                       /* +0x00b8 the low 4 bits of the cell outer_extra */
    int extra_high;                      /* +0x00bc outer_extra = (extra_high << 4) + extra_low */
    int cell_field_14;                   /* +0x00c0 buildingedit_setup writes 1; buildingedit_addblock copies it to the cell */
    uint8_t pad_00c4[0x4];
    int drag;                            /* +0x00c8 buildingedit_render mode 0: a drag box shows while it is not 0 */
    int drag_start_x;                    /* +0x00cc  */
    int drag_start_z;                    /* +0x00d0  */
    int drag_end_x;                      /* +0x00d4  */
    int drag_end_z;                      /* +0x00d8  */
    int cursor_x;                        /* +0x00dc  */
    int floor;                           /* +0x00e0 buildingedit_render draws the blocks of this floor */
    int cursor_z;                        /* +0x00e4  */
    int area;                            /* +0x00e8 building_checkeditor: the building area that a left click sets in the area mode */
}; uint8_t lf_raw[0xec]; } BuildingEditor;
SR_STATIC_ASSERT(sizeof(BuildingEditor) == 0xec, "BuildingEditor");
SR_STATIC_ASSERT(offsetof(BuildingEditor, mode) == 0x0, "BuildingEditor.mode");
SR_STATIC_ASSERT(offsetof(BuildingEditor, unused_04) == 0x4, "BuildingEditor.unused_04");
SR_STATIC_ASSERT(offsetof(BuildingEditor, file) == 0x8, "BuildingEditor.file");
SR_STATIC_ASSERT(offsetof(BuildingEditor, wall_direction) == 0x48, "BuildingEditor.wall_direction");
SR_STATIC_ASSERT(offsetof(BuildingEditor, tool) == 0x4c, "BuildingEditor.tool");
SR_STATIC_ASSERT(offsetof(BuildingEditor, block_kind) == 0x50, "BuildingEditor.block_kind");
SR_STATIC_ASSERT(offsetof(BuildingEditor, outer_block) == 0x54, "BuildingEditor.outer_block");
SR_STATIC_ASSERT(offsetof(BuildingEditor, inner_block) == 0x58, "BuildingEditor.inner_block");
SR_STATIC_ASSERT(offsetof(BuildingEditor, turn) == 0x5c, "BuildingEditor.turn");
SR_STATIC_ASSERT(offsetof(BuildingEditor, cell_field_10) == 0x60, "BuildingEditor.cell_field_10");
SR_STATIC_ASSERT(offsetof(BuildingEditor, packed_parts) == 0x64, "BuildingEditor.packed_parts");
SR_STATIC_ASSERT(offsetof(BuildingEditor, outer_slots) == 0x78, "BuildingEditor.outer_slots");
SR_STATIC_ASSERT(offsetof(BuildingEditor, inner_slots) == 0x98, "BuildingEditor.inner_slots");
SR_STATIC_ASSERT(offsetof(BuildingEditor, extra_low) == 0xb8, "BuildingEditor.extra_low");
SR_STATIC_ASSERT(offsetof(BuildingEditor, extra_high) == 0xbc, "BuildingEditor.extra_high");
SR_STATIC_ASSERT(offsetof(BuildingEditor, cell_field_14) == 0xc0, "BuildingEditor.cell_field_14");
SR_STATIC_ASSERT(offsetof(BuildingEditor, drag) == 0xc8, "BuildingEditor.drag");
SR_STATIC_ASSERT(offsetof(BuildingEditor, drag_start_x) == 0xcc, "BuildingEditor.drag_start_x");
SR_STATIC_ASSERT(offsetof(BuildingEditor, drag_start_z) == 0xd0, "BuildingEditor.drag_start_z");
SR_STATIC_ASSERT(offsetof(BuildingEditor, drag_end_x) == 0xd4, "BuildingEditor.drag_end_x");
SR_STATIC_ASSERT(offsetof(BuildingEditor, drag_end_z) == 0xd8, "BuildingEditor.drag_end_z");
SR_STATIC_ASSERT(offsetof(BuildingEditor, cursor_x) == 0xdc, "BuildingEditor.cursor_x");
SR_STATIC_ASSERT(offsetof(BuildingEditor, floor) == 0xe0, "BuildingEditor.floor");
SR_STATIC_ASSERT(offsetof(BuildingEditor, cursor_z) == 0xe4, "BuildingEditor.cursor_z");
SR_STATIC_ASSERT(offsetof(BuildingEditor, area) == 0xe8, "BuildingEditor.area");

/* join_server[]: record of 0x48 bytes. */
typedef union JoinServer { struct {
    int version;                         /* +0x0000 joinmenu: the version of the server, 8 bits in the pong */
    char name[32];                       /* +0x0004 joinmenu: the name of a new server, 0x20 bytes of the pong */
    int account_key;                     /* +0x0024 joinmenu: the key in the pong; the join copies it to the game and createclientaccount */
    uint32_t address;                    /* +0x0028 joinmenu: the sender address in host order */
    uint16_t port;                       /* +0x002c joinmenu: the sender port in host order */
    uint8_t pad_002e[0x2];
    int players;                         /* +0x0030 joinmenu: the player count, 8 bits in the pong */
    uint8_t pad_0034[0x4];
    int max_players;                     /* +0x0038 joinmenu: the player limit, 8 bits in the pong */
    int ping;                            /* +0x003c joinmenu: the pong gives back the send time; the record keeps the round trip in ms */
    int game_type;                       /* +0x0040 joinmenu: 4 bits in the pong; 3 round, 4 world */
    uint8_t pad_0044[0x4];
}; uint8_t lf_raw[0x48]; } JoinServer;
SR_STATIC_ASSERT(sizeof(JoinServer) == 0x48, "JoinServer");
SR_STATIC_ASSERT(offsetof(JoinServer, version) == 0x0, "JoinServer.version");
SR_STATIC_ASSERT(offsetof(JoinServer, name) == 0x4, "JoinServer.name");
SR_STATIC_ASSERT(offsetof(JoinServer, account_key) == 0x24, "JoinServer.account_key");
SR_STATIC_ASSERT(offsetof(JoinServer, address) == 0x28, "JoinServer.address");
SR_STATIC_ASSERT(offsetof(JoinServer, port) == 0x2c, "JoinServer.port");
SR_STATIC_ASSERT(offsetof(JoinServer, players) == 0x30, "JoinServer.players");
SR_STATIC_ASSERT(offsetof(JoinServer, max_players) == 0x38, "JoinServer.max_players");
SR_STATIC_ASSERT(offsetof(JoinServer, ping) == 0x3c, "JoinServer.ping");
SR_STATIC_ASSERT(offsetof(JoinServer, game_type) == 0x40, "JoinServer.game_type");

/* join_address[]: record of 0x8 bytes. */
typedef union JoinAddress { struct {
    uint32_t address;                    /* +0x0000 joinmenu: an address to ping, host order */
    uint16_t port;                       /* +0x0004 joinmenu: the port of the address */
    uint8_t pad_0006[0x2];
}; uint8_t lf_raw[0x8]; } JoinAddress;
SR_STATIC_ASSERT(sizeof(JoinAddress) == 0x8, "JoinAddress");
SR_STATIC_ASSERT(offsetof(JoinAddress, address) == 0x0, "JoinAddress.address");
SR_STATIC_ASSERT(offsetof(JoinAddress, port) == 0x4, "JoinAddress.port");

/* editor_entry[]: record of 0x44 bytes. */
typedef union EditorEntry { struct {
    int index;                           /* +0x0000 buildingedit_blockmenu and buildingedit_texturemenu: the number of the block or texture in the game list */
    char name[64];                       /* +0x0004 buildingedit_blockmenu and buildingedit_texturemenu: the name of the block or texture */
}; uint8_t lf_raw[0x44]; } EditorEntry;
SR_STATIC_ASSERT(sizeof(EditorEntry) == 0x44, "EditorEntry");
SR_STATIC_ASSERT(offsetof(EditorEntry, index) == 0x0, "EditorEntry.index");
SR_STATIC_ASSERT(offsetof(EditorEntry, name) == 0x4, "EditorEntry.name");

#pragma pack(pop)

#endif
