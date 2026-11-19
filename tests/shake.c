#include "shake.h"
#include <assert.h>
#include <math.h>

static unsigned feed(shake_detector_t *detector, uint32_t *ms, unsigned count,
                     float x, float y, float z)
{
    unsigned events = 0;
    const float sample[] = {x, y, z};
    for (unsigned i = 0; i < count; ++i, *ms += 10)
        events += shake_update(detector, *ms, sample);
    return events;
}

int main(void)
{
    shake_detector_t detector = {0};
    uint32_t ms = UINT32_MAX - 400;
    assert(feed(&detector, &ms, 100, 0, 0, 1) == 0);
    // Gradual tilt through 90 degrees.
    for (int i = 1; i <= 100; ++i) {
        float angle = i * 0.015708f;
        assert(feed(&detector, &ms, 1, sinf(angle), 0, cosf(angle)) == 0);
    }
    // A single tap then settling does not count as a shake.
    assert(feed(&detector, &ms, 1, 1, 2.4f, 0) == 0);
    assert(feed(&detector, &ms, 100, 1, 0, 0) == 0);
    unsigned gestures = 0;
    for (unsigned i = 0; i < 12; ++i)
        gestures += feed(&detector, &ms, 8, 1, i % 2 ? -1.6f : 1.6f, 0);
    assert(gestures == 1);
    assert(feed(&detector, &ms, 100, 1, 0, 0) == 0);
    assert(feed(&detector, &ms, 8, 1, 0, 1.8f) == 0);
    assert(feed(&detector, &ms, 8, 1, 0, -1.8f) == 1);
}
