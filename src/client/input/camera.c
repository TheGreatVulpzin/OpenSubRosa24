/* Camera controls translate along horizontal axes and change the viewing angles.
 * Shift increases speed. Control reduces speed.
 * Wheel events select zoom or spectator distance with gradual interpolation.
 */
#include "input/camera.h"
#include "input/keyboard.h"
#include "sr_math.h"
#include "sr_runtime.h"

#define CAMERA_MOUSE_SCALE 320.0
#define CAMERA_PITCH_LIMIT 1.5707963705062866f
#define CAMERA_ZOOM_LIMIT 0.875f
#define CAMERA_DISTANCE_MIN 2.0f
#define CAMERA_DISTANCE_MAX 32.0f
#define FREE_CAMERA_SPEED 0.5f
#define FREE_CAMERA_FAST_SPEED 2.0f
#define FREE_CAMERA_SLOW_SCALE 0.125
#define SPECTATOR_SPEED 0.25f
#define SPECTATOR_FAST_SPEED 1.0f
#define SPECTATOR_SLOW_SCALE 0.25
#define CAMERA_ZOOM_INCREMENT 0.125

/* Translate the camera in key order. Each coordinate write rounds to float. */
static void camera_translate(InputCamera *camera, float speed)
{
    if (cl_data_audio_keys[KEY_W]) {
        camera->pos.x = (float)(x87_sin(camera->yaw) * speed + camera->pos.x);
        camera->pos.z = (float)(camera->pos.z - x87_cos(camera->yaw) * speed);
    }
    if (cl_data_audio_keys[KEY_S]) {
        camera->pos.x = (float)(camera->pos.x - x87_sin(camera->yaw) * speed);
        camera->pos.z = (float)(x87_cos(camera->yaw) * speed + camera->pos.z);
    }
    if (cl_data_audio_keys[KEY_A]) {
        camera->pos.x = (float)(camera->pos.x - x87_cos(camera->yaw) * speed);
        camera->pos.z = (float)(camera->pos.z - x87_sin(camera->yaw) * speed);
    }
    if (cl_data_audio_keys[KEY_D]) {
        camera->pos.x = (float)(x87_cos(camera->yaw) * speed + camera->pos.x);
        camera->pos.z = (float)(x87_sin(camera->yaw) * speed + camera->pos.z);
    }
    if (cl_data_audio_keys[KEY_Q])
        camera->pos.y = (float)((double)camera->pos.y + speed);
    if (cl_data_audio_keys[KEY_Z])
        camera->pos.y = (float)((double)camera->pos.y - speed);
}

/* Move the free camera. Right drag turns the view without a pitch limit. */
void movecamera(int camera_index)
{
    InputCamera *camera = &cl_data_input_cameras[camera_index];
    InputMouse *mouse = &cl_data_input_mouse;
    float speed = cl_data_audio_keys[KEY_SHIFT] ? FREE_CAMERA_FAST_SPEED : FREE_CAMERA_SPEED;
    float sensitivity;
    if (cl_data_audio_keys[KEY_CONTROL])
        speed = (float)((double)speed * FREE_CAMERA_SLOW_SCALE);
    camera_translate(camera, speed);
    sensitivity = (float)(1.0 - camera->zoom);
    if (mouse->right) {
        camera->yaw = (float)((double)(float)mouse->dx * sensitivity / CAMERA_MOUSE_SCALE + camera->yaw);
        camera->pitch = (float)((double)(float)mouse->dy * sensitivity / CAMERA_MOUSE_SCALE + camera->pitch);
    }
    for (; mouse->wheel_up > 0; mouse->wheel_up--)
        camera->zoom_target = (float)((double)camera->zoom_target + CAMERA_ZOOM_INCREMENT);
    for (; mouse->wheel_down > 0; mouse->wheel_down--)
        camera->zoom_target = (float)((double)camera->zoom_target - CAMERA_ZOOM_INCREMENT);
    if (camera->zoom_target < 0.0f) camera->zoom_target = 0.0f;
    if (CAMERA_ZOOM_LIMIT < camera->zoom_target) camera->zoom_target = CAMERA_ZOOM_LIMIT;
    camera->zoom = (float)(((double)camera->zoom_target - camera->zoom) * 0.125 * 0.5 + camera->zoom);
}

/* Move the spectator camera and limit its pitch and target distance. */
void movecamera2(int camera_index)
{
    InputCamera *camera = &cl_data_input_cameras[camera_index];
    InputMouse *mouse = &cl_data_input_mouse;
    float speed = cl_data_audio_keys[KEY_SHIFT] ? SPECTATOR_FAST_SPEED : SPECTATOR_SPEED;
    if (cl_data_audio_keys[KEY_CONTROL])
        speed = (float)((double)speed * SPECTATOR_SLOW_SCALE);
    camera_translate(camera, speed);
    camera->yaw = (float)((double)(float)mouse->dx / CAMERA_MOUSE_SCALE + camera->yaw);
    if (!sr_data_option.invertmouse)
        camera->pitch = (float)((double)(float)mouse->dy / CAMERA_MOUSE_SCALE + camera->pitch);
    else
        camera->pitch = (float)(camera->pitch - (double)(float)mouse->dy / CAMERA_MOUSE_SCALE);
    if (camera->pitch < -CAMERA_PITCH_LIMIT) camera->pitch = -CAMERA_PITCH_LIMIT;
    if (CAMERA_PITCH_LIMIT < camera->pitch) camera->pitch = CAMERA_PITCH_LIMIT;
    for (; mouse->wheel_up > 0; mouse->wheel_up--)
        camera->zoom_target = (float)((double)camera->zoom_target - 1.0);
    for (; mouse->wheel_down > 0; mouse->wheel_down--)
        camera->zoom_target = (float)((double)camera->zoom_target + 1.0);
    if (camera->zoom_target < CAMERA_DISTANCE_MIN) camera->zoom_target = CAMERA_DISTANCE_MIN;
    if (CAMERA_DISTANCE_MAX < camera->zoom_target) camera->zoom_target = CAMERA_DISTANCE_MAX;
    camera->zoom = (float)(((double)camera->zoom_target - camera->zoom) * 0.125 * 0.5 + camera->zoom);
}
