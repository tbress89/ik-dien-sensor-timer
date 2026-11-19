// Football Shot Timer — 3D-printable case
// ------------------------------------------------------------------
// Three parts:
//   box   the bottom shell: sensor hole, clip pockets, tripod nut pocket
//   lid   display window, knob hole, buzzer holes, snap clips (no screws)
//   hood  sun shield that slides over the sensor (IR sensors hate sunlight)
//
// Export one part at a time (or use the Makefile-style commands in README):
//   openscad -D 'part="box"'  -o stl/box.stl  case.scad
//   openscad -D 'part="lid"'  -o stl/lid.stl  case.scad
//   openscad -D 'part="hood"' -o stl/hood.stl case.scad
// part = "assembly" shows everything in place with grey dummy parts.
//
// Change the numbers in the first block to match YOUR parts, then re-export.
// All sizes in millimetres.

part = "assembly";

/* ================= what goes inside — MEASURE YOURS ================= */
powerbank   = [100, 65, 23];   // length, width, height
sensor_zone = 40;              // free space behind the front wall (sensor back end + cable)
inner_h     = 68;              // inside height of the box

/* ================= sensor (E18-D80NK, M18 thread) =================== */
sensor_hole = 18.6;
sensor_z    = 45;              // sensor centre above the ground

/* ================= lid layout ======================================= */
display_body   = [12.7, 19.1]; // 0.56" single digit, outside size
display_window = [10.6, 17.0]; // visible opening in the lid
display_x      = 85;           // centre, measured from the front (sensor) face
encoder_hole   = 7.5;          // knob shaft / bushing
encoder_x      = 122;
buzzer_x       = 32;

/* ================= shell ============================================ */
wall     = 3;
floor_t  = 3;
lid_t    = 3;
lip_h    = 4;                  // lip on the lid that drops into the box
clear    = 0.4;                // gap between lip and walls (printer tolerance)
corner_r = 6;

/* ================= snap clips (lid ↔ box, no tools) ================= */
// One clip on each long side. To open: pull both finger tabs outward and lift.
clip_w     = 22;               // width of a clip arm
clip_t     = 1.8;              // arm thickness (thinner = easier to open)
clip_len   = 20;               // arm length below the lid
clip_hook  = 1.2;              // how far the hook reaches into the box wall
clip_catch = 13;               // hook's catch face, below the lid underside
clip_gap   = 0.3;              // gap between arm and box wall

/* ================= tripod mount (standard 1/4"-20 thread) =========== */
// A regular 1/4"-20 hex nut drops into a pocket in the floor, from the inside.
// The tripod screw comes up through the floor and pulls the nut down onto it.
// The power bank rests on the nut block and two rails, so it holds the nut in place.
tripod_mount = true;
tripod_nut_af = 11.5;          // across flats (nut is 11.1 = 7/16")
tripod_nut_h  = 5.8;           // pocket depth (nut is 5.6 = 7/32")
tripod_hole   = 6.9;           // screw passes freely (1/4" = 6.35)
tripod_block  = 5;             // block height above the floor = how far the power bank is raised

with_switch = false;           // cut-out for a KCD11 mini rocker switch in the back wall
switch_cut  = [19.2, 13.2];

/* ================= sun hood ========================================= */
hood_id  = 33;                 // clears the outer M18 nut
hood_len = 32;

/* ==================================================================== */

$fn = 64;

rib   = 2;                                         // stop that keeps the power bank out of the sensor zone
inner = [sensor_zone + rib + powerbank.x + 2, powerbank.y + 7];  // +2: 1 mm gap at each end of the power bank
outer = [inner.x + 2 * wall, inner.y + 2 * wall];
box_h = floor_t + inner_h;
pb_lift = tripod_mount ? tripod_block : 0;          // power bank sits this much above the floor
pb_x0   = wall + sensor_zone + rib + 1;             // power bank's front end
pb_y0   = wall + (inner.y - powerbank.y) / 2;
mount   = [outer.x / 2, outer.y / 2];               // tripod screw: middle of the box

module rrect(size, r, h) {
  linear_extrude(h) offset(r) offset(-r) square(size);
}

/* ------------------------------ box ------------------------------- */

module box() {
  difference() {
    box_body();
    if (tripod_mount) translate([mount.x, mount.y, 0]) {
      translate([0, 0, floor_t + tripod_block - tripod_nut_h]) cylinder(d = tripod_nut_af / cos(30), h = tripod_nut_h + 1, $fn = 6);
      translate([0, 0, -1]) cylinder(d = tripod_hole, h = floor_t + tripod_block + 2);
    }
  }
}

module box_body() {
  difference() {
    rrect(outer, corner_r, box_h);
    // cavity
    translate([wall, wall, floor_t]) rrect(inner, corner_r - wall, box_h);
    // sensor hole in the front wall
    translate([-1, outer.y / 2, sensor_z]) rotate([0, 90, 0]) cylinder(d = sensor_hole, h = wall + 2);
    // drain holes in the sensor zone
    for (y = [wall + 14, outer.y - wall - 14])
      translate([wall + 12, y, -1]) cylinder(d = 3, h = floor_t + 2);
    // optional switch
    if (with_switch)
      translate([outer.x - wall - 1, outer.y / 2 - switch_cut.x / 2, box_h - 22 - switch_cut.y / 2])
        cube([wall + 2, switch_cut.x, switch_cut.y]);
    // pockets the clip hooks snap into (one in each long wall)
    for (side = [0, 1]) clip_side(side)
      translate([-(clip_w + 1) / 2, -1, box_h - clip_catch - 3.5]) cube([clip_w + 1, 1 + clip_hook + 0.4, 3.7]);
  }
  // power bank stop
  translate([wall + sensor_zone, wall + 8, floor_t]) cube([rib, inner.y - 16, 6 + pb_lift]);
  if (tripod_mount) tripod_block();
}

module tripod_block() {   // nut block + two rails the power bank rests on (holes are cut in box())
  top = floor_t + tripod_block;
  translate([mount.x, mount.y, 0]) cylinder(d = tripod_nut_af / cos(30) + 6, h = top);
  for (y = [pb_y0 + 10, pb_y0 + powerbank.y - 13])
    translate([pb_x0 - 1, y, 0]) cube([powerbank.x + 1, 3, top]);
}

/* ------------------------------ clips ----------------------------- */
// Local frame of a clip: x along the wall (0 = middle), y = 0 is the box's outer wall
// face, +y points into the box. side 0 = front long wall, side 1 = back long wall.
module clip_side(side) {
  if (side == 0) translate([outer.x / 2, 0, 0]) children();
  else translate([outer.x / 2, outer.y, 0]) mirror([0, 1, 0]) children();
}

// One clip arm, hanging from the lid. z = 0 is the lid underside.
module clip_arm() {
  y0 = -clip_gap - clip_t;          // outside face of the arm
  rotate([90, 0, 90]) translate([0, 0, -clip_w / 2]) linear_extrude(clip_w)
    polygon([                       // (y, z) profile
      [y0, lid_t], [0.01, lid_t], [0.01, 0],                     // bridge to the lid plate
      [-clip_gap, 0],
      [-clip_gap, -clip_catch],                                   // inner face down to the hook
      [clip_hook, -clip_catch],                                   // flat catch face
      [clip_hook, -clip_catch - 0.6],
      [-clip_gap, -clip_catch - 0.6 - (clip_hook + clip_gap) * 1.5], // lead-in slope
      [-clip_gap, -clip_len],
      [y0 - 4, -clip_len],                                        // finger tab (45°, prints without support)
      [y0, -clip_len + 4],
    ]);
}

/* ------------------------------ lid ------------------------------- */
// Modelled in place: underside at z = 0, top at z = lid_t, lip hangs below.

module lid() {
  cy = outer.y / 2;
  difference() {
    union() {
      rrect(outer, corner_r, lid_t);
      for (side = [0, 1]) clip_side(side) clip_arm();
      // lip
      translate([0, 0, -lip_h]) difference() {
        translate([wall + clear, wall + clear]) rrect(inner - [2, 2] * clear, corner_r - wall - clear, lip_h);
        translate([wall + clear + 1.6, wall + clear + 1.6]) rrect(inner - [2, 2] * (clear + 1.6), 1, lip_h + 1);
      }
      // frame that holds the display from below
      translate([display_x, cy, -6]) linear_extrude(6)
        square(display_body + [0.3, 0.3] + [2.4, 2.4], center = true);
    }
    // display: window + pocket
    translate([display_x, cy, -1]) linear_extrude(lid_t + 2) square(display_window, center = true);
    translate([display_x, cy, -7]) linear_extrude(7 + lid_t - 0.8) square(display_body + [0.3, 0.3], center = true);
    // knob
    translate([encoder_x, cy, -1]) cylinder(d = encoder_hole, h = lid_t + 2);
    // buzzer sound holes
    translate([buzzer_x, cy, -1]) {
      cylinder(d = 2.2, h = lid_t + 2);
      for (a = [0 : 60 : 300]) rotate(a) translate([4.5, 0, 0]) cylinder(d = 2.2, h = lid_t + 2);
    }
    // label (engraved)
    translate([0, 0, lid_t - 0.6]) linear_extrude(1)
      translate([encoder_x, cy - 15]) text("-  SEC  +", size = 4, halign = "center", valign = "center",
                                           font = "Liberation Sans:style=Bold");
  }
}

/* ------------------------------ hood ------------------------------ */
// Printed standing up. Goes over the sensor between the wall and the outer nut.

module hood() {
  difference() {
    union() {
      cylinder(d = hood_id + 7, h = 2.5);               // flange against the wall
      cylinder(d = hood_id + 4, h = hood_len);
    }
    translate([0, 0, -1]) cylinder(d = sensor_hole, h = 5);
    translate([0, 0, 2.5]) cylinder(d = hood_id, h = hood_len);
  }
}

/* ------------------------- dummy parts ---------------------------- */

module d_powerbank() {
  translate([pb_x0, pb_y0, floor_t + pb_lift]) cube(powerbank);
}
module d_nut() {   // 1/4"-20 nut + the tripod screw (6 mm long) coming up through the floor
  if (tripod_mount) translate([mount.x, mount.y, 0]) {
    translate([0, 0, floor_t + tripod_block - tripod_nut_h]) cylinder(d = 11.1 / cos(30), h = 5.6, $fn = 6);
    translate([0, 0, -0.01]) cylinder(d = 6.3, h = 6);
  }
}
module d_breadboard() {   // breadboard + ESP32 + wires, on top of the power bank
  translate([wall + sensor_zone + rib + 1 + (powerbank.x - 84) / 2, wall + (inner.y - 55) / 2, floor_t + pb_lift + powerbank.z])
    cube([84, 55, 9 + 13]);
}
module d_sensor() {
  translate([-wall - 2.5 - hood_len + 4, outer.y / 2, sensor_z]) rotate([0, 90, 0]) cylinder(d = 18, h = 66);
  translate([wall + 0.01, outer.y / 2, sensor_z]) rotate([0, 90, 0]) cylinder(d = 31, h = 5, $fn = 6);  // inner nut
}
module d_display() { translate([display_x, outer.y / 2, box_h - 8 + lid_t - 0.8]) linear_extrude(8) square(display_body, center = true); }
module d_encoder() {
  translate([encoder_x - 13, outer.y / 2 - 9.5, box_h - 13]) cube([27, 19, 1.6]);   // PCB
  translate([encoder_x - 6.25, outer.y / 2 - 6.25, box_h - 11.4]) cube([12.5, 12.5, 11.4]);
}
module d_buzzer() { translate([buzzer_x - 11, outer.y / 2 - 6.5, box_h - 12]) cube([22, 13, 12]); }

module dummies() {
  d_powerbank(); d_breadboard(); d_sensor(); d_display(); d_encoder(); d_buzzer(); d_nut();
}
module lid_in_place() { translate([0, 0, box_h]) lid(); }
module hood_in_place() { translate([0, outer.y / 2, sensor_z]) rotate([0, -90, 0]) hood(); }

/* ------------------------------ output ---------------------------- */

if (part == "box") box();
else if (part == "lid") translate([0, outer.y, lid_t]) rotate([180, 0, 0]) lid();   // print top-down
else if (part == "hood") hood();
else if (part == "assembly") {
  color("#2f7de1") box();
  color("#f2f2f2", 0.55) lid_in_place();
  color("#2f7de1") hood_in_place();
  color("#888888") dummies();
}
else if (part == "open") {   // lid lifted off, to see inside
  color("#2f7de1") box();
  color("#f2f2f2") translate([0, 0, 45]) lid_in_place();
  color("#2f7de1") hood_in_place();
  color("#888888") dummies();
}
// collision checks: each of these must render EMPTY
// (parts that rest on each other are moved 0.01 mm apart, so touching faces don't count)
else if (part == "check_box")    intersection() { box(); union() {
                                   translate([0, 0, 0.01]) dummies();
                                   translate([0, 0, 0.01]) lid_in_place();
                                   translate([-0.01, 0, 0]) hood_in_place(); } }
else if (part == "check_lid")    intersection() { lid_in_place(); union() { d_powerbank(); d_breadboard(); d_sensor(); } }
else if (part == "check_inside") intersection() { union() { d_display(); d_encoder(); d_buzzer(); } union() { d_powerbank(); d_breadboard(); d_sensor(); } }
