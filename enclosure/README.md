# ESP32-S3-CAM Enclosure

Parametric, friction-fit, snap-together 3D-printable case for the
[nulllaborg ESP32-S3-CAM](https://github.com/nulllaborg/esp32s3-cam).
No screws, no metal inserts — the four PCB mount posts double as the
case-closure mechanism.

Designed in OpenSCAD. Single source file: `esp32s3-cam-case.scad`.
Construction idiom borrowed from `../../hydro-dash/enclosure/hydro-dash-case.scad`.

## Quick start

1. Open `esp32s3-cam-case.scad` in OpenSCAD.
2. Set `mode = "assembly"` and render (F5) — confirm the PCB outline,
   USB-C cutout, and lens window all look reasonable.
3. Set `mode = "back"`, render (F6), export STL.
4. **Test fit first:** print the back shell at 0% infill, 2 perimeters.
   Drop your bare PCB in. The board should land flat on the four post
   shoulders with the posts visible through the M2 mounting holes.
5. Set `mode = "lid"`, export STL, print, test the friction fit.
6. Re-print both at production settings (see below).

## Tuning the friction fit

The lid clicks onto the back shell because four hollow tubes inside
the lid slide over the four post shafts on the back. The interference
is set by `FRONT_BORE_DIAM` vs. `PCB_POST_DIAM`:

- **Too loose / lid falls off:** decrease `FRONT_BORE_DIAM` by 0.05 mm.
- **Too tight / lid won't seat:** increase `FRONT_BORE_DIAM` by 0.05 mm.

The default `FRONT_BORE_DIAM = 1.8` over a `PCB_POST_DIAM = 1.7` shaft
gives 0.1 mm light interference, which is a good starting point for
PETG. PLA prints slightly oversize and may need an extra 0.05 mm bore.

## Print settings

| Setting | Value | Why |
|---|---|---|
| Material | **PETG** | Better fatigue life than PLA at the friction-fit tubes. |
| Layer height | 0.20 mm | Compromise; 0.16 mm if you want a cleaner lens window. |
| Perimeters | 3 | Strength on the post tubes. |
| Infill | 20 % | More than enough for a case this small. |
| Orientation | Open face down | Both halves: print with the cavity-opening face on the bed. |
| Supports | None | Geometry is support-free in the recommended orientation. |

## Feature toggles

All optional features are wired up but disabled by default. Flip the
boolean at the top of the SCAD file to enable:

- `USE_USB_C` — on (right edge)
- `USE_FLASH_HOLES` — on (two 2 mm holes flanking the lens)
- `USE_SD_SLOT` — bottom-edge cutout for microSD access
- `USE_BUTTON_HOLES` — pinholes for BOOT/RESET (paperclip access)
- `USE_IPEX_CUTOUT` — slot for external antenna pigtail
- `USE_BATTERY_CUTOUT` — slot for JST PH2.0 cable
- `USE_VENTS` — slot grid on back face
- `USE_KEYHOLE` — wall-mount keyhole on back face

## Verifying against your board

Every dimension in the file that depends on the actual board is
parametric. The defaults come from the size drawing
(`picture/esp32s3_cam_size.png` in the nulllaborg repo). Before
committing to a final print, double-check at least:

- `PCB_LEN`, `PCB_WID`, `PCB_THK`
- `MOUNT_HOLE_INSET` and the hole diameter
- USB-C: `USB_C_Y_CENTER` and `USB_C_Z_OFFSET`. The cutout W/H are
  deliberately sized for **cable overmold clearance** (~11 × 6.5 mm),
  not the connector body — leave them generous so any USB-C cable
  fits. The cutout straddles the seam so the opening is in both halves.
- `FLASH_LED_PITCH` and `FLASH_LED_X_FROM_EDGE` — the two flash LEDs
  sit on the short edge opposite the USB-C, flanking the camera FFC
  along the Y axis.
- `LENS_HOLE_X` / `LENS_HOLE_Y` — depends on how you fold the camera FFC.

Note: the Prusa Firmware-ESP32-Cam README claims this board is a
"dimensional copy" of the AiThinker ESP32-CAM. **It is not.**
AiThinker is 40.5 × 27 mm; this board is 38.4 × 30.4 mm with a
different hole pattern. Don't reuse an AiThinker enclosure.
