/*
 * pakon_calcache.h — when to recalibrate, and the saved calibration.
 *
 * TLB.dll does not calibrate before every scan (FN_bBeforeScan 1002dbd0,
 * docs/TLB_FINDINGS.md). It keeps one calibration record per scan setting
 * and film type, with two timestamps: the last calibration of any kind and
 * the last full one (FindCorrections 10021590 stamps the first every time,
 * the second only on a full run). Before a scan:
 *
 *   - no record, or the last full calibration is more than 72000 s (20 h)
 *     old (FUN_10010250): full calibration, LED current search included;
 *   - else the last calibration is more than DarkPointCorrectIntervalMinutes
 *     old (default 60, settings constructor FUN_10011fd0): calibration
 *     without the current search, from the saved currents;
 *   - else: no calibration, the saved values are applied.
 *
 * The OEM keeps the record in the registry, so it outlives the program. We
 * keep it in a file next to the EEPROM archive, one per film type (the duty
 * caps depend on the film-base densities).
 */
#ifndef PAKON_CALCACHE_H
#define PAKON_CALCACHE_H

#include <stddef.h>
#include <stdint.h>

#include "pakon_lightcal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAKON_CAL_INTERVAL_S  (60 * 60)   /* DarkPointCorrectIntervalMinutes = 60 */
#define PAKON_CAL_FULL_S      72000       /* full calibration at least every 20 h */

typedef enum {
    PAKON_CAL_REUSE = 0,    /* apply the saved values, no calibration */
    PAKON_CAL_REFRESH,      /* calibrate from the saved currents */
    PAKON_CAL_FULL          /* calibrate from scratch */
} pakon_cal_action;

const char *pakon_cal_action_name(pakon_cal_action a);  /* "reuse" / ... */

/*
 * What to do before a scan at `now`, given the record's `when` (last
 * calibration) and `when_full` (last full calibration), all time() seconds.
 * A record from the future (the clock went back) counts as stale: full.
 */
pakon_cal_action pakon_cal_needed(int64_t now, int64_t when, int64_t when_full);

/* One calibration: the light-calibration result and what it is valid for. */
typedef struct {
    unsigned   serial;              /* EEPROM serial */
    unsigned   offset;              /* EEPROM Base 16 Offset */
    pakon_film film;
    int64_t    when, when_full;     /* time() of the last / last full calibration */
    int        gain[3], off[3];     /* A/D gain codes and offsets, R, G, B */
    unsigned   cur[4];              /* LED currents, R, G, B, IR */
    uint16_t   duty[4];             /* open-gate duties, R, G, B, IR */
    /* Fixed-pattern tables, [channel][pixel] (PAKON_LC_NCH * line_px). */
    size_t     line_px, lines_dark, lines_bright;
    double    *dark, *bright;
} pakon_cal_record;

/* `dir`/calibration-<film>.bin. Returns 0, or -1 if it does not fit. */
int pakon_cal_path(char *buf, size_t size, const char *dir, pakon_film film);

/* Write the record (tables required). Returns 0 on success. */
int pakon_cal_save(const char *path, const pakon_cal_record *rec);

/*
 * Read a record and check it is for this scanner, film and line: returns 0
 * with the tables allocated (free with pakon_cal_free), or -1 when the file
 * is missing, unreadable, from another version, or does not match `serial`,
 * `offset`, `film` and `line_px`.
 */
int pakon_cal_load(const char *path, unsigned serial, unsigned offset,
                   pakon_film film, size_t line_px, pakon_cal_record *rec);

void pakon_cal_free(pakon_cal_record *rec);

#ifdef __cplusplus
}
#endif

#endif /* PAKON_CALCACHE_H */
