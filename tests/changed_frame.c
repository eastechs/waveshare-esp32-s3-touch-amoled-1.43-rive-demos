#include "display.h"
#include "changed_frame.h"
#include <assert.h>
#include <string.h>

static uint32_t source[RENDER_SIZE*RENDER_SIZE], previous[RENDER_SIZE*RENDER_SIZE], panel[RENDER_SIZE*RENDER_SIZE];
static unsigned calls;

void display_frame_region(const uint32_t *pixels, int x, int y, int width, int height)
{
    assert(x >= 0 && width > 0 && x+width <= RENDER_SIZE);
    assert(y >= 0 && height > 0 && y+height <= RENDER_SIZE);
    ++calls;
    for (int row = y; row < y+height; ++row)
        memcpy(panel+row*RENDER_SIZE+x, pixels+row*RENDER_SIZE+x, width*sizeof(*pixels));
}

int main(void)
{
    int last = RENDER_SIZE*RENDER_SIZE-1;
    source[0] = source[last] = 0xff224466;
    assert(display_changes(previous, source));
    assert(calls == 2 && memcmp(panel, source, sizeof(panel)) == 0);
    assert(!display_changes(previous, source));
    assert(calls == 2);
    source[last] = 0;
    source[last-1] = 0xff224466;
    assert(display_changes(previous, source));
    assert(calls == 3 && memcmp(panel, source, sizeof(panel)) == 0);
}
