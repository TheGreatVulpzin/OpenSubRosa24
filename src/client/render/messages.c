/* The chat bubbles over the heads of humans and over items.
 *
 * Every message sits in a ring of 256 records. A message with a target shows as a bubble over
 * the head of a human or over an item: the size falls with the distance, the bubble sticks to
 * the screen edge when the target is off screen, and bubbles of one target stack upward.
 */
#include "render/messages.h"

#include "client.h"
#include "render/draw2d.h"
#include "render/text.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

#define g_chat cl_data_chatmessage
#define g_chat_head cl_data_chat_head
#define g_camera cl_data_audio_camera

/* Game functions of other files. */
#define text_getlength CL_FN(0x509540, double(__cdecl *)(const char *text, float size))

enum {
    NUM_RING = 256,
    NUM_HUMANS = 512,
    NUM_ITEMS = 1024,
    KIND_HUMAN = 1,
    KIND_ITEM = 2,
    NO_TARGET = -1,
    STYLE_WHISPER = 0,
    STYLE_YELL = 2,
    HEAD_BONE = 2,
    TEXT_FLAGS = 0x45,
    BITMAP_FLAGS = 5,
    NO_BITMAP = -1
};
#define LIFE_SCALE 200.0
#define NEAR_SIZE 32.0
#define MAX_SIZE 32.0f
#define MIN_SIZE 4.0f
#define FADE_SIZE 8.0f
#define SCREEN_HALF_WIDTH 512.0
#define SCREEN_CENTER_Y 320.0
#define TOP_EDGE 24.0f
#define BOTTOM_EDGE 616.0f
#define BEHIND_LIMIT (-0.125f)

/* Tell which messages the ring holds: from the newest back to the one after it. */
static int next_older(int index)
{
    return (index - 1) & (NUM_RING - 1);
}

/* Draw one bubble over its target and count it for the target. */
static void draw_bubble(const ChatMessage *message, int *stack)
{
    vec3 position;
    float color[3] = {0.0f, 0.0f, 0.0f};
    float alpha = (float)((float)message->timer / LIFE_SCALE);
    float across, up, ahead, size, width, radius, x, y;
    double angle;

    if (message->kind == KIND_HUMAN)
        position = sr_humans[message->target].bones[HEAD_BONE].pos;
    if (message->kind == KIND_ITEM) {
        position = sr_items[message->target].pos;
        color[0] = color[1] = color[2] = 0.5f;
    }
    scaleaddvectors(&position, &position, &g_camera.rot[1], 0.5f);
    subtractvectors(&position, &position, &g_camera.pos);
    across = dotproduct(&position, &g_camera.rot[0]);
    up = dotproduct(&position, &g_camera.rot[1]);
    ahead = (float)-dotproduct(&position, &g_camera.rot[2]);
    size = (float)(NEAR_SIZE / ((double)vectorlength(&position) * 0.125));
    if (message->style == STYLE_WHISPER)
        size = (float)(size * 0.25);
    if (message->style == STYLE_YELL)
        size = (float)(size + size);
    if (message->kind == KIND_ITEM)
        size = (float)(size * 0.25);
    if (message->style == STYLE_WHISPER)
        color[2] = (float)(color[2] + 0.5);
    if (message->style == STYLE_YELL)
        color[0] = (float)(color[0] + 0.5);
    if (MAX_SIZE < size)
        size = MAX_SIZE;
    if (!(MIN_SIZE < size))
        return;
    if (size < FADE_SIZE)
        alpha = (float)((size - 4.0) * 0.25 * alpha);
    width = (float)text_getlength(message->text, size);
    angle = x87_atan2((double)up, (double)across);
    radius = (float)((double)x87_sqrt((double)(up * up + across * across)) * 512.0);
    if (ahead <= 1.0f) {
        if (ahead < -1.0f)
            radius = (float)(radius / (-ahead * g_camera.projection));
    } else
        radius = (float)(radius / (ahead * g_camera.projection));
    if (SCREEN_HALF_WIDTH - width * 0.5 < radius)
        radius = (float)(SCREEN_HALF_WIDTH - width * 0.5);
    x = (float)(x87_cos(angle) * radius + SCREEN_HALF_WIDTH);
    y = (float)(SCREEN_CENTER_Y - x87_sin(angle) * radius);
    if (y < TOP_EDGE)
        y = TOP_EDGE;
    if (BOTTOM_EDGE < y)
        y = BOTTOM_EDGE;
    y = (float)(y - (float)*stack * size);
    if (ahead <= BEHIND_LIMIT)
        drawbitmap(NO_BITMAP, x, y, (float)(width + 4.0), (float)(size * 0.875), BITMAP_FLAGS, 0.75f, 0.75f, 0.75f,
                   (float)(alpha * 0.75));
    else
        drawbitmap(NO_BITMAP, x, y, (float)(width + 4.0), (float)(size * 0.875), BITMAP_FLAGS, 1.0f, 1.0f, 1.0f,
                   (float)(alpha * 0.75));
    drawtext(message->text, x, y, size, TEXT_FLAGS, color[0], color[1], color[2], alpha);
    ++*stack;
}

/* Draw the chat bubbles of the frame. The count of bubbles of every human and item starts at 0. */
void __cdecl frame_drawmessages(void)
{
    int i;
    int index;

    for (i = 0; i < NUM_HUMANS; ++i)
        sr_humans[i].bubbles = 0;
    for (i = 0; i < NUM_ITEMS; ++i)
        sr_items[i].bubbles = 0;
    for (index = (int)g_chat_head; index != (int)((g_chat_head + 1) & (NUM_RING - 1)); index = next_older(index)) {
        const ChatMessage *message = &g_chat[index];

        if (message->timer > 0 && message->delay > 0 && (message->kind == KIND_HUMAN || message->kind == KIND_ITEM) &&
            message->target != NO_TARGET) {
            if (message->kind == KIND_HUMAN)
                draw_bubble(message, &sr_humans[message->target].bubbles);
            else
                draw_bubble(message, &sr_items[message->target].bubbles);
        }
    }
}
