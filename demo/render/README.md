# Render demo

A small program that shows how the Sub Rosa client draws one frame. It draws a city of boxes with the same passes as `renderframe` (`src/client/render/frame.c`) and `renderframedepth` (`src/client/render/depth.c`), and shows the result of each pass.

Build: `demo\render\build.bat`. The program is `build\demo\render_demo.exe`. It needs only Windows and OpenGL 2.0. It uses no game files.

## Keys

| Key | Action |
| --- | --- |
| Left, Right, 1 to 9 | select the pass |
| Q, E | turn the sun |
| R, F | tilt the sun |
| Right mouse drag | turn the camera |
| Wheel | move the camera nearer or farther |
| T | show or hide the buffer thumbnails |
| Esc | quit |

## The passes

1. **Depth.** The opaque geometry writes only depth. The view shows near as dark and far as light.
2. **Shadow volumes.** Each box extrudes its silhouette edges away from the sun. Front faces add 1 to the stencil count and back faces take 1 away. The view draws the volumes in see-through green, as the shadow debug key of the game does.
3. **Stencil count.** Black is count 0 (lit), red 1, yellow 2, white 3 or more.
4. **Shadow mask.** The red channel becomes 1 where the stencil count is 0.
5. **Occlusion.** The green channel holds ambient occlusion from 8 depth samples around each pixel.
6. **Blur.** Two passes, across and down, with a step of 1.125 divided by the size.
7. **Opaque.** The lit scene reads the red and green channels at its screen position.
8. **Sky and sun.** The sky fills the pixels with no geometry. The sun is a sphere with additive blend.
9. **Alpha.** The car windows draw from far to near.

The shader code of the demo is simple and its own. The order of the passes, the stencil method, the channel use and the blur step follow the client.
