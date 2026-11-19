#include "changed_frame.h"
#include "display.h"
#include <string.h>

// Group adjacent changed columns so an isolated moving feature costs one rectangle.
bool display_changes(uint32_t *previous, const uint32_t *frame)
{
    uint8_t top[RENDER_SIZE], bottom[RENDER_SIZE] = {0};
    _Static_assert(RENDER_SIZE <= UINT8_MAX, "column extents must fit in one byte");
    memset(top, RENDER_SIZE, sizeof(top));
    for (int y = 0; y < RENDER_SIZE; ++y) {
        for (int x = 0; x < RENDER_SIZE; ++x) {
            int i = y*RENDER_SIZE+x;
            if (previous[i] == frame[i]) continue;
            if (top[x] > y) top[x] = y;
            bottom[x] = y+1;
            previous[i] = frame[i];
        }
    }
    bool transferred = false;
    for (int x = 0; x < RENDER_SIZE;) {
        if (!bottom[x]) { ++x; continue; }
        int start = x, y0 = top[x], y1 = bottom[x];
        do {
            if (top[x] < y0) y0 = top[x];
            if (bottom[x] > y1) y1 = bottom[x];
            ++x;
        } while (x < RENDER_SIZE && bottom[x]);
        transferred = true;
        display_frame_region(frame, start, y0, x-start, y1-y0);
    }
    return transferred;
}
