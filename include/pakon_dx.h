/*
 * pakon_dx.h — the DX sensor block, PICL register 0x90 (pure, no I/O).
 *
 * The event service reads `01 03 LOW 1e 90` after each LOW event; the reply
 * carries 30 bytes after its 4-byte header (`01 20 <LOW> 08`). Layout from
 * pakon-reference dx-barcode.md, checked against our captures:
 *
 *   bytes 0-1  position counter, big-endian
 *   byte  2    entry count
 *   bytes 3..  entries, 5 bytes each; type in the low nibble of byte 0
 *
 * Entry types seen: 0 (none), 3 (decoded barcode), 5 (code position), 7 and
 * 8 (sensor edge events). Types 5/7/8 carry a flag in byte 1 and a 16-bit
 * big-endian position in bytes 2-3.
 *
 * Serial 3054 decodes the barcode (resources/36frames.pcapng, 69 type-3
 * codes, all with good parity, product 97 / generation 10, 0A to 36A). The
 * F-135+ the reference was written on never produced a type 3. On our unit
 * byte 1 of a type 3 stays the same through a roll (0x01 on two rolls, 0x02
 * on the third, which reads its frame numbers counting down), so it does not
 * mark the "A" code, as the reference reads it; which sensor read the code is
 * a guess.
 */
#ifndef PAKON_DX_H
#define PAKON_DX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAKON_DX_REPLY_LEN   30u   /* payload after the reply header */
#define PAKON_DX_MAX_ENTRIES 5u    /* 3 + 5 * 5 = 28 of the 30 bytes */

enum {
    PAKON_DX_NONE = 0,
    PAKON_DX_CODE = 3,
    PAKON_DX_POSITION = 5,
    PAKON_DX_EDGE_A = 7,
    PAKON_DX_EDGE_B = 8
};

typedef struct {
    uint8_t type;    /* low nibble of byte 0 */
    uint8_t hi;      /* high nibble of byte 0 (meaning unknown) */
    uint8_t flag;    /* byte 1 */
    uint8_t b[3];    /* bytes 2-4 */
} pakon_dx_entry;

typedef struct {
    uint16_t position;
    uint8_t count;
    pakon_dx_entry e[PAKON_DX_MAX_ENTRIES];
} pakon_dx_block;

/* Parse a 30-byte payload. Returns 0, or -1 if `len` is short or the count
 * does not fit. */
int pakon_dx_parse(const uint8_t *payload, size_t len, pakon_dx_block *out);

/* 16-bit position of a type 5/7/8 entry. */
uint16_t pakon_dx_entry_position(const pakon_dx_entry *e);

/*
 * A decoded barcode (type 3). Bytes 2-4:
 *   byte 2: bits 7-2 frame_raw[5:0]; bit 1 must be 0; bit 0 parity
 *   byte 3: bits 7-6 product[1:0]; bit 5 invalid; bits 4-1 generation;
 *           bit 0 frame_raw[6]
 *   byte 4: bits 4-0 product[6:2]
 * Parity is even over the 19 bits of bytes 2, 3 and 4[4:0].
 * frame_raw counts half frames (frame * 2, + 1 for the "A" code).
 */
typedef struct {
    uint8_t frame_raw;   /* 0..127 */
    uint8_t product;     /* DX part 1, 0..127 */
    uint8_t generation;  /* DX part 2, 0..15 */
    uint8_t slot;        /* entry byte 1 */
    int parity_ok;
    int valid;           /* parity good, invalid bit and byte 2 bit 1 clear */
} pakon_dx_code;

/* Returns 1 and fills `c` if `e` is a type 3, else 0. */
int pakon_dx_code_decode(const pakon_dx_entry *e, pakon_dx_code *c);

/* Frame label of a frame_raw value: "12", "12A"; 124/125 print "X"/"XA" and
 * 126/127 "00"/"00A", the leader labels (reference: -4 and -2 in 7-bit
 * two's complement). `buf` needs 5 bytes. */
const char *pakon_dx_frame_label(uint8_t frame_raw, char *buf, size_t size);

/* One line describing a block, for logs. Returns `buf`. */
const char *pakon_dx_format(const pakon_dx_block *b, char *buf, size_t size);

/*
 * Codes collected over a scan. Each code keeps the position of the type 5
 * entry before it (the engine takes a code's position from there), or -1.
 */
#define PAKON_DX_ROLL_MAX     160u
#define PAKON_DX_ROLL_FILMS   8u

typedef struct {
    uint8_t frame_raw;
    uint8_t slot;
    int32_t position;
} pakon_dx_roll_code;

typedef struct {
    unsigned blocks, entries, codes, rejected;
    unsigned type_count[16];
    struct { uint8_t product, generation; unsigned votes; } film[PAKON_DX_ROLL_FILMS];
    unsigned films;
    pakon_dx_roll_code code[PAKON_DX_ROLL_MAX];
    unsigned stored;
    int32_t last_position;   /* last type 5 position, -1 if none yet */
} pakon_dx_roll;

void pakon_dx_roll_init(pakon_dx_roll *r);
void pakon_dx_roll_add(pakon_dx_roll *r, const pakon_dx_block *b);

/* Product and generation read most often. Returns 0 if no valid code. */
int pakon_dx_roll_film(const pakon_dx_roll *r, uint8_t *product,
                       uint8_t *generation);

/* One line summarising the roll, for logs. Returns `buf`. */
const char *pakon_dx_roll_format(const pakon_dx_roll *r, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* PAKON_DX_H */
