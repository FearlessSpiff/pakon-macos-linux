#include "pakon_dx.h"

#include <stdio.h>
#include <string.h>

int pakon_dx_parse(const uint8_t *payload, size_t len, pakon_dx_block *out)
{
    if (!payload || !out || len < PAKON_DX_REPLY_LEN)
        return -1;
    memset(out, 0, sizeof *out);
    out->position = (uint16_t)((payload[0] << 8) | payload[1]);
    out->count = payload[2];
    if (out->count > PAKON_DX_MAX_ENTRIES)
        return -1;
    for (unsigned i = 0; i < out->count; i++) {
        const uint8_t *p = payload + 3 + 5 * i;
        pakon_dx_entry *e = &out->e[i];
        e->type = p[0] & 0x0F;
        e->hi = p[0] >> 4;
        e->flag = p[1];
        memcpy(e->b, p + 2, 3);
    }
    return 0;
}

uint16_t pakon_dx_entry_position(const pakon_dx_entry *e)
{
    return (uint16_t)((e->b[0] << 8) | e->b[1]);
}

static unsigned popcount8(uint8_t v)
{
    unsigned n = 0;
    for (; v; v &= (uint8_t)(v - 1))
        n++;
    return n;
}

int pakon_dx_code_decode(const pakon_dx_entry *e, pakon_dx_code *c)
{
    if (!e || !c || e->type != PAKON_DX_CODE)
        return 0;
    uint8_t b2 = e->b[0], b3 = e->b[1], b4 = e->b[2];
    c->frame_raw = (uint8_t)(((b3 & 0x01) << 6) | (b2 >> 2));
    c->product = (uint8_t)(((b4 & 0x1F) << 2) | (b3 >> 6));
    c->generation = (b3 >> 1) & 0x0F;
    c->slot = e->flag;
    c->parity_ok = (popcount8(b2) + popcount8(b3) + popcount8(b4 & 0x1F)) % 2 == 0;
    c->valid = c->parity_ok && !(b3 & 0x20) && !(b2 & 0x02);
    return 1;
}

const char *pakon_dx_frame_label(uint8_t frame_raw, char *buf, size_t size)
{
    unsigned frame = (frame_raw & 0x7F) >> 1;
    const char *half = (frame_raw & 1) ? "A" : "";
    if (frame == 62)
        snprintf(buf, size, "X%s", half);
    else if (frame == 63)
        snprintf(buf, size, "00%s", half);
    else
        snprintf(buf, size, "%u%s", frame, half);
    return buf;
}

const char *pakon_dx_format(const pakon_dx_block *b, char *buf, size_t size)
{
    size_t o = 0;
    int n = snprintf(buf, size, "pos 0x%04x, %u entr%s", b->position, b->count,
                     b->count == 1 ? "y" : "ies");
    o = n > 0 ? (size_t)n : 0;
    for (unsigned i = 0; i < b->count && o < size; i++) {
        const pakon_dx_entry *e = &b->e[i];
        pakon_dx_code c;
        char lab[8];
        if (pakon_dx_code_decode(e, &c))
            n = snprintf(buf + o, size - o, "; code %s %u/%u slot %u%s",
                         pakon_dx_frame_label(c.frame_raw, lab, sizeof lab),
                         c.product, c.generation, c.slot,
                         c.valid ? "" : c.parity_ok ? " INVALID" : " BAD-PARITY");
        else if (e->type == PAKON_DX_NONE)
            n = snprintf(buf + o, size - o, "; none");
        else
            n = snprintf(buf + o, size - o, "; type %u flag 0x%02x pos 0x%04x",
                         e->type, e->flag, pakon_dx_entry_position(e));
        o += n > 0 ? (size_t)n : 0;
    }
    return buf;
}

void pakon_dx_roll_init(pakon_dx_roll *r)
{
    memset(r, 0, sizeof *r);
    r->last_position = -1;
}

void pakon_dx_roll_add(pakon_dx_roll *r, const pakon_dx_block *b)
{
    r->blocks++;
    for (unsigned i = 0; i < b->count; i++) {
        const pakon_dx_entry *e = &b->e[i];
        pakon_dx_code c;
        r->entries++;
        r->type_count[e->type]++;
        if (e->type == PAKON_DX_POSITION) {
            r->last_position = pakon_dx_entry_position(e);
            continue;
        }
        if (!pakon_dx_code_decode(e, &c))
            continue;
        if (!c.valid) {
            r->rejected++;
            continue;
        }
        r->codes++;
        unsigned f;
        for (f = 0; f < r->films; f++)
            if (r->film[f].product == c.product &&
                r->film[f].generation == c.generation)
                break;
        if (f == r->films && r->films < PAKON_DX_ROLL_FILMS) {
            r->film[f].product = c.product;
            r->film[f].generation = c.generation;
            r->films++;
        }
        if (f < r->films)
            r->film[f].votes++;
        if (r->stored < PAKON_DX_ROLL_MAX) {
            pakon_dx_roll_code *rc = &r->code[r->stored++];
            rc->frame_raw = c.frame_raw;
            rc->slot = c.slot;
            rc->position = r->last_position;
        }
        r->last_position = -1;
    }
}

int pakon_dx_roll_film(const pakon_dx_roll *r, uint8_t *product,
                       uint8_t *generation)
{
    unsigned best = 0;
    for (unsigned f = 1; f < r->films; f++)
        if (r->film[f].votes > r->film[best].votes)
            best = f;
    if (r->films == 0)
        return 0;
    if (product)
        *product = r->film[best].product;
    if (generation)
        *generation = r->film[best].generation;
    return 1;
}

const char *pakon_dx_roll_format(const pakon_dx_roll *r, char *buf, size_t size)
{
    uint8_t prod, gen;
    char first[8], last[8];
    if (!pakon_dx_roll_film(r, &prod, &gen)) {
        snprintf(buf, size, "no code read (%u sensor reads, %u entries, "
                 "%u rejected)", r->blocks, r->entries, r->rejected);
        return buf;
    }
    snprintf(buf, size, "film %u/%u, %u codes (%u rejected), frames %s..%s, "
             "%u sensor reads", prod, gen, r->codes, r->rejected,
             r->stored ? pakon_dx_frame_label(r->code[0].frame_raw, first, sizeof first) : "-",
             r->stored ? pakon_dx_frame_label(r->code[r->stored - 1].frame_raw, last, sizeof last) : "-",
             r->blocks);
    return buf;
}
