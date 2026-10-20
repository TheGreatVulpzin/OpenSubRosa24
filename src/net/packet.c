/* The packets of the game.
 *
 * A packet is a bit stream in the socket record (sr_bsdsocket): pbitwrite adds bits to the
 * send buffer from the low bit of each byte up, and pbitread takes them from the receive
 * buffer in the same order. cs_pwrite and packposition add whole bytes: they first move to
 * the next byte. Each received packet starts with the 4 bytes of the socket id ("7DFP").
 */
#include "packet.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "vectors.h"

#define BUFFER_SIZE 0x10000

/* Write the low bits of *value. Returns 0 when the send buffer is full. A count of 32 bits
 * writes zeros: the x86 uses only the low 5 bits of a shift count. */
int pbitwrite(const uint32_t *value, uint32_t bits)
{
    BsdSocket *bs = &sr_bsdsocket;
    uint32_t v, room;
    if (bs->send_size > BUFFER_SIZE - 1)
        return 0;
    v = ~(0xffffffffu << (bits & 31)) & *value;
    if ((int)bits <= 0)
        return 1;
    for (;;) {
        uint8_t *byte = &bs->send_buf[bs->send_size];
        *byte &= (uint8_t) ~(((1u << (bits & 31)) - 1) << (bs->send_bit & 31));
        *byte |= (uint8_t)(v << (bs->send_bit & 31));
        room = 8 - (uint32_t)bs->send_bit;
        if (bits < room) {
            bs->send_bit += (int)bits;
            return 1;
        }
        if (bits == room)
            break;
        bs->send_size++;
        bits -= room;
        v >>= room & 31;
        bs->send_bit = 0;
        if ((int)bits < 1)
            return 1;
    }
    bs->send_size++;
    bs->send_bit = 0;
    return 1;
}

/* Read bits into *value, low bit first. Returns 0 when the packet has too few bytes. */
int pbitread(uint32_t *value, uint32_t bits)
{
    BsdSocket *bs = &sr_bsdsocket;
    uint32_t v = 0, room;
    int bit, shift = 0;
    if (bs->recv_pos >= BUFFER_SIZE || ((int)bits >> 3) + bs->recv_pos > bs->recv_size)
        return 0;
    bit = bs->recv_bit;
    if ((int)bits > 0) {
        for (;;) {
            v |= ((uint32_t)(bs->recv_buf[bs->recv_pos] >> (bit & 31)) & ~(0xffffffffu << (bits & 31)))
                 << (shift & 31);
            room = 8 - (uint32_t)bit;
            if (bits < room) {
                bs->recv_bit = bit + (int)bits;
                *value = v;
                return 1;
            }
            if (bits == room)
                break;
            bs->recv_pos++;
            bits -= room;
            shift += (int)room;
            bs->recv_bit = 0;
            bit = 0;
            if ((int)bits < 1) {
                *value = v;
                return 1;
            }
        }
        bs->recv_pos++;
        bs->recv_bit = 0;
    }
    *value = v;
    return 1;
}

int pbitwritenego(const uint32_t *value, uint32_t bits)
{
    pbitwrite(value, bits);
    return 1;
}

/* Read bits as a value where all bits set means -1 (0xffffffff). A failed read keeps the
 * bit count as the value. */
int pbitreadnego(uint32_t *value, uint32_t bits)
{
    uint32_t v = bits;
    pbitread(&v, bits);
    *value = v == (1u << (bits & 31)) - 1 ? 0xffffffffu : v;
    return 1;
}

/* Move to the next whole byte when some bits of the current byte are in use. */
static void align_send(BsdSocket *bs)
{
    if (bs->send_bit > 0) {
        bs->send_size++;
        bs->send_bit = 0;
    }
}

/* Write size * count bytes at the next whole byte. Returns 0 when the send buffer is full. */
int cs_pwrite(const void *data, int size, int count)
{
    BsdSocket *bs = &sr_bsdsocket;
    if (bs->send_size > BUFFER_SIZE - 1)
        return 0;
    align_send(bs);
    memcpy(&bs->send_buf[bs->send_size], data, (size_t)(size * count));
    bs->send_size += size * count;
    return 1;
}

/* Read size * count bytes at the next whole byte. Returns 0 when the packet has too few bytes.
 * The size test comes before the move to the next byte, so a read can pass the end by one. */
int cs_pread(void *dst, int size, int count)
{
    BsdSocket *bs = &sr_bsdsocket;
    size_t n;
    if (bs->recv_pos > 0xffff)
        return 0;
    n = (size_t)(size * count);
    if (bs->recv_size < (int)(n + (size_t)bs->recv_pos))
        return 0;
    if (bs->recv_bit > 0) {
        bs->recv_pos++;
        bs->recv_bit = 0;
    }
    memcpy(dst, &bs->recv_buf[bs->recv_pos], n);
    bs->recv_pos += (int)n;
    return 1;
}

/* Receive one packet on the current socket. A packet that does not start with the socket id
 * counts as 0 bytes. The first byte compares as a signed char, so an id byte of 0x80 or more
 * never matches. */
int recvpacket(void)
{
    BsdSocket *bs = &sr_bsdsocket;
    const uint8_t *id = (const uint8_t *)&sr_socketid;
    if (bs->enabled[bs->current] == 0)
        return 0;
    memset(&bs->addr[4], 0, 4); /* sin_addr */
    bs->recv_size = os_recvfrom((uintptr_t)bs->handle[bs->current], (char *)bs->recv_buf, BUFFER_SIZE, 0, bs->addr,
                                &bs->addr_len);
    if (bs->recv_size > 0) {
        if (bs->recv_buf[0] == (int)(signed char)id[0] && bs->recv_buf[1] == id[1] && bs->recv_buf[2] == id[2] &&
            bs->recv_buf[3] == id[3]) {
            bs->recv_pos = 4;
            bs->recv_bit = 0;
            return bs->recv_size;
        }
        bs->recv_size = 0;
    }
    return bs->recv_size;
}

/* Send the send buffer to ip:port (host byte order) on the current socket. */
void sendpacket(uint32_t ip, uint16_t port)
{
    BsdSocket *bs = &sr_bsdsocket;
    uint32_t net_ip = _byteswap_ulong(ip);
    uint16_t net_port = _byteswap_ushort(port);
    if (bs->enabled[bs->current] == 0)
        return;
    align_send(bs);
    memcpy(&bs->addr[4], &net_ip, 4);   /* sin_addr */
    memcpy(&bs->addr[2], &net_port, 2); /* sin_port */
    os_sendto((uintptr_t)bs->handle[bs->current], (const char *)bs->send_buf, bs->send_size, 0, bs->addr, bs->addr_len);
}

/* An angle in radians as bits - 1 bits of size and one sign bit (the top bit). */
void packangle(const float *angle, uint32_t bits)
{
    uint32_t half = (1u << (bits & 31)) >> 1;
    uint32_t v = (uint32_t)(long long)(((float)half / 6.28318530718) * fabs(*angle)) & (half - 1);
    if (*angle < 0.0)
        v |= half;
    pbitwrite(&v, bits);
}

/* Add 4 raw bytes at the next whole byte. Returns 0 when the send buffer is full. */
static int put_dword(BsdSocket *bs, const float *value)
{
    if (bs->send_size >= BUFFER_SIZE)
        return 0;
    align_send(bs);
    memcpy(&bs->send_buf[bs->send_size], value, 4);
    bs->send_size += 4;
    return 1;
}

/* A position as 3 raw floats. */
void packposition(const vec3 *pos)
{
    BsdSocket *bs = &sr_bsdsocket;
    if (put_dword(bs, &pos->x) && put_dword(bs, &pos->y))
        put_dword(bs, &pos->z);
}

/* The 6 corners of the octahedron, and its 8 faces: 3 corners each, by octant. */
static const vec3 octahedron[6] = {
    { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f },
    { 1.0f, 0.0f, 0.0f },  { 0.0f, 0.0f, 1.0f },  { 0.0f, 1.0f, 0.0f },
};
static const int octahedron_faces[8][3] = {
    { 5, 3, 4 }, { 5, 4, 1 }, { 5, 2, 3 }, { 5, 1, 2 }, { 4, 3, 0 }, { 1, 4, 0 }, { 3, 2, 0 }, { 2, 1, 0 },
};

/* packorientation keeps the triangle and the midpoints of its edges in a global. */
#define g_tri SR_AT(vec3, 0xf437b00) /* 3 corners */
#define g_mid (g_tri + 3)           /* corner 0 + 1, corner 2 + 1, corner 2 + 0 */

/* Which side of the plane through the origin, a and b the point v is on. */
static __forceinline float side(const vec3 *v, const vec3 *a, const vec3 *b)
{
    float dx = (float)(v->x - b->x), dy = (float)(v->y - b->y), dz = (float)(v->z - b->z);
    float ex = (float)(a->x - b->x), ey = (float)(a->y - b->y), ez = (float)(a->z - b->z);
    float cx = (float)(ey * dz - ez * dy);
    float cy = (float)(ez * dx - dz * ex);
    float cz = (float)(dy * ex - ey * dx);
    return (float)(cz * v->z + (v->x * cx + v->y * cy));
}

/* side and midpoint are inline: as calls, packorientation is 2% slower. */
static __forceinline void midpoint(vec3 *m, const vec3 *a, const vec3 *b)
{
    m->x = (float)(a->x + b->x);
    m->y = (float)(a->y + b->y);
    m->z = (float)(a->z + b->z);
    normalizevector(m, m);
}

/* A unit vector as a path in the octahedron: 3 bits for the octant (face), then 2 bits for
 * each split of the face into 4 triangles (0: at corner 0, 1: at corner 1, 2: at corner 2,
 * 3: the middle one). */
void packorientation(const vec3 *dir, uint32_t bits)
{
    vec3 *tri = g_tri, *mid = g_mid;
    uint32_t code = dir->x < 0.0, one = 8, sub;
    int k, pos;
    if (dir->z < 0.0)
        code |= 2;
    if (dir->y < 0.0)
        code |= 4;
    for (k = 0; k < 3; k++)
        tri[k] = octahedron[octahedron_faces[code][k]];
    for (pos = 3; pos < (int)bits; pos += 2) {
        midpoint(&mid[0], &tri[0], &tri[1]);
        midpoint(&mid[1], &tri[2], &tri[1]);
        midpoint(&mid[2], &tri[2], &tri[0]);
        /* Each test is false for a NaN, so the form of each test matters. */
        if (side(dir, &mid[0], &mid[2]) <= 0.0) {
            if (0.0 < side(dir, &mid[1], &mid[0])) {
                tri[0] = mid[0];
                tri[2] = mid[1];
                sub = one;
            } else if (side(dir, &mid[2], &mid[1]) <= 0.0) {
                tri[0] = mid[0];
                tri[1] = mid[1];
                tri[2] = mid[2];
                sub = 3u << (pos & 31);
            } else {
                tri[0] = mid[2];
                tri[1] = mid[1];
                sub = 2u << (pos & 31);
            }
        } else {
            tri[1] = mid[0];
            tri[2] = mid[2];
            sub = 0;
        }
        code |= sub;
        one = one << 2 | one >> 30; /* 1 << pos, as a rotation */
    }
    pbitwrite(&code, bits);
}

/* A number in 0..range as bits - 1 bits of size and one sign bit (the top bit). */
void packscalar(const float *x, float range, uint32_t bits)
{
    uint32_t half = (1u << (bits & 31)) >> 1;
    uint32_t v = (uint32_t)(long long)(((float)half / range) * fabs(*x));
    if (v > half - 1)
        v = half - 1;
    if (*x < 0.0)
        v |= half;
    pbitwrite(&v, bits);
}

/* The reverse of packscalar with the range 1.0. */
float unpackscalar(uint32_t raw, uint32_t bits)
{
    uint32_t half = (1u << (bits & 31)) >> 1;
    float v = (float)((raw & (half - 1)) * (1.0 / half));
    return (raw & half) == half ? -v : v;
}

/* An angle in radians from bits - 1 bits of size and a sign bit. A failed read keeps the bit
 * count as the raw value. */
void unpackangle(float *out, uint32_t bits)
{
    uint32_t raw = bits, half, m;
    float a;
    pbitread(&raw, bits);
    half = (1u << (bits & 31)) >> 1;
    m = (half - 1) & raw;
    a = (float)((double)m / ((double)half / 6.28318530718));
    *out = a;
    if ((raw & half) == half)
        *out = -a;
}

/* A unit vector as the axis of its largest component (and the sign of it), and the two other
 * components divided by it: 2 * magnitude + sign. */
void packunitvector(uint32_t out[3], const float v[3], uint8_t bits)
{
    uint32_t axis = fabs(v[0]) < fabs(v[1]);
    uint32_t *o = out + 1;
    double scale;
    int i;
    if (fabs(v[axis]) < fabs(v[2]))
        axis = 2;
    out[0] = axis;
    if (v[axis] < 0.0)
        out[0] = axis | 4;
    scale = (double)((1 << (bits & 31)) - 1);
    for (i = 0; i < 3; i++) {
        int k;
        if ((uint32_t)i == axis)
            continue;
        k = (int)(long long)(fabs((double)v[i] / v[axis]) * scale);
        *o = (uint32_t)(k * 2);
        if (v[i] < 0.0)
            *o = (uint32_t)(k * 2) | 1;
        o++;
    }
}

/* The difference a - b of 3 components as signed numbers in -range..range. */
void packpositiondelta(const vec3 *a, const vec3 *b, float range, uint32_t bits)
{
    uint32_t half = (1u << (bits & 31)) >> 1, v;
    const float *pa = &a->x, *pb = &b->x;
    int i;
    for (i = 0; i < 3; i++) {
        float d = (float)(pa[i] - pb[i]);
        v = (uint32_t)(long long)(fabs(d) * ((float)half / range));
        if (v > half - 1)
            v = half - 1;
        if (d < 0.0)
            v |= half;
        pbitwrite(&v, bits);
    }
}

/* ref + the difference of packpositiondelta. A failed read keeps the previous raw value. */
void unpackpositiondelta(vec3 *out, const vec3 *ref, float range, uint32_t bits)
{
    uint32_t half = (1u << (bits & 31)) >> 1, raw = half;
    float *po = &out->x;
    const float *pr = &ref->x;
    int i;
    for (i = 0; i < 3; i++) {
        float d;
        pbitread(&raw, bits);
        d = (float)((double)((half - 1) & raw) / ((float)half / range));
        if ((half & raw) == half)
            d = -d;
        po[i] = (float)(pr[i] + d);
    }
}

/* The reverse of packorientation: the face from the first 3 bits, then each pair of bits
 * picks one of the 4 triangles. The result is the normalized sum of the 3 corners. */
void unpackorientation(vec3 *out, uint32_t bits)
{
    vec3 *tri = g_tri, *mid = g_mid;
    uint32_t code = bits;
    int k, pos;
    pbitread(&code, bits);
    for (k = 0; k < 3; k++)
        tri[k] = octahedron[octahedron_faces[code & 7][k]];
    for (pos = 3; pos < (int)bits; pos += 2) {
        midpoint(&mid[0], &tri[0], &tri[1]);
        midpoint(&mid[1], &tri[2], &tri[1]);
        midpoint(&mid[2], &tri[2], &tri[0]);
        switch ((code >> (pos & 31)) & 3) {
        case 0:
            tri[1] = mid[0];
            tri[2] = mid[2];
            break;
        case 1:
            tri[0] = mid[0];
            tri[2] = mid[1];
            break;
        case 2:
            tri[0] = mid[2];
            tri[1] = mid[1];
            break;
        default:
            tri[0] = mid[0];
            tri[1] = mid[1];
            tri[2] = mid[2];
            break;
        }
    }
    out->x = (float)(tri[1].x + tri[0].x);
    out->y = (float)(tri[1].y + tri[0].y);
    out->z = (float)(tri[1].z + tri[0].z);
    out->x = (float)(out->x + tri[2].x);
    out->y = (float)(out->y + tri[2].y);
    out->z = (float)(out->z + tri[2].z);
    normalizevector(out, out);
}
