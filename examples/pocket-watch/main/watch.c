#include "watch.h"
#include "pixel.h"
#include "display.h"
#include <math.h>
#include <string.h>

#define TAU 6.28318530718f
enum { HINGE_MS = 620 };
typedef struct { int x, y, teeth, driver, tx, ty; } wheel_t;
typedef struct { int tx, ty, half_width, length, tail; } hand_t;

#define WHEEL(name, x, y, teeth, driver, tx, ty) {x, y, teeth, driver, tx, ty},
#define HAND(...)
static const wheel_t wheels[] = {
#include "parts.def"
};
#undef WHEEL
#undef HAND
#define WHEEL(...)
#define HAND(name, tx, ty, half_width, length, tail) {tx, ty, half_width, length, tail},
static const hand_t hands[] = {
#include "parts.def"
};
#undef WHEEL
#undef HAND

static float lid_position(const watch_t *watch, uint64_t now)
{
    float travel = (float)(now - watch->changed_ms) / HINGE_MS;
    return watch->opening ? fminf(1, watch->start_position + travel)
                          : fmaxf(0, watch->start_position - travel);
}

// The dome curves gently through the fluting, then rolls over at the outer lip.
// Tabulate its surface normals by squared radius (in half-resolution pixels).
enum { CASE_RADIUS = 115 };
typedef struct { float radial, z; } case_normal_t;
static case_normal_t normals[CASE_RADIUS*CASE_RADIUS/32+1];

void watch_init(watch_t *watch, const uint32_t *dial, const uint32_t *lid, const uint32_t *tray)
{
    *watch = (watch_t){.dial = dial, .lid = lid, .tray = tray};
    // Radii of the raised beading drawn around the medallion and on the rim.
    const float bead_radii[] = {71, 100, 108};
    for (unsigned i = 0; i < sizeof(normals)/sizeof(*normals); ++i) {
        float r = sqrtf(i*32+16);
        float lip = fmaxf(0, (r-99)/16);
        float slope = r/204 + .85f*lip*lip;
        for (unsigned j = 0; j < sizeof(bead_radii)/sizeof(*bead_radii); ++j) {
            float t = (r-bead_radii[j])/1.75f;
            if (fabsf(t) < 1) slope += .82f*t*(1-t*t)*(1-t*t);
        }
        float nz = 1/sqrtf(1+slope*slope);
        normals[i] = (case_normal_t){slope*nz/r, nz};
    }
}

bool watch_set_tilt(watch_t *watch, float x, float y)
{
    int lx = lroundf(192*x), ly = lroundf(192*y);
    if (lx == watch->light_x && ly == watch->light_y) return false;
    watch->light_x = lx;
    watch->light_y = ly;
    return true;
}

void watch_toggle(watch_t *watch, uint64_t now)
{
    watch->start_position = lid_position(watch, now);
    watch->changed_ms = now;
    watch->opening = !watch->opening;
}

bool watch_is_open(const watch_t *watch, uint64_t now)
{
    return watch->opening && lid_position(watch, now) == 1;
}

bool watch_is_closed(const watch_t *watch, uint64_t now)
{
    return !watch->opening && lid_position(watch, now) == 0;
}

// The parts tray is twice the framebuffer resolution. Walk it in 16.16 fixed
// point so each tooth and the thin sweep hand can rotate without a vector redraw.
static void rotate_part(uint32_t *frame, const uint32_t *tray, int cx, int cy, int tx, int ty,
                        int half_width, int length, int tail, float angle)
{
    float cs = cosf(angle), sn = sinf(angle);
    float mid_y = (tail-length)*.5f, half_height = (tail+length)*.5f;
    float mx = cx-sn*mid_y, my = cy+cs*mid_y;
    float rx = fabsf(cs)*half_width + fabsf(sn)*half_height;
    float ry = fabsf(sn)*half_width + fabsf(cs)*half_height;
    int left = fmaxf(0, floorf((mx-rx)*.5f)), top = fmaxf(0, floorf((my-ry)*.5f));
    int right = fminf(RENDER_SIZE, ceilf((mx+rx)*.5f)), end = fminf(RENDER_SIZE, ceilf((my+ry)*.5f));
    int dx = 2*left+1-cx, dy = 2*top+1-cy;
    int u0 = lroundf((tx+cs*dx+sn*dy)*65536), v0 = lroundf((ty-sn*dx+cs*dy)*65536);
    int dc = lroundf(cs*131072), ds = lroundf(sn*131072);
    for (int y = top; y < end; y++, u0 += ds, v0 += dc) {
        int u = u0, v = v0;
        for (int x = left; x < right; x++, u += dc, v -= ds) {
            int sx = u >> 16, sy = v >> 16;
            if (sx < tx-half_width || sx >= tx+half_width || sy < ty-length || sy >= ty+tail) continue;
            uint32_t *dst = frame + y*RENDER_SIZE+x;
            *dst = over(tray[sy*DISPLAY_SIZE+sx], *dst);
        }
    }
}

typedef struct { float x, y, z; } direction_t;

// A broad light above the viewer's left shoulder. Express the half-vector
// between lamp and viewer in lid coordinates, including rotation at the hinge.
static direction_t cover_light(const watch_t *watch, float hinge)
{
    float x = -.34f - watch->light_x/192.f, y = -.31f - watch->light_y/192.f;
    float inv = 1/sqrtf(1 + x*x + y*y);
    float c = cosf(hinge), s = sinf(hinge);
    return (direction_t){(c*x+s)*inv, y*inv, (c-s*x)*inv};
}

static uint32_t reflect_brass(uint32_t metal, int dx, int dy, direction_t h)
{
    int r2 = dx*dx + dy*dy;
    if (r2 <= 67*67 || r2 >= CASE_RADIUS*CASE_RADIUS) return metal;
    case_normal_t n = normals[r2/32];
    float aligned = (dx*h.x + dy*h.y)*n.radial + n.z*h.z;
    if (aligned < .80f) return metal;
    float lobe = aligned*aligned;
    lobe *= lobe;
    lobe *= lobe;
    lobe *= lobe; // Broad satin reflection and a smaller polished core.
    float core = lobe*lobe;
    unsigned strength = (unsigned)(82*lobe*lobe + 64*core*core*core*core);
    // Recessed engraving catches less of the reflected light than the raised brass.
    strength = strength*(68 + ((metal >> 8) & 255))/256;
    return pixel_mix(metal, 0xffffedc9, strength);
}

void watch_paint(const watch_t *watch, uint32_t *frame, uint64_t uptime_ms, uint32_t local_ms)
{
    float position = lid_position(watch, uptime_ms);
    memcpy(frame, watch->dial, RENDER_SIZE*RENDER_SIZE*sizeof(*frame));
    if (position > 0) {
        float rest[sizeof(wheels)/sizeof(*wheels)];
        int direction[sizeof(wheels)/sizeof(*wheels)];
        float crank_angle = 0;
        for (unsigned i = 0; i < sizeof(wheels)/sizeof(*wheels); ++i) {
            const wheel_t w = wheels[i];
            if (w.driver < 0) {
                rest[i] = 0;
                direction[i] = 1;
            } else {
                const wheel_t driver = wheels[w.driver];
                float contact = atan2f(w.y-driver.y, w.x-driver.x);
                rest[i] = fmodf(contact+TAU*.5f+(driver.teeth*(contact-rest[w.driver])-TAU*.5f)/w.teeth, TAU/w.teeth);
                if (rest[i] < 0) rest[i] += TAU/w.teeth;
                direction[i] = -direction[w.driver];
            }
            float angle = rest[i] + direction[i]*TAU*(uptime_ms % (200*w.teeth))/(200*w.teeth);
            if (i == 0) crank_angle = angle;
            int r = (5*w.teeth+10)/2;
            rotate_part(frame, watch->tray, w.x, w.y, w.tx, w.ty, r, r, r, angle);
        }
        // One end follows a spoke pin. The other slides vertically at x=144.
        float pin_x = wheels[0].x+19*cosf(crank_angle), pin_y = wheels[0].y+19*sinf(crank_angle);
        rotate_part(frame, watch->tray, lroundf(pin_x), lroundf(pin_y), 30, 402, 8, 112, 8,
                    asinf((144-pin_x)/104));
        const int periods[] = {43200000, 3600000, 60000};
        for (unsigned i = 0; i < sizeof(hands)/sizeof(*hands); ++i) {
            const hand_t h = hands[i];
            float angle = TAU * (local_ms % periods[i]) / periods[i];
            rotate_part(frame, watch->tray, DISPLAY_SIZE/2, DISPLAY_SIZE/2, h.tx, h.ty,
                        h.half_width, h.length, h.tail, angle);
        }
    }
    if (position >= 1) return;
    float eased = position*position*(3-2*position);
    float width = cosf(eased*TAU*.25f);
    int columns = lroundf(RENDER_SIZE*width);
    if (columns <= 0) return;
    direction_t light = cover_light(watch, eased*TAU*.25f);
    int step = (RENDER_SIZE << 16)/columns;
    for (int y = 0; y < RENDER_SIZE; ++y) {
        int row = y*RENDER_SIZE;
        int dy = y-RENDER_SIZE/2;
        for (int x = 0, u = step/2; x < columns; ++x, u += step) {
            int sx = u>>16, dx = sx-RENDER_SIZE/2;
            uint32_t color = watch->lid[row+sx];
            if ((color>>24) == 255) color = reflect_brass(color, dx, dy, light);
            frame[row+x] = over(color, frame[row+x]);
        }
    }
}
