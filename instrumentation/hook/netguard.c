/* Input checks on received datagrams, before the game reads them (Phase 5 item 6, early).
 *
 * Voice index (docs/protocol.md, section 12, item 15): serverrecv (0x33650) reads the index
 * of each of the 6 voice frames of a type 4 packet with 8 bits and uses it before the
 * "& 0x3f" mask. An index from 64 to 255 makes memcpy write up to 255 client bytes up to
 * 48 KB after the 64-entry voice ring. The client writes 0..63, so the guard masks the index
 * in the datagram to 6 bits, the same range.
 *
 * The parse copies tools/net/srpacket.py (Input.read): pbitread packs bits from the least
 * significant bit, cs_pread first moves to the next byte when the bit cursor is not 0. The
 * game reads a short packet as if stale bytes of the previous datagram followed it, so a
 * type 4 packet that ends before its last voice frame is dropped: the attacker could choose
 * those stale bytes with an earlier datagram.
 */
#include <stdint.h>
#include <string.h>

#include "netguard.h"

typedef struct Reader {
    uint8_t *buf;
    int len, pos, bit, bad;
} Reader;

/* pbitread: up to 32 bits, least significant bit first. bit_at gets the position of the
 * first bit, so the caller can rewrite the field. */
static uint32_t read_bits(Reader *r, int n, int *pos_at, int *bit_at)
{
    uint32_t out = 0;
    int got = 0;
    if (r->len < r->pos + (n + r->bit + 7) / 8) {  /* stricter than the game: every bit present */
        r->bad = 1;
        return 0;
    }
    if (pos_at) {
        *pos_at = r->pos;
        *bit_at = r->bit;
    }
    while (n > 0) {
        int room = 8 - r->bit, take = n < room ? n : room;
        out |= (uint32_t)((r->buf[r->pos] >> r->bit) & ((1u << take) - 1)) << got;
        got += take;
        n -= take;
        r->bit += take;
        if (r->bit == 8) {
            r->pos++;
            r->bit = 0;
        }
    }
    return out;
}

/* cs_pread: align to the next byte, then skip n raw bytes. */
static void skip_bytes(Reader *r, int n)
{
    if (r->bit) {
        r->pos++;
        r->bit = 0;
    }
    if (r->len < r->pos + n) {
        r->bad = 1;
        return;
    }
    r->pos += n;
}

/* Write the low 8 bits of v at (pos, bit), least significant bit first. */
static void write_byte_bits(uint8_t *buf, int pos, int bit, uint32_t v)
{
    int k;
    for (k = 0; k < 8; k++, bit++) {
        if (bit == 8) {
            pos++;
            bit = 0;
        }
        buf[pos] = (uint8_t)((buf[pos] & ~(1u << bit)) | (((v >> k) & 1u) << bit));
    }
}

int sv_netguard_input(uint8_t *buf, int len, int *masked)
{
    Reader r = { buf, len, 4, 0, 0 };
    int i, n;
    *masked = 0;
    if (len < 5 || memcmp(buf, "7DFP", 4) != 0 || buf[4] != 4)
        return SV_GUARD_PASS;
    skip_bytes(&r, 1);                  /* type */
    skip_bytes(&r, 4 + 8 * 4 + 4);      /* game_id, 8 floats, buttons */
    read_bits(&r, 8, NULL, NULL);       /* input_mode */
    read_bits(&r, 4, NULL, NULL);       /* hud_mode */
    read_bits(&r, 16, NULL, NULL);      /* event_ack */
    skip_bytes(&r, 12);                 /* cam_pos */
    n = (int)read_bits(&r, 4, NULL, NULL);
    read_bits(&r, 8, NULL, NULL);       /* action_start */
    for (i = 0; i < n && !r.bad; i++) {
        uint32_t kind = read_bits(&r, 4, NULL, NULL), k, len6;
        if (kind == 0) {                /* menu: id, int32 value */
            read_bits(&r, 8, NULL, NULL);
            skip_bytes(&r, 4);
        } else if (kind == 1) {         /* chat: length, one aligned byte per character, mode */
            len6 = read_bits(&r, 6, NULL, NULL);
            for (k = 0; k < len6 && !r.bad; k++)
                skip_bytes(&r, 1);
            read_bits(&r, 4, NULL, NULL);
        } else if (kind == 2) {         /* item: two 16-bit arguments */
            read_bits(&r, 16, NULL, NULL);
            read_bits(&r, 16, NULL, NULL);
        }
    }
    for (i = 0; i < 6 && !r.bad; i++) {
        int pos, bit;
        uint32_t index = read_bits(&r, 8, &pos, &bit), size;
        size = read_bits(&r, 8, NULL, NULL);
        read_bits(&r, 2, NULL, NULL);   /* mode */
        skip_bytes(&r, (int)size);
        if (!r.bad && index > 63) {
            write_byte_bits(buf, pos, bit, index & 0x3f);
            (*masked)++;
        }
    }
    return r.bad ? SV_GUARD_DROP : SV_GUARD_PASS;
}
