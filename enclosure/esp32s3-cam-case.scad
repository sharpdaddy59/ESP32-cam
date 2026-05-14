// esp32s3-cam-case.scad — parametric friction-fit enclosure for the
// nulllaborg ESP32-S3-CAM (https://github.com/nulllaborg/esp32s3-cam).
//
// Author note: every dimension that depends on the actual board is
// flagged "MEASURE" — take calipers to your unit before printing the
// final pieces. The defaults below come from the size drawing in the
// nulllaborg repo (picture/esp32s3_cam_size.png); the very first thing
// to do is print the back_shell at 0% infill, drop your bare board in,
// and confirm it lands cleanly on the four post shoulders.
//
// Construction:
//   - Two-piece body: back_shell (USB cutout) and lid (lens window,
//     flash LED holes). They join via mating posts (lid) and shafts
//     (back) with a tunable friction fit — no screws, no snap hooks.
//   - PCB is captured by four stepped posts rising from the back-shell
//     cavity floor: a wide shoulder under the PCB acts as a positive
//     vertical stop the board lands on, and a narrower shaft above
//     passes through the M2 mounting holes and protrudes up into the
//     lid's bores.
//   - Lens window in the lid lets the FFC-mounted camera lens look
//     out the top. Two small holes flanking the lens align with the
//     GPIO3 flash LEDs on the PCB.
//   - A pry slot at one corner of the seam accepts a flat tool to
//     re-open the case.
//
// Top-of-file `mode` selects what to render:
//   "lid" / "back" / "assembly" / "exploded"

mode = "lid";

// =============================================================
// FEATURE TOGGLES — flip these to include/exclude optional parts.
// =============================================================
USE_USB_C          = true;    // USB-C cutout on the right short edge
USE_FLASH_HOLES    = true;    // 2 holes in lid for the GPIO3 flash LEDs
USE_SD_SLOT        = false;   // microSD slot on bottom long edge
USE_BUTTON_HOLES   = false;   // BOOT + RESET pinholes (paperclip access)
USE_IPEX_CUTOUT    = false;   // slot for external antenna pigtail
USE_BATTERY_CUTOUT = false;   // slot for JST PH2.0 battery cable
USE_VENTS          = false;   // vent slot grid on back face
USE_KEYHOLE        = false;   // wall-mount keyhole on back face

// =============================================================
// PARAMETERS — tune these to match your board and your printer.
// Almost every issue you'll hit on first print is fixable here
// without touching the geometry below.
// =============================================================

// ----- PCB ----------------------------------------------------
// From picture/esp32s3_cam_size.png. MEASURE on your board.
PCB_LEN              = 40.0;   // long edge of the PCB
PCB_WID              = 27.0;   // short edge
PCB_THK              =  1.6;   // PCB thickness alone
MOUNT_HOLE_DIAM      =  2.7;   // M2 clearance (through-hole on PCB)
MOUNT_HOLE_INSET     =  2.0;   // centre-of-hole to nearest PCB edge

// ----- Camera lens window (in the lid) ------------------------
// The camera module floats on a 24-pin FFC, so its position is
// mechanically flexible. The lens folds over the WROOM-1 module and
// looks straight up through this hole. MEASURE/ADJUST as needed.
LENS_HOLE_DIAM       =  10.0;   // ~OV2640/OV3660 lens barrel clearance
LENS_HOLE_X          = 11.5;   // PCB-local X of hole centre
LENS_HOLE_Y          = PCB_WID / 2;   // PCB-local Y of hole centre

// ----- Flash LED holes (in the lid) ---------------------------
// Two white SMD flash LEDs on the PCB top side, flanking the camera
// FFC connector along the SHORT edge of the PCB opposite the USB-C
// (i.e. the -X short edge). The LEDs sit along that short edge with
// the FFC connector between them. MEASURE on your board.
FLASH_LED_HOLE_DIAM   =  2.0;
FLASH_LED_PITCH       = -23.0;  // centre-to-centre distance between LEDs (along Y)
FLASH_LED_X_FROM_EDGE =  9.0;  // distance from PCB -X edge to LED centres

// ----- USB-C connector (PCB +X short edge, surface-mount) -----
// "Y position" = distance from PCB -Y edge to centre of connector.
// The cutout sizes below are CABLE OVERMOLD clearances, not the
// connector body — they're deliberately larger than the connector so
// a typical USB-C cable's overmolded plug can be inserted. The
// connector body itself is ~8.94 × 3.26 mm, but cables won't fit if
// the opening is only that size.
// MEASURE on your board; bump up if your cable still won't fit.
USB_C_WIDTH          = 9.0;   // cable-overmold clearance, width
USB_C_HEIGHT         =  5.0;   // cable-overmold clearance, height
USB_C_Y_CENTER       = PCB_WID / 2;   // centred on short edge
USB_C_Z_OFFSET       =  3.2;   // centre relative to PCB BOTTOM (positive = above PCB)
USB_CUTOUT_SLACK     =  0.6;   // extra clearance around the cutout

// ----- microSD slot (bottom long edge, optional) --------------
SD_SLOT_WIDTH        = 14.0;
SD_SLOT_HEIGHT       =  2.0;
SD_SLOT_X_CENTER     = 24.0;   // PCB-local X of slot centre — MEASURE
SD_SLOT_Z_OFFSET     =  2.5;   // centre relative to PCB BOTTOM
SD_CUTOUT_SLACK      =  0.8;

// ----- Button pinholes (BOOT/RESET, optional) -----------------
BUTTON_HOLE_DIAM     =  2.0;
BOOT_BTN_X_FROM_EDGE =  3.0;   // BOOT: PCB +X, -Y corner; MEASURE
BOOT_BTN_Y_FROM_EDGE =  3.0;
RESET_BTN_X_FROM_EDGE = 3.0;   // RESET: PCB +X, +Y corner; MEASURE
RESET_BTN_Y_FROM_EDGE = 3.0;
BUTTON_Z_OFFSET      =  2.5;   // centre above PCB bottom

// ----- IPEX antenna cutout (optional) -------------------------
IPEX_CUTOUT_W        =  6.0;
IPEX_CUTOUT_H        =  3.0;
IPEX_Z_OFFSET        =  2.5;

// ----- Battery JST PH2.0 cutout (optional) --------------------
BATTERY_CUTOUT_W     =  6.5;
BATTERY_CUTOUT_H     =  3.5;
BATTERY_X_CENTER     =  8.0;   // PCB-local X — MEASURE
BATTERY_Z_OFFSET     =  2.5;

// ----- Case body / fit ----------------------------------------
WALL                 =  2.0;
PCB_PERIMETER_GAP    =  0.4;   // case-to-PCB tolerance (each side)
BACK_DEPTH           =  3.0;   // space below PCB (bottom of S3-CAM is flat)
FRONT_DEPTH          = 7.0;   // space above PCB (camera lens stack-up)
CASE_FILLET          =  2.0;   // outer corner rounding
$fn                  = 64;

// ----- PCB-retention posts (scaled for M2 — see hydro-dash for
// the same idiom with M2.5). Stepped post: wider SHOULDER below
// the PCB acts as a positive stop the board sits on (so you can't
// push it past), transitioning into a narrower shaft that passes
// through the M2 mounting hole and continues up past the PCB top
// by PCB_POST_PROTRUDE — far enough for the lid's mating tubes to
// slide over and create the friction fit that holds the case shut.
//
// Cross-section profile:
//                             ___       <- chamfered tip
//                            |   |
//                            |   |       <- upper shaft (through PCB,
//                            |   |          PCB_POST_DIAM dia)
//                       _____|___|_____  <- PCB rests here on shoulder
//                      |               |
//                      |               | <- shoulder
//                      |               |    (PCB_POST_SHOULDER_DIAM dia)
//                      |               |
//                      ----------------- <- back-shell cavity floor
//
// Shoulder diameter is smaller than the hydro-dash reference (4.5mm)
// because the S3-CAM's MOUNT_HOLE_INSET is only 3.2mm; a 4.5mm
// shoulder would overhang the PCB edge.
PCB_POST_SHOULDER_DIAM =  3.6; // wider seat the PCB rests on
PCB_POST_DIAM          =  2.0; // upper shaft, through M2 hole fit
PCB_POST_PROTRUDE      =  3.5; // shaft extends above PCB top to engage lid bore

// ----- Lid mating posts ---------------------------------------
// Hollow cylindrical tubes hanging from the lid ceiling, one per PCB
// mounting-hole position. Each slides OVER the protruding tip of the
// corresponding back-shell post. Friction between the bore and the
// back post shaft is what holds the case together — there are no
// snap hooks.
//
// Tune FRONT_BORE_DIAM until you get the click feel you want:
//   - too loose? decrease (toward PCB_POST_DIAM = 1.7)
//   - too tight? increase by 0.05 mm at a time
FRONT_POST_OD        =  3.6;
FRONT_BORE_DIAM      =  2.2;   // light interference with 1.7mm shaft
FRONT_POST_LEN       =  6.5;
FRONT_BORE_TOP_GAP   =  1.0;

// ----- Pry slot (for disassembly) -----------------------------
// Open slot at one corner of the seam — accepts a flat tool to twist
// the two halves apart when you need to re-open the case.
PRY_SLOT_W           =  5.0;
PRY_SLOT_D           =  1.5;

// ----- Wall mount keyhole (optional) --------------------------
KEYHOLE_DIAM_BIG     =  8.0;
KEYHOLE_DIAM_SMALL   =  4.0;
KEYHOLE_SLOT_LEN     =  6.0;
KEYHOLE_DEPTH        =  1.2;
KEYHOLE_OFFSET_FROM_TOP = 8.0;

// ----- Vent slots (optional) ----------------------------------
VENT_SLOT_LEN        = 12.0;
VENT_SLOT_W          =  1.2;
VENT_ROWS            =  2;
VENT_COLS            =  3;
VENT_PITCH_X         =  3.0;
VENT_PITCH_Y         =  2.5;

// =============================================================
// DERIVED CONSTANTS — usually no need to touch
// =============================================================
INNER_X              = PCB_LEN + 2*PCB_PERIMETER_GAP;
INNER_Y              = PCB_WID + 2*PCB_PERIMETER_GAP;
OUTER_X              = INNER_X + 2*WALL;
OUTER_Y              = INNER_Y + 2*WALL;
TOTAL_HEIGHT         = WALL + BACK_DEPTH + PCB_THK + FRONT_DEPTH + WALL;
SEAM_Z               = WALL + BACK_DEPTH + PCB_THK;  // lid meets back here

// Mount-hole positions (PCB local coords). Order: BL, BR, TR, TL.
mount_hole_xy = [
  [MOUNT_HOLE_INSET,           MOUNT_HOLE_INSET],
  [PCB_LEN - MOUNT_HOLE_INSET, MOUNT_HOLE_INSET],
  [PCB_LEN - MOUNT_HOLE_INSET, PCB_WID - MOUNT_HOLE_INSET],
  [MOUNT_HOLE_INSET,           PCB_WID - MOUNT_HOLE_INSET],
];

// =============================================================
// HELPER MODULES
// =============================================================

// 2D rounded square -> extruded box with filleted vertical edges
module rounded_box(x, y, z, r=CASE_FILLET) {
  linear_extrude(z)
    offset(r=r) offset(r=-r) square([x, y]);
}

// PCB alignment post — stepped: wide shoulder below PCB, narrow shaft
// through PCB and above. The shoulder's top face is the surface the
// PCB rests on (positive vertical stop, can't be pushed past).
module pcb_post(h_below_pcb, h_above_pcb) {
  upper_h = PCB_THK + h_above_pcb;
  // Shoulder (below PCB)
  cylinder(d=PCB_POST_SHOULDER_DIAM, h=h_below_pcb);
  // Upper shaft (through PCB hole + protrude into lid bore)
  translate([0, 0, h_below_pcb]) {
    cylinder(d=PCB_POST_DIAM, h=upper_h - 0.6);
    // Chamfered tip — narrows over the top 0.6mm so the bore (and the
    // PCB hole on insertion) can self-centre on the post.
    translate([0, 0, upper_h - 0.6])
      cylinder(d1=PCB_POST_DIAM, d2=PCB_POST_DIAM - 0.6, h=0.6);
  }
}

// USB connector cutout — runs through the +X short wall.
// y_center: PCB-local Y of cutout centre. z_offset: vertical position
// relative to PCB BOTTOM surface (positive = above PCB bottom).
module usb_cutout(y_center, w, h, z_offset) {
  translate([PCB_LEN + PCB_PERIMETER_GAP - 0.5,
             y_center - w/2 - USB_CUTOUT_SLACK/2,
             WALL + BACK_DEPTH + z_offset - h/2 - USB_CUTOUT_SLACK/2])
    cube([WALL + 1.5,
          w + USB_CUTOUT_SLACK,
          h + USB_CUTOUT_SLACK]);
}

// microSD slot cutout — through the -Y long wall.
module sd_slot_cutout() {
  translate([SD_SLOT_X_CENTER - SD_SLOT_WIDTH/2 - SD_CUTOUT_SLACK/2,
             -WALL - PCB_PERIMETER_GAP - 0.5,
             WALL + BACK_DEPTH + SD_SLOT_Z_OFFSET - SD_SLOT_HEIGHT/2 - SD_CUTOUT_SLACK/2])
    cube([SD_SLOT_WIDTH + SD_CUTOUT_SLACK,
          WALL + 1.5,
          SD_SLOT_HEIGHT + SD_CUTOUT_SLACK]);
}

// IPEX antenna cutout — through the -X short wall, top-Y corner.
module ipex_cutout() {
  translate([-WALL - PCB_PERIMETER_GAP - 0.5,
             PCB_WID - IPEX_CUTOUT_W - 1.0,
             WALL + BACK_DEPTH + IPEX_Z_OFFSET - IPEX_CUTOUT_H/2])
    cube([WALL + 1.5, IPEX_CUTOUT_W, IPEX_CUTOUT_H]);
}

// JST PH2.0 battery connector cutout — through the -Y long wall.
module battery_cutout() {
  translate([BATTERY_X_CENTER - BATTERY_CUTOUT_W/2,
             -WALL - PCB_PERIMETER_GAP - 0.5,
             WALL + BACK_DEPTH + BATTERY_Z_OFFSET - BATTERY_CUTOUT_H/2])
    cube([BATTERY_CUTOUT_W, WALL + 1.5, BATTERY_CUTOUT_H]);
}

// BOOT/RESET pinhole — through the +X short wall at a given corner.
module button_pinhole(y_from_edge, name="boot") {
  // name = "boot" → -Y corner, "reset" → +Y corner
  y = (name == "reset") ? (PCB_WID - y_from_edge) : y_from_edge;
  translate([PCB_LEN + PCB_PERIMETER_GAP - 0.5,
             y,
             WALL + BACK_DEPTH + BUTTON_Z_OFFSET])
    rotate([0, 90, 0])
      cylinder(d=BUTTON_HOLE_DIAM, h=WALL + 1.5);
}

// Wall-mount keyhole (subtractive). Cuts through back face.
// Big circle on top (screw head slips in); slot extending in +Y so
// gravity catches the case on the screw shaft below.
module keyhole_negative() {
  cylinder(d=KEYHOLE_DIAM_BIG, h=WALL + 1);
  hull() {
    cylinder(d=KEYHOLE_DIAM_SMALL, h=WALL + 1);
    translate([0, KEYHOLE_SLOT_LEN, 0])
      cylinder(d=KEYHOLE_DIAM_SMALL, h=WALL + 1);
  }
  // Recess for screw head on the OUTER face (so head sits flush)
  translate([0, 0, -0.01])
    cylinder(d=KEYHOLE_DIAM_BIG + 1, h=KEYHOLE_DEPTH);
}

// Vent slot grid (subtractive)
module vent_grid_negative() {
  for (cx = [0 : VENT_COLS - 1])
    for (cy = [0 : VENT_ROWS - 1])
      translate([cx * VENT_PITCH_X, cy * (VENT_SLOT_W + VENT_PITCH_Y), 0])
        cube([VENT_SLOT_LEN, VENT_SLOT_W, WALL + 1]);
}

// =============================================================
// PCB DUMMY (visualization only)
// =============================================================
module pcb_dummy() {
  color("green", 0.7) {
    // PCB itself
    translate([0, 0, WALL + BACK_DEPTH])
      cube([PCB_LEN, PCB_WID, PCB_THK]);
    // WROOM-1 module sitting on top, roughly centred
    color("dimgray")
      translate([PCB_LEN/2 - 9.0, PCB_WID/2 - 8.0,
                 WALL + BACK_DEPTH + PCB_THK])
        cube([18.0, 16.0, 3.3]);
    // Camera lens stack, indicated as a small puck above the WROOM-1
    color("black")
      translate([LENS_HOLE_X, LENS_HOLE_Y,
                 WALL + BACK_DEPTH + PCB_THK + 3.3])
        cylinder(d=8.0, h=5.0);
  }
}

// =============================================================
// LID — viewer side. Camera lens window, flash LED holes, lid
// mating tubes pointing down, pry slot.
// =============================================================
module lid() {
  union() {
    difference() {
      // Outer body
      translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP, SEAM_Z])
        rounded_box(OUTER_X, OUTER_Y, FRONT_DEPTH + WALL);

      // Inside cavity — hollow out the underside of the lid
      translate([-PCB_PERIMETER_GAP, -PCB_PERIMETER_GAP, SEAM_Z - 0.01])
        cube([INNER_X, INNER_Y, FRONT_DEPTH + 0.02]);

      // Lens window — through the top face
      translate([LENS_HOLE_X, LENS_HOLE_Y,
                 SEAM_Z + FRONT_DEPTH - 0.01])
        cylinder(d=LENS_HOLE_DIAM, h=WALL + 1);

      // Flash LED holes — along the -X short edge of the PCB,
      // flanking the camera FFC connector along the Y axis.
      if (USE_FLASH_HOLES) {
        led_x = FLASH_LED_X_FROM_EDGE;
        for (dy = [-FLASH_LED_PITCH/2])
          translate([led_x, PCB_WID/2 + dy,
                     SEAM_Z + FRONT_DEPTH - 0.01])
            cylinder(d=FLASH_LED_HOLE_DIAM, h=WALL + 1);
      }

      // USB-C cutout — the connector sits ABOVE the PCB top, which is
      // the seam. The cutout cube straddles the seam: lower portion is
      // cut from the back shell, upper portion from the lid. Both
      // halves' difference() blocks subtract the same cube so the
      // opening passes cleanly through the seam.
      if (USE_USB_C)
        usb_cutout(USB_C_Y_CENTER, USB_C_WIDTH, USB_C_HEIGHT, USB_C_Z_OFFSET);

      // microSD / BOOT-RESET / IPEX / battery cutouts also straddle
      // the seam (the components sit on top of the PCB). Subtract them
      // from the lid as well as the back shell.
      if (USE_SD_SLOT)         sd_slot_cutout();
      if (USE_BUTTON_HOLES) {
        button_pinhole(BOOT_BTN_Y_FROM_EDGE, "boot");
        button_pinhole(RESET_BTN_Y_FROM_EDGE, "reset");
      }
      if (USE_IPEX_CUTOUT)     ipex_cutout();
      if (USE_BATTERY_CUTOUT)  battery_cutout();

      // Pry slot at one corner so a flat tool can split the seam.
      // Placed at the (-X, -Y) corner — opposite the USB-C side.
      translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP - 0.01,
                 SEAM_Z - 0.01])
        cube([PRY_SLOT_W, WALL + 0.5, PRY_SLOT_D]);
    }

    // Mating tubes — hollow cylinders hanging from the ceiling at each
    // PCB mounting-hole position. Each slides over the protruding tip
    // of the back-shell post; friction between the bore and the back
    // post shaft holds the case closed.
    for (p = mount_hole_xy)
      translate([p[0], p[1], SEAM_Z + FRONT_DEPTH - FRONT_POST_LEN])
        difference() {
          cylinder(d=FRONT_POST_OD, h=FRONT_POST_LEN);
          // Bore: open at the bottom, closed FRONT_BORE_TOP_GAP from top
          translate([0, 0, -0.01])
            cylinder(d=FRONT_BORE_DIAM,
                     h=FRONT_POST_LEN - FRONT_BORE_TOP_GAP + 0.01);
        }
  }
}

// =============================================================
// BACK SHELL — cable side. USB-C cutout (default), optional SD /
// button / antenna / battery / vent / keyhole features, and the
// four PCB-retention posts.
// =============================================================
module back_shell() {
  union() {
    // Outer body and all subtractive features. PCB-retention posts
    // are added AFTER this difference so the cavity cube doesn't
    // truncate them.
    difference() {
      // Outer body
      translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP, 0])
        rounded_box(OUTER_X, OUTER_Y, WALL + BACK_DEPTH + PCB_THK);

      // Inner cavity (hollow tray below the PCB seat)
      translate([-PCB_PERIMETER_GAP, -PCB_PERIMETER_GAP, WALL])
        cube([INNER_X, INNER_Y, BACK_DEPTH + PCB_THK + 0.01]);

      // USB-C cutout
      if (USE_USB_C)
        usb_cutout(USB_C_Y_CENTER, USB_C_WIDTH, USB_C_HEIGHT, USB_C_Z_OFFSET);

      // microSD slot
      if (USE_SD_SLOT) sd_slot_cutout();

      // BOOT/RESET pinholes
      if (USE_BUTTON_HOLES) {
        button_pinhole(BOOT_BTN_Y_FROM_EDGE, "boot");
        button_pinhole(RESET_BTN_Y_FROM_EDGE, "reset");
      }

      // IPEX antenna cutout
      if (USE_IPEX_CUTOUT) ipex_cutout();

      // Battery JST cutout
      if (USE_BATTERY_CUTOUT) battery_cutout();

      // Wall-mount keyhole on the back face
      if (USE_KEYHOLE) back_keyhole();

      // Vent slot grid on the back face
      if (USE_VENTS) back_vents();
    }

    // PCB-retention posts — rise from the cavity floor at z=WALL
    // through the PCB and protrude into the lid's mating tubes.
    for (p = mount_hole_xy)
      translate([p[0], p[1], WALL])
        pcb_post(BACK_DEPTH - 0.5, PCB_POST_PROTRUDE);
  }
}

module back_keyhole() {
  cx = PCB_LEN / 2;
  cy = PCB_WID - KEYHOLE_OFFSET_FROM_TOP;
  translate([cx, cy, -0.01])
    keyhole_negative();
}

module back_vents() {
  start_x = PCB_LEN / 2 - (VENT_COLS - 1) * VENT_PITCH_X / 2 - VENT_SLOT_LEN/2;
  start_y = PCB_WID / 2 + 5;
  translate([start_x, start_y, -0.01])
    vent_grid_negative();
}

// =============================================================
// LAYOUT MODES
// =============================================================
module assembly_view() {
  pcb_dummy();
  back_shell();
  lid();
}

module exploded_view() {
  pcb_dummy();
  back_shell();
  // Lid lifted up
  translate([0, 0, 25]) lid();
}

// Top-level dispatch
if      (mode == "lid")       lid();
else if (mode == "back")      back_shell();
else if (mode == "assembly")  assembly_view();
else if (mode == "exploded")  exploded_view();
else                          assembly_view();
