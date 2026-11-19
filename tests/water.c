#include "water.h"
#include "test_panel.h"
#include <math.h>
#include <stdio.h>

void display_parallel_rows(display_rows_fn paint, const void *context)
{
    uint16_t tile[16*DISPLAY_SIZE];
    for (int y = 0; y < DISPLAY_SIZE; y += 16) {
        int rows = DISPLAY_SIZE-y < 16 ? DISPLAY_SIZE-y : 16;
        int split = (rows/2) & ~1;
        // The second core may finish first. Neither half can depend on the other.
        paint(context, y+split, rows-split, tile+split*DISPLAY_SIZE);
        if (split) paint(context, y, split, tile);
        for (int i = 0; i < rows; ++i)
            for (int x = 0; x < DISPLAY_SIZE; ++x)
                panel[DISPLAY_SIZE-1-x][y+i] = __builtin_bswap16(tile[i*DISPLAY_SIZE+x]);
    }
    last_scan_width = DISPLAY_SIZE;
    ++transfers;
}

static water_t water;
static uint8_t glass[WATER_GLASS_BYTES];
static const float upright[3] = {0, 1, 0}, face_up[3] = {0, 0, -1};

static unsigned green(int x, int y)
{
    return (panel[y+RENDER_SIZE][x+RENDER_SIZE]>>5) & 63;
}

static void orient(const float force[3])
{
    water_enter(&water, force);
    water_present(&water);
    water_snapshot(&water, swipe);
    assert_swipe_matches_panel();
}

static void advance(const float force[3], int frames)
{
    for (int i = 0; i < frames; ++i) water_advance(&water, force, 20);
}

int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
    FILE *file = fopen(argv[1], "rb");
    assert(file && fread(glass, 1, sizeof glass, file) == sizeof glass && fgetc(file) == EOF);
    fclose(file);
    assert(!water_init(&water, glass, sizeof glass-1));
    assert(water_init(&water, glass, sizeof glass));
    orient(upright);
    assert(last_scan_width == DISPLAY_SIZE && green(0,-80)+6 < green(0,80));
    assert(green(0,0) > green(0,15)); // The meniscus crosses the center.
    unsigned dry = green(0,-80), submerged = green(0,80);
    // The empty half is uniform right up to the edge of the round panel.
    uint16_t background = panel[RENDER_SIZE-80][RENDER_SIZE];
    for (int y = 0; y < RENDER_SIZE-4; ++y) {
        for (int x = 0; x < DISPLAY_SIZE; ++x) {
            int dx = 2*x+1-DISPLAY_SIZE, dy = 2*y+1-DISPLAY_SIZE;
            if (dx*dx+dy*dy < DISPLAY_SIZE*DISPLAY_SIZE) assert(panel[y][x] == background);
        }
    }
    orient((float[3]){1,0,0});
    assert(green(-90,0)+7 < green(90,0));
    orient((float[3]){0,.866f,-.5f});
    assert(green(0,-160) < green(0,-60) && green(0,160) < green(0,60));
    orient(face_up);
    assert(green(0,-80) > dry+15 && green(0,80) > submerged+4);
    // Even the two-pixel lighting blocks must respect the circular panel edge.
    for (int y = 0; y < DISPLAY_SIZE; ++y) {
        for (int x = 0; x < DISPLAY_SIZE; ++x) {
            int dx = 2*x+1-DISPLAY_SIZE, dy = 2*y+1-DISPLAY_SIZE;
            assert((panel[y][x] != 0) == (dx*dx+dy*dy < DISPLAY_SIZE*DISPLAY_SIZE));
        }
    }
    orient((float[3]){0,0,1});
    assert(green(0,80) < submerged+8);
    orient((float[3]){0,0,0});
    assert(water.down[2] == -1);

    // At rest, alternating sub-pixel IMU noise must not continuously launch waves.
    orient(upright);
    for (int i = 0; i < 150; ++i) {
        float noise = i%2 ? .002f : -.002f;
        water_advance(&water, (float[3]){noise,1,-noise}, 20);
    }
    assert(water.ripple == 0 && fabsf(water.down[0]) < .001f);
    // Slow tilt still moves the surface even if each input step is small.
    for (int i = 1; i <= 100; ++i) {
        float angle = .002f*i;
        water_advance(&water, (float[3]){sinf(angle),cosf(angle),0}, 20);
    }
    assert(fabsf(water.down[0]-sinf(.2f)) < .02f);

    // A lateral impulse tips the surface. It crosses level on the rebound and settles.
    orient(upright);
    advance((float[3]){1,1,0}, 6);
    assert(water.down[0] > .1f && water.ripple > .4f);
    bool crossed = false;
    for (int i = 0; i < 200; ++i) {
        advance(upright, 1);
        if (water.down[0] < -.04f) crossed = true;
    }
    assert(crossed && fabsf(water.down[0]) < .001f && water.ripple < .001f);
    water_present(&water);
    assert(green(0,-80) == dry && green(0,80) == submerged);

    if (argc == 3) {
        FILE *out = fopen(argv[2], "wb");
        assert(out);
        for (int i = 0; i < 128; ++i) {
            float angle = fminf(1,fmaxf(0,(i-55)/48.f))*1.5707963f;
            float force[] = {i >= 14 && i < 19 ? 1.3f : 0, cosf(angle), -sinf(angle)};
            water_advance(&water, force, 40);
            water_present(&water);
            assert(fwrite(panel, sizeof panel, 1, out) == 1);
        }
        fclose(out);
    }
    puts("Water holds level in three axes, renders from above and below, sloshes and settles, and matches its swipe frame.");
}
