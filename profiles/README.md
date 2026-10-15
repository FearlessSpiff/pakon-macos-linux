# Colour profiles & correction data

Kodak/Pakon colour-correction data extracted from the OEM F-X35 COM SERVER
install (`Config/ColorCorrection/`), used by `tools/pakon_image.py` to reproduce
the OEM's image processing. © Eastman Kodak Company.

## Used today

- **`rpd.pf`** — "RPD Rendering Profile" (a `RPD_dls_3` + `yellow5` ICC cascade).
  The OEM "Use Color Correction (12-bit RPD)" stage. Applied by `--jpeg` via
  littleCMS to render the vibrant positive. Default for `--rpd-profile`.
- **`srgb.pf`** — sRGB IEC61966-2.1 output profile (the OEM output colourspace).
- **`_ClientColNegLut.txt`** — the C-41 negative inversion curve. Fit exactly to
  `out = 3500*log10(16383/in)`; this formula is baked into `pakon_image.py`
  (`_c41_lut`). Kept here as the source of truth / provenance.
- **`_ClientColNegMat.txt`** — ColNeg 3×3 + offset crosstalk matrix (the
  `_C41_MAT`/`_C41_OFF` constants; off by default — over-subtracts G/B here).
- **`Defaults.ini`** — per-film-product colour-slider defaults (mostly neutral).
- **`ColRevLut1.pf`** — the slide path's device link, "Pakon CMY to ROMM12"
  (ICC lut16): PosMatrix output → 12-bit ROMM. `--slide` / `render_slide`.
- **`romm.pf`** — ROMM RGB input profile; the slide path's `CRInputProfile`.

## Available for future use

- **`satplus03..15.pf` / `satminus03..15.pf`** — saturation-variant rendering
  profiles (could back a `--saturation` option).
- **`unity.pf`** — alternate output colourspace (Kodak Lab).
- **`cold_bw.pf`, `warm_bw_ld0_9_22.pf`, `sepia_*.pf`** — B&W / sepia looks.
- **`ColRevLutS6.lut`** — a 4096-entry 12-bit tone curve. `PIBegin` loads it
  into a global that nothing in `PakonIMAu.dll` reads, so the OEM never applies
  it.

Re-extract from the OEM install if needed; see the `[[oem-software-cache]]`
memory for the location.
