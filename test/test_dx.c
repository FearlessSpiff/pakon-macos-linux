/*
 * test_dx — the DX sensor block (PICL 0x90), no hardware.
 *
 * Fixtures in test/fixtures/dx are every 0x90 reply payload in three OEM
 * captures from serial 3054 (resources/<name>.pcapng): two full rolls, which
 * decode as product 97 / generation 10 and 96 / 12, and one short strip,
 * 82 / 3 (KODAK GOLD 400 in the OEM table), whose frame numbers count down.
 */
#include "pakon_dx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) do {                                   \
        if (cond) { printf("ok   - %s\n", (msg)); }             \
        else { printf("FAIL - %s\n", (msg)); failures++; }      \
    } while (0)

static int unhex(const char *s, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        if (sscanf(s + 2 * i, "%2x", &v) != 1)
            return -1;
        out[i] = (uint8_t)v;
    }
    return 0;
}

/* Feed every payload in `name` to a roll. Returns the number read, -1 on a
 * missing file, -2 on a parse failure. */
static int load_roll(const char *dir, const char *name, pakon_dx_roll *roll)
{
    char path[512], line[128];
    snprintf(path, sizeof path, "%s/%s.hex", dir, name);
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    int n = 0;
    pakon_dx_roll_init(roll);
    while (fgets(line, sizeof line, f)) {
        uint8_t p[PAKON_DX_REPLY_LEN];
        pakon_dx_block b;
        if (line[0] == '#' || line[0] == '\n')
            continue;
        if (unhex(line, p, sizeof p) || pakon_dx_parse(p, sizeof p, &b)) {
            fclose(f);
            return -2;
        }
        pakon_dx_roll_add(roll, &b);
        n++;
    }
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "test/fixtures/dx";
    char msg[256], lab[8];
    uint8_t p[PAKON_DX_REPLY_LEN];
    pakon_dx_block b;
    pakon_dx_code c;

    /* 36frames.pcapng: position 0x73a9, five entries, code 0A first. */
    unhex("73a9053301045418050153dbcc030108541805016031cc03010d54180000", p, 30);
    CHECK(pakon_dx_parse(p, sizeof p, &b) == 0, "parse a five-entry block");
    CHECK(b.position == 0x73a9 && b.count == 5, "position is big-endian, count 5");
    CHECK(b.e[0].type == 3 && b.e[0].hi == 3 && b.e[1].type == 5,
          "type in the low nibble, high nibble kept apart");
    CHECK(pakon_dx_entry_position(&b.e[1]) == 0x53db, "type 5 position 0x53db");
    CHECK(pakon_dx_code_decode(&b.e[0], &c) == 1, "type 3 decodes");
    CHECK(c.frame_raw == 1 && c.product == 97 && c.generation == 10 &&
          c.slot == 1 && c.valid, "code 0A, product 97 / generation 10");
    CHECK(pakon_dx_code_decode(&b.e[1], &c) == 0, "type 5 is not a code");

    /* pakon_scan.pcapng: slot 0x02, frame_raw 12 (frame 6), 82 / 3. */
    unhex("37b205b5021343cc030231861405021e9ecc03022c861405022b5bcc0000", p, 30);
    pakon_dx_parse(p, sizeof p, &b);
    pakon_dx_code_decode(&b.e[1], &c);
    CHECK(c.frame_raw == 12 && c.product == 82 && c.generation == 3 &&
          c.slot == 2 && c.valid, "code 6, product 82 / generation 3, slot 2");

    /* Flip one data bit: parity fails. */
    b.e[1].b[2] ^= 0x01;
    pakon_dx_code_decode(&b.e[1], &c);
    CHECK(!c.parity_ok && !c.valid, "a flipped bit fails parity");

    /* The invalid bit set (and parity kept even by flipping bit 0 of byte 2). */
    b.e[1].b[2] ^= 0x01;
    b.e[1].b[1] ^= 0x20;
    b.e[1].b[0] ^= 0x01;
    pakon_dx_code_decode(&b.e[1], &c);
    CHECK(c.parity_ok && !c.valid, "the invalid bit rejects a code");

    /* Idle and edge blocks. */
    unhex("be7401000000000000000000000000000000000000000000000000000000", p, 30);
    CHECK(pakon_dx_parse(p, sizeof p, &b) == 0 && b.count == 1 &&
          b.e[0].type == PAKON_DX_NONE, "idle reply: one type 0 entry");
    unhex("08ff01370208f7cc00000000000000000000000000000000000000000000", p, 30);
    pakon_dx_parse(p, sizeof p, &b);
    CHECK(b.e[0].type == PAKON_DX_EDGE_A && b.e[0].flag == 0x02 &&
          pakon_dx_entry_position(&b.e[0]) == 0x08f7, "type 7, flag 0x02, 0x08f7");
    unhex("00ff06000000000000000000000000000000000000000000000000000000", p, 30);
    CHECK(pakon_dx_parse(p, sizeof p, &b) == -1, "a count past five is refused");
    CHECK(pakon_dx_parse(p, 29, &b) == -1, "a short payload is refused");

    /* Labels. */
    CHECK(!strcmp(pakon_dx_frame_label(73, lab, sizeof lab), "36A"), "73 is 36A");
    CHECK(!strcmp(pakon_dx_frame_label(2, lab, sizeof lab), "1"), "2 is 1");
    CHECK(!strcmp(pakon_dx_frame_label(126, lab, sizeof lab), "00"), "126 is 00");
    CHECK(!strcmp(pakon_dx_frame_label(124, lab, sizeof lab), "X"), "124 is X");

    /* Whole rolls. */
    static const struct {
        const char *name;
        int reads;
        unsigned codes, product, generation;
        int first, last;
    } rolls[] = {
        { "36frames",       38, 69, 97, 10,  1, 73 },
        { "pakon_fullroll", 25, 45, 96, 12,  1, 49 },
        { "pakon_scan",      9,  5, 82,  3, 12,  8 },
    };
    for (size_t i = 0; i < sizeof rolls / sizeof rolls[0]; i++) {
        static pakon_dx_roll r;
        uint8_t prod = 0, gen = 0;
        int n = load_roll(dir, rolls[i].name, &r);
        snprintf(msg, sizeof msg, "%s: %d sensor reads (want %d)", rolls[i].name,
                 n, rolls[i].reads);
        CHECK(n == rolls[i].reads, msg);
        if (n <= 0)
            continue;
        pakon_dx_roll_film(&r, &prod, &gen);
        snprintf(msg, sizeof msg, "%s: %u codes, 0 rejected, film %u/%u",
                 rolls[i].name, r.codes, prod, gen);
        CHECK(r.codes == rolls[i].codes && r.rejected == 0 && r.films == 1 &&
              prod == rolls[i].product && gen == rolls[i].generation, msg);
        snprintf(msg, sizeof msg, "%s: frame_raw %d..%d", rolls[i].name,
                 r.code[0].frame_raw, r.code[r.stored - 1].frame_raw);
        CHECK(r.code[0].frame_raw == rolls[i].first &&
              r.code[r.stored - 1].frame_raw == rolls[i].last, msg);
        /* Each code but a block-leading one follows a type 5 in its block. */
        unsigned placed = 0;
        for (unsigned k = 0; k < r.stored; k++)
            placed += r.code[k].position >= 0;
        snprintf(msg, sizeof msg, "%s: %u of %u codes have a preceding type 5",
                 rolls[i].name, placed, r.stored);
        CHECK(placed > 0, msg);
        printf("     %s\n", pakon_dx_roll_format(&r, msg, sizeof msg));
    }

    /* Within a block the codes and the type 5 positions alternate, one per
     * half frame: 36frames 0x53db -> 0x6031 is 3158 counts. */
    unhex("73a9053301045418050153dbcc030108541805016031cc03010d54180000", p, 30);
    pakon_dx_parse(p, sizeof p, &b);
    {
        static pakon_dx_roll r;
        pakon_dx_roll_init(&r);
        pakon_dx_roll_add(&r, &b);
        CHECK(r.stored == 3 && r.code[0].position == -1 &&
              r.code[1].position == 0x53db && r.code[2].position == 0x6031,
              "a code takes the position of the type 5 before it");
    }

    if (failures)
        printf("%d failure(s)\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
