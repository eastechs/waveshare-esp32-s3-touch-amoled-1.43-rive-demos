#include "test_panel.h"
#include "vu_meter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t band[VU_METER_BAND_BYTES];
static uint16_t rest[DISPLAY_SIZE][DISPLAY_SIZE];
static uint16_t active[DISPLAY_SIZE][DISPLAY_SIZE];

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size > 0);
    rewind(file);
    uint8_t *art = malloc((size_t)size);
    assert(art && fread(art, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    assert(vu_meter_valid(art, (size_t)size));
    assert(!vu_meter_valid(art, (size_t)size-1));

    vu_meter_t meter;
    vu_meter_init(&meter, art, band);
    vu_meter_enter(&meter, swipe, false);
    assert(!transfers);
    vu_meter_enter(&meter, swipe, true);
    assert(transfers == 3);
    assert_swipe_matches_panel();
    for (int y = 0; y < DISPLAY_SIZE; ++y) {
        for (int x = 0; x < DISPLAY_SIZE; ++x) {
            int i = 2*(y*DISPLAY_SIZE+x);
            assert(panel[y][x] == (art[i] | (art[i+1] << 8)));
        }
    }
    // Sample the brass chamfer where the round display meets each panel edge.
    const int center = DISPLAY_SIZE/2, last = DISPLAY_SIZE-1;
    const uint16_t rim[] = {panel[center][0], panel[center][last], panel[0][center], panel[last][center]};
    for (unsigned i = 0; i < sizeof(rim)/sizeof(*rim); ++i)
        assert((rim[i] >> 11) >= 5 && (rim[i] >> 11) > (rim[i] & 31) + 2);
    memcpy(rest, panel, sizeof(rest));

    vu_meter_advance(&meter, 200, swipe, true);
    assert(transfers == 3);
    vu_meter_advance(&meter, 5200, swipe, true);
    assert(transfers == 4);
    assert(memcmp(rest, panel, sizeof(rest)) != 0);
    for (int y = 0; y < DISPLAY_SIZE; ++y) {
        if (y < VU_METER_PATCH_Y || y >= VU_METER_PATCH_Y+VU_METER_PATCH_HEIGHT)
            assert(memcmp(rest[y], panel[y], sizeof(rest[y])) == 0);
    }
    assert_swipe_matches_panel();
    memcpy(active, panel, sizeof(active));

    // Reentering after another scene used the buffer reconstructs the held pose.
    memset(swipe, 0, sizeof(swipe));
    memset(panel, 0, sizeof(panel));
    vu_meter_enter(&meter, swipe, true);
    assert(memcmp(active, panel, sizeof(active)) == 0);
    assert_swipe_matches_panel();
    vu_meter_advance(&meter, 2600, swipe, true);
    assert(memcmp(rest, panel, sizeof(rest)) == 0);
    assert_swipe_matches_panel();
    unsigned settled = transfers;
    vu_meter_advance(&meter, 0, swipe, true);
    assert(transfers == settled);

    free(art);
    puts("Native VU panel, swipe mirror, skipped frames, resume, and loop seam passed.");
}
