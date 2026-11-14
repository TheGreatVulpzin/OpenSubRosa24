# Subrosa Render Pipeline Demo

A program that shows how the Sub Rosa client draws one frame. It draws a part of the real city with the same passes as `renderframe` (`src/client/render/frame.c`) and `renderframedepth` (`src/client/render/depth.c`), and shows the result of each pass. Each pass can also build itself in an animation.

## Build and run

1. Build: `demo\render\build.bat`. It writes `build\demo\render_demo.exe` and copies `passes.txt` next to it.
2. Export the scene from a client recording:
   `python -I demo/render/export_scene.py build/recordings/client-play-20261103-1229/sv_snapshot.bin build/demo/city.scene`
   It takes about two minutes.
3. Start `build\demo\render_demo.exe`. Without `city.scene` the program draws a city of boxes.

The program needs only Windows and OpenGL 2.0.

## Keys

| Key | Action |
| --- | --- |
| Left, Right, 1 to 9 | select a pass. The pass builds itself one time. |
| Space | play all passes in order, with the animation, and loop |
| Enter | skip the animation of the current pass |
| Q, E | turn the sun |
| R, F | tilt the sun |
| Right mouse drag | turn the camera |
| Wheel | move the camera nearer or farther |
| T | show or hide the buffer images under the view |
| Esc | quit |

## The scene

`export_scene.py` reads the client snapshot of a recording:

- The block sectors (`block_sectors`, 8192 records of 8 x 8 x 8 cells, a cell is 4 units). The exporter keeps the cells within 96 cells of the middle of the city. A normal cell becomes a cube with its visible faces. A mesh cell (window, door, stairs, fence) becomes a full cube, so the demo shows no window holes.
- The material of each face, and the texture of each material (`texturelist.material[]`), scaled down to 64 x 64.
- The active vehicles. The demo draws each one as a body and a cabin with four windows.

## The side panel

The texts come from `passes.txt` next to the program. Edit that file to change them. No new build is necessary.

## The animation

- Depth and opaque geometry: the city appears in rings from the middle outward.
- Shadow volumes: the volumes grow away from the sun.
- Stencil count, shadow mask, occlusion, blur: a line sweeps across the image of the pass before.
- Sky and sun: the sky fills in.
- Transparent surfaces: the car windows appear one at a time, from far to near.

## What follows the client and what does not

The order of the passes, the stencil method, the use of the red and green channels, the blur step and the sort key of the windows follow the client. The shaders of the demo are short and its own. The city has no interiors, no landscape and no models of the game.
