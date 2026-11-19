#include "water.h"
#include "pixel.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { SURFACE, SUBMERGED, NORMAL_SCALE = 1024 };

static float length3(const float v[3])
{
    return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
}

static void direction(const float v[3], const float fallback[3], float out[3])
{
    float length = length3(v);
    for (int i = 0; i < 3; ++i) out[i] = length > .06f ? v[i]/length : fallback[i];
}

bool water_init(water_t *w, const uint8_t *glass, size_t bytes)
{
    if (!glass || bytes != WATER_GLASS_BYTES) return false;
    w->glass = glass;
    memcpy(w->caustics, glass+WATER_FIELD_BYTES, sizeof w->caustics);
    // Both rows in a pair fit inside the wider of their two circular spans.
    for (int row = 0; row < DISPLAY_SIZE; row += 2) {
        float dy = fabsf(row+1.f-RENDER_SIZE)-.5f;
        w->inset[row/2] = (int)(RENDER_SIZE-sqrtf(RENDER_SIZE*RENDER_SIZE-dy*dy)) & ~1;
    }
    for (int i = 0; i < 256; ++i) {
        w->sine[i] = lroundf(127*sinf(i*.024543693f));
        w->color[SURFACE][i] = pixel_mix(0xff042e51, 0xff82e6dc, i);
        w->color[SUBMERGED][i] = pixel_mix(0xff031b36, 0xff22b5bb, i);
    }
    return true;
}

void water_enter(water_t *w, const float force[3])
{
    const float face_up[3] = {0, 0, -1};
    direction(force, face_up, w->down);
    memcpy(w->previous_force, force, sizeof w->previous_force);
    memset(w->velocity, 0, sizeof w->velocity);
    w->phase = w->ripple = 0;
}

void water_advance(water_t *w, const float force[3], uint32_t elapsed_ms)
{
    if (!elapsed_ms) return;
    float target[3], change[3];
    direction(force, w->down, target);
    for (int i = 0; i < 3; ++i) change[i] = force[i]-w->previous_force[i];
    memcpy(w->previous_force, force, sizeof w->previous_force);
    // Small changes in accelerometer readings at rest still contribute to
    // orientation, but only a perceptible acceleration excites fresh ripples.
    w->ripple = fminf(1, w->ripple+.65f*fmaxf(0, length3(change)-.009f));

    // Inertia lets the surface overshoot apparent gravity, then settle level.
    if (elapsed_ms > 250) elapsed_ms = 250;
    while (elapsed_ms) {
        unsigned ms = elapsed_ms < 10 ? elapsed_ms : 10;
        elapsed_ms -= ms;
        float dt = ms*.001f;
        for (int i = 0; i < 3; ++i) {
            w->velocity[i] += (61*(target[i]-w->down[i])-4.8f*w->velocity[i])*dt;
            w->down[i] += w->velocity[i]*dt;
        }
        w->ripple *= 1-2.1f*dt;
        w->phase += dt*(130 + 40*w->ripple);
        if (w->phase >= 256) w->phase -= 256;
    }
}

typedef struct {
    const water_t *water;
    int nx, ny, nz, phase, amplitude;
    int16_t wave[DISPLAY_SIZE];
    int8_t refract[DISPLAY_SIZE];
} water_frame_t;

// Round small offsets about zero so noise cannot flip a texture sample by a
// whole texel when a slowly moving wave passes through its neutral position.
static int rounded_offset(int value, unsigned shift)
{
    return (value+(1 << (shift-1))) >> shift;
}

static void prepare(const water_t *w, water_frame_t *f)
{
    float down[3];
    const float face_up[3] = {0, 0, -1};
    direction(w->down, face_up, down);
    f->water = w;
    f->nx = lroundf(NORMAL_SCALE*down[0]);
    f->ny = lroundf(NORMAL_SCALE*down[1]);
    f->nz = lroundf(NORMAL_SCALE*down[2]);
    f->phase = (int)w->phase;
    f->amplitude = lroundf(128*fminf(1, w->ripple+.09f*length3(w->velocity)));
    for (int col = 0; col < DISPLAY_SIZE; ++col) {
        f->wave[col] = (w->sine[(col+f->phase)&255]*f->amplitude)>>3;
        f->refract[col] = rounded_offset(f->wave[col], 8);
    }
}

static int clip8(int value)
{
    return value < 0 ? 0 : value > 255 ? 255 : value;
}

typedef struct {
    const uint8_t *art;
    int plane, reflection, tex_u, tex_v;
} water_row_t;

static water_row_t prepare_row(const water_frame_t *f, int row)
{
    int wave = (f->water->sine[(row-f->phase)&255]*f->amplitude)>>3;
    return (water_row_t){
        .art = f->water->glass + WATER_SAMPLE_BYTES*row*DISPLAY_SIZE,
        .plane = (row-RENDER_SIZE)*f->nx + (RENDER_SIZE-1)*f->ny + 4*wave,
        .reflection = 2*row + DISPLAY_SIZE-1 - 344 + rounded_offset(wave,5),
        .tex_u = row + rounded_offset(f->nx,4),
        .tex_v = rounded_offset(wave,7) + rounded_offset(f->ny,4),
    };
}

// Intersect the viewing ray with the vessel. The two signed distances also
// identify blocks that need individual samples to keep the meniscus sharp.
static inline uint32_t shade(const water_frame_t *f, const water_row_t *r, int col,
                             const uint8_t *art, bool *detail)
{
    int z = art[0];
    if (!z) {
        if (detail) *detail = true;
        return 0;
    }
    int mid = r->plane-col*f->ny+4*f->wave[col];
    int projected = f->nz*z;
    int front = mid+projected, back = mid-projected;
    int near_distance = abs(front), far_distance = abs(back);
    if (detail)
        *detail = z < 48 || near_distance < 12*NORMAL_SCALE || far_distance < 12*NORMAL_SCALE;

    uint32_t color = 0xff101c28;
    if (front >= 0 || back >= 0) {
        int layer = front >= 0 ? SUBMERGED : SURFACE;
        int u = ((r->tex_u+(z>>1))>>1) + f->refract[col];
        int v = (col+r->tex_v-(z>>1))>>1;
        int focus = f->water->caustics[(v & (WATER_TILE-1))*WATER_TILE+(u & (WATER_TILE-1))];
        int light = art[1] + ((focus*(290-z)) >> (layer == SURFACE ? 10 : 11));
        if (layer == SURFACE) {
            int band = r->reflection-col+((z-160)>>1)+5*f->refract[col];
            int sheen = 53-(abs(band)>>1);
            light += (110-z)>>2;
            if (sheen > 0) light += (sheen*sheen)>>6;
        } else {
            light -= front>>12;
            if (back < 0) light += 12;
        }
        color = f->water->color[layer][clip8(light)];
    }
    int lip = 174-(near_distance>>4);
    int far_lip = 69-(far_distance>>4);
    if (far_lip > lip) lip = far_lip;
    return lip > 0 ? pixel_mix(color, 0xffd9f8f6, lip) : color;
}

static inline uint16_t panel_color(uint32_t color)
{
    return __builtin_bswap16(pixel_rgb565(color));
}

static void paint_pair(const water_frame_t *f, int row, uint16_t *out, uint16_t *next)
{
    // Share one lighting sample over a 2x2 patch away from the panel edge and
    // meniscus. Both cores write directly to distinct rows of a DMA tile.
    water_row_t a = prepare_row(f, row), b = prepare_row(f, row+1);
    int inset = f->water->inset[row/2];
    memset(out, 0, inset*sizeof(*out));
    memset(next, 0, inset*sizeof(*next));
    int end = DISPLAY_SIZE-inset;
    for (int i = inset; i < end; i += 2) {
        bool detail;
        const uint8_t *sample = a.art + WATER_SAMPLE_BYTES*(i/2);
        uint32_t color = shade(f, &a, i, sample, &detail);
        out[i] = panel_color(color);
        if (detail) {
            out[i+1] = panel_color(shade(f, &a, i+1, sample+WATER_SAMPLE_BYTES*RENDER_SIZE, NULL));
            const uint8_t *below = b.art + WATER_SAMPLE_BYTES*(i/2);
            next[i] = panel_color(shade(f, &b, i, below, NULL));
            next[i+1] = panel_color(shade(f, &b, i+1, below+WATER_SAMPLE_BYTES*RENDER_SIZE, NULL));
        } else {
            // Retain ordered dithering through RGB565 conversion in smooth gradients.
            next[i+1] = out[i];
            out[i+1] = next[i] = panel_color(color+0x020102);
        }
    }
    memset(out+end, 0, inset*sizeof(*out));
    memset(next+end, 0, inset*sizeof(*next));
}

static void paint_rows(const void *context, int row, int count, uint16_t *out)
{
    for (int i = 0; i < count; i += 2, out += 2*DISPLAY_SIZE)
        paint_pair(context, row+i, out, out+DISPLAY_SIZE);
}

void water_present(const water_t *w)
{
    water_frame_t state;
    prepare(w, &state);
    display_parallel_rows(paint_rows, &state);
}

void water_snapshot(const water_t *w, uint32_t *frame)
{
    water_frame_t state;
    prepare(w, &state);
    uint16_t row[DISPLAY_SIZE], second[DISPLAY_SIZE];
    for (int y = 0; y < DISPLAY_SIZE; y += 2) {
        paint_pair(&state, y, row, second);
        for (int x = 0; x < RENDER_SIZE; ++x)
            frame[x*RENDER_SIZE+y/2] = pixel_from_rgb565(__builtin_bswap16(row[DISPLAY_SIZE-1-2*x]));
    }
}
