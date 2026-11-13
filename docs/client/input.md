# Keyboard, mouse and player controls

Keyboard and mouse state supply movement axes, action bits and camera motion. Files in `src/client/input/` cover polling, item controls, player controls, debug keys and menu dispatch.

## Records

| Record | Fields | Use |
| --- | --- | --- |
| `InputMouse`, 0x2c bytes | `x`, `y`, `dx`, `dy`, `left`, `right`, `left_released`, `right_released`, `wheel_up`, `wheel_down`, `buttons` | Cursor coordinates, relative motion, button transitions and wheel counts. |
| `InputCamera`, 0x1c bytes | `pos`, `yaw`, `pitch`, `zoom`, `zoom_target` | Free camera position, view and zoom or distance. |
| `Player` | `human_id`, `input_axes`, `yaw`, `input_pitch`, `input_extra`, `input_buttons`, `move_mode`, `zoom_target`, `zoom`, `crouch_toggle`, `input_mode`, `unused_cc`, `held_item_previous` | The input packet and its local control state. `unused_cc` holds the control mode for transition detection. |
| `Human` | `health`, `vehicle_id`, `vehicle_seat`, `axis`, `pitch`, `bones`, `inventory` | Body state, view frame and items in each hand. |
| `Item` | `type`, `pos`, `rot`, `call_state`, `num_bills` | Phone, cash and door controls. |
| `Game` | `link_state`, `gametype`, `state`, `option_menu`, `voice_menu`, `connections_menu`, `spectator_human` | Menus, game mode and spectator selection. |
| `Debug` | `display`, `display_mode`, `world_view`, `human_view` | Diagnostic display switches. |

Keyboard arrays hold 512 SDL scan codes. `checkkeyboard` copies 323 bytes into the transition buffer and reads 323 bytes from SDL. Right Shift also sets Left Shift. Bytes outside this range receive no writes.

The camera array has sixteen records. Chat text reserves 64 bytes and accepts up to 60 characters from its text control.

The shared game flags `unused_257dc` and `unused_257e0` control camera following and the map display. `Option.invertmouse` selects the vertical mouse sign. The selected player uses `sr_data_view`, and the server switch uses `sr_server`.

Connection removal uses 1024 IPv4 slots. The client count lies 0x28884 bytes after the list base. Its address is independent of the server packet workspace layout. The client input switch uses an integer at the byte position of `Game.start_angle_b`.

Mouse polling copies the complete mouse record into the transition record before reading SDL. Absolute coordinates scale to a 1024 by 640 canvas. Relative polling centers the cursor at 512 by 320 and stores relative motion separately. The low eight button bits form `buttons`. Left and right release flags require a matching transition state of exactly one. Polling does not consume wheel counts.

Coordinate conversion truncates to a signed 32-bit integer. An invalid or out-of-range conversion produces `INT32_MIN`.

## Item controls

`getinputs_phone` accepts ten keypad digits and ten number-row digits. A press queues the digit only when the corresponding key in the transition buffer is zero. Keypad digits run before number-row digits. Two keys for the same digit queue two actions. Human and car controls select phone digits when the selected phone has `call_state` equal to one.

`getinputs_money` consumes positive upward wheel counts as action zero and downward counts as action one. Negative counts receive no writes. R queues an action equal to `num_bills + 2`.

`getinputs_door` uses the first item of the left hand. A missing human or empty hand causes no writes. The hinge lies half a unit behind the item along its first rotation axis. Mouse motion moves a target along the head axes. Horizontal reach is limited to 0.875 from the first body bone. The angle of the target about the hinge becomes `input_axes[0]`.

## Movement and view

On foot, W and S control the forward axis. A and D control the sideways axis. The wheel selects movement modes zero through two, with zoom targets zero, 0.0625 and 0.5. Cash in the selected hand consumes the wheel for bill actions. Shift selects the second hand. A door in that hand supplies the initial door angle, but door motion uses the left hand.

The hand-change check can read item index minus one. Its type word lies 416 bytes before the item array. A door type at that position selects the angle from the same record position.

View initialization converts signaling NaNs to quiet NaNs. Diagnostic camera position copies also use float loads and stores that quiet signaling NaNs.

Right drag changes `input_extra` and sets free look. Without right drag, the extra angles contribute one eighth of their value to the view and decay by 0.875. Zoom scales mouse sensitivity. Yaw wraps once across double-precision pi. Pitch and extra view angles use float limits. Control toggles the crouch latch after the crouch action bit reads it.

Car controls select gear, steering, shift and forward axes. W takes priority over S. The forward axis changes by 0.03125 with a key and relaxes by 0.125 without either key. Steering changes by approximately 0.0375 and has limits of -0.75 and 0.75. Left steering runs before right steering.

Right drag adjusts gear and shift. Without right drag, gear becomes zero, one or two and shift becomes minus one, zero or one. View yaw has limits of float pi, and pitch has limits of half pi.

An unordered gear value selects two without right drag. An unordered shift value selects zero. Right drag permits unordered values in these axes. Left steering maps an unordered value to -0.75. Right steering maps it to 0.75.

Helicopter mouse controls use two shared accumulators. Each accumulator decays toward zero by one and then receives one sixteenth of the mouse displacement. Right drag with no left button controls the view instead. W and S adjust the forward axis. A and D control yaw. All four movement axes have limits of minus one and one.

Pitch inversion affects human and car view, and the spectator camera. Helicopter view uses its direct vertical sign.

An unordered helicopter mouse accumulator becomes zero during decay.

Input blocking suppresses movement keys and item buttons. Mouse view and zoom can change independently of that block. A menu blocks the movement updates according to each control mode. Mode initialization can clear fields before the menu check.

Action bits use 0x1 for fire, 0x2 for free look, 0x4 for jump and 0x8 for crouch. Shift uses 0x10 and Grave uses 0x20. The five inventory slots use 0x40 through 0x400. E uses 0x800, R uses 0x1000, and F uses 0x2000. Helicopter keypad digits zero through three use 0x4000 through 0x20000. Delete uses 0x40000 in human and car controls.

## Cameras

`movecamera` uses a base speed of 0.5, or 2 with Shift. Control multiplies speed by 0.125. W, S, A and D translate along horizontal view axes. Q and Z change height. Right drag turns the view. Wheel counts select zoom from zero through 0.875.

`movecamera2` uses a base speed of 0.25, or 1 with Shift. Control multiplies speed by 0.25. Mouse motion always turns the view. Pitch has limits of half pi. Wheel counts select distance from 2 through 32. Both cameras smooth the target difference by one sixteenth.

Coordinate writes, angle writes and smoothing results round to float. Sensitivity rounds before multiplication by mouse movement. Integer mouse displacement converts to float before double arithmetic. Trigonometric calls use the client math imports. Vector operations round their result components, including the door reach length and its reciprocal factor.

## Menus and dispatch

`getinputs` creates chat, pause and connection controls before polling. Map and player-menu switches read that key state. It polls system messages, keys, the mouse and menu controls in that order. A change into relative capture performs two extra relative reads. Firing waits for release of the left button after menu use.

Connection controls store the address of a local selection value. Menu processing writes the selected connection index through this pointer during input dispatch. The address depends on the call frame and can differ between executions. Its target is valid only during input dispatch.

`dispatch.c` holds `getinputs`, but no table entry calls it. The connection menu stores the address of a local variable, and that address differs on each call.

Return opens chat. Return release sends a nonempty message unless Escape cancels it. T, Y and U select talk, yell and whisper. Player menu actions enter the action ring and clear `menu_action`.

The body must have positive health to select movement controls. A body on foot selects human controls. A car or passenger seat selects car controls. A helicopter driver selects helicopter controls. A player without a living body has zero movement axes and can select spectator targets. Player index minus one omits player menu and movement updates.

`debug_getinputs` selects camera players, resets interior measurements, copies the body view position and changes diagnostic display switches. Input blocking permits F6 and F11 display controls. F11 cycles six display modes. Insert adds 10000 to the money of player zero on a press. If the body exists, Insert also copies the head position into camera zero on each call.

## Functions

| File | Functions |
| --- | --- |
| `keyboard.c`, `keyboard.h` | `checkkeyboard`, `input_pressed` |
| `mouse.c` | `checkmouse`, `checkmouserelative` |
| `items.c` | `getinputs_phone`, `getinputs_money`, `getinputs_door` |
| `camera.c` | `movecamera`, `movecamera2` |
| `player_inputs.c` | `getinputs_human`, `getinputs_car`, `getinputs_heli` |
| `debug.c` | `debug_getinputs` |
| `dispatch.c` | `getinputs` |
