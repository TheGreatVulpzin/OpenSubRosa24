/* Small functions on the records of the client: free slots, the cash of a team, the tax on an account,
 * the width of a text and three numeric helpers.
 */
#include "setup/small_records.h"

#include <math.h>
#include <stdint.h>

#include "client.h"
#include "sr_runtime.h"
#include "vectors.h"

#define NUM_PLAYERS 0x100
#define NUM_OBJECTS 0x400
#define NUM_ITEMS 0x400
#define NUM_HUMANS 0x200
#define TEAM_RECORD 0x117c
#define TEXT_LIMIT 1000
#define TAX_FREE 10000
#define TAX_RATE 0.05000000074505806
#define DISTANCE_POWER 1.75
#define DISTANCE_RANGE 256.0

/* The index of the first player slot that is not in use, or -1. */
int __cdecl getnextplayer(void)
{
    int i;

    for (i = 0; i < NUM_PLAYERS; i++)
        if (sr_data_player[i].active == 0)
            return i;
    return -1;
}

/* The index of the first object slot that is not in use, or -1. */
int __cdecl getnextobject(void)
{
    int i;

    for (i = 0; i < NUM_OBJECTS; i++)
        if (sr_data_object[i].active == 0)
            return i;
    return -1;
}

/* The index of the first item slot that is not in use, or -1. */
int __cdecl getnextitem(void)
{
    int i;

    for (i = 0; i < NUM_ITEMS; i++)
        if (sr_data_item[i].active == 0)
            return i;
    return -1;
}

/* The index of the first human slot that is not in use, or -1. */
int __cdecl getnexthuman(void)
{
    int i;

    for (i = 0; i < NUM_HUMANS; i++)
        if (sr_data_human[i].active == 0)
            return i;
    return -1;
}

/* Copy the four values of a quaternion as they are. */
void __cdecl copyquaternion(uint32_t *to, const uint32_t *from)
{
    to[0] = from[0];
    to[1] = from[1];
    to[2] = from[2];
    to[3] = from[3];
}

/* Add points to the cash of a team. */
void __cdecl addpointstoteam(int team, int points)
{
    *(int *)((uint8_t *)&sr_data_game.team0_cash + team * TEAM_RECORD) += points;
}

/* Take a part of the cash that is above 10000 from an account: 5 percent of the part above, cut to a
 * whole number. An index outside the list, or an account with 10000 or less, changes nothing. */
void __cdecl accountdeathtax(int index)
{
    Account *account;

    if (index < 0 || index >= sr_data_accounts.count)
        return;
    account = &sr_data_accounts.records[index];
    if (0 < account->money - TAX_FREE)
        account->money -= (int)(long long)((double)(account->money - TAX_FREE) * TAX_RATE);
}

/* The width of a text in the current font at a scale: the sum of the widths of its characters,
 * up to 1000 characters. A character above 127 reads the memory before the table. */
float __cdecl gettextlength(const char *text, float scale)
{
    float length = 0.0f;
    int i;

    for (i = 0; text[i] != '\0' && i < TEXT_LIMIT; i++)
        length = (float)(cl_data_fonts[cl_data_fontnum].width[(signed char)text[i]] * scale + length);
    return length;
}

/* Round a value to the nearest multiple of a step. */
float __cdecl fround(float value, float step)
{
    double rounded = floor((double)(value / step + 0.5));

    return (float)(rounded * step);
}

/* The share of a packet that a speaker at a distance gets, from 0 to 256: 256 at the speaker and
 * 0 at 256 units. The result is a number of 64 bits. */
long long __cdecl net_calcdistancefactor(float distance)
{
    double power = pow((double)distance, DISTANCE_POWER);

    return (long long)((1.0 - (float)(power / DISTANCE_RANGE)) * DISTANCE_RANGE);
}

/* The distance along a ray to a sphere. The ray starts at origin and goes along direction, and the sphere has the given radius. */
float __cdecl raydistancesphere2(const vec3 *origin, const vec3 *direction, float radius)
{
    float along = (float)-dotproduct(origin, direction);
    double inside = (double)(float)dotproduct(origin, origin);
    double root = sqrt((double)(radius * radius - (float)(inside - (double)along * (double)along)));

    return (float)(along - root);
}
