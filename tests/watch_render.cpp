#include "display.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>
extern "C" {
#include "watch.h"
}
#include <cassert>
#include <fstream>

using Frame = std::vector<uint32_t>;

static Frame baked_part(const std::filesystem::path& directory, const char *name, int side)
{
    std::ifstream in(directory / name, std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
    Frame frame(side*side);
    uLongf size = frame.size()*sizeof(uint32_t);
    assert(uncompress(reinterpret_cast<Bytef*>(frame.data()), &size, bytes.data(), bytes.size()) == Z_OK);
    assert(size == frame.size()*sizeof(uint32_t));
    return frame;
}

static int changes(const Frame& a, const Frame& b, int cx, int cy, int r)
{
    int n = 0;
    for (int y = cy-r; y <= cy+r; ++y)
        for (int x = cx-r; x <= cx+r; ++x)
            n += a[y*RENDER_SIZE+x] != b[y*RENDER_SIZE+x];
    return n;
}

// Center of the added green-channel light on opaque brass, independent of its engraving.
static double highlight_x(const Frame& frame, const Frame& lid)
{
    double sum = 0, weighted_x = 0;
    for (int y = 0; y < RENDER_SIZE; ++y) {
        for (int x = 0; x < RENDER_SIZE; ++x) {
            int dx = x-RENDER_SIZE/2, dy = y-RENDER_SIZE/2, r2 = dx*dx+dy*dy;
            if (r2 < 72*72 || r2 > 114*114) continue;
            int i = y*RENDER_SIZE+x;
            int gain = int((frame[i]>>8)&255) - int((lid[i]>>8)&255);
            if (gain > 0) { sum += gain; weighted_x += x*gain; }
        }
    }
    assert(sum > 0);
    return weighted_x/sum;
}

static void preview(const char *prefix, const char *name, const Frame& frame)
{
    std::ofstream out(std::string(prefix)+name+".ppm", std::ios::binary);
    out << "P6\n" << RENDER_SIZE << " " << RENDER_SIZE << "\n255\n";
    for (auto pixel : frame) {
        out.put(pixel >> 16); out.put(pixel >> 8); out.put(pixel);
    }
    assert(out.good());
}

int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
    auto dial = baked_part(argv[1], "watch-dial.argb.z", RENDER_SIZE);
    auto lid = baked_part(argv[1], "watch-lid.argb.z", RENDER_SIZE);
    auto tray = baked_part(argv[1], "watch-tray.argb.z", DISPLAY_SIZE);

    watch_t watch;
    watch_init(&watch, dial.data(), lid.data(), tray.data());
    Frame frame(dial.size());
    const uint32_t ten_past_ten = (10*3600+10*60+37)*1000;
    auto paint = [&](uint64_t now, uint32_t clock) {
        watch_paint(&watch, frame.data(), now, clock);
    };
    paint(0, ten_past_ten);
    Frame closed = frame;
    // The enamel center and the exposed case band retain their authored pixels.
    for (size_t i = 0; i < closed.size(); ++i) {
        if ((lid[i] >> 24) == 0) assert(closed[i] == dial[i]);
        int dx = int(i%RENDER_SIZE)-RENDER_SIZE/2, dy = int(i/RENDER_SIZE)-RENDER_SIZE/2;
        if (dx*dx + dy*dy < 66*66) assert(closed[i] == lid[i]);
    }
    assert(watch_is_closed(&watch, 0));
    // Sweep across level in small steps. The reflection tracks a stationary light.
    double previous = RENDER_SIZE;
    for (int step = -10; step <= 10; ++step) {
        watch_set_tilt(&watch, step*.03f, 0);
        paint(40, ten_past_ten);
        double x = highlight_x(frame, lid);
        assert(x < previous);
        if (step > -10) assert(previous-x < 14);
        previous = x;
    }
    assert(watch_set_tilt(&watch, .8f, -.3f));
    paint(40, ten_past_ten);
    assert(changes(closed, frame, 116, 23, 9) > 80);
    assert(changes(closed, frame, 116,116,40) == 0);
    if (argc == 3) preview(argv[2], "tilted", frame);
    assert(!watch_set_tilt(&watch, .8f, -.3f));
    watch_set_tilt(&watch, -.65f, -.3f);
    paint(60, ten_past_ten);
    if (argc == 3) preview(argv[2], "opposite", frame);
    assert(watch_set_tilt(&watch, 0, 0));
    paint(80, ten_past_ten);
    assert(frame == closed);
    watch_toggle(&watch, 100);
    paint(370, ten_past_ten);
    Frame partial = frame;
    watch_toggle(&watch, 370);
    paint(370, ten_past_ten);
    assert(frame == partial);
    paint(700, ten_past_ten);
    assert(frame == closed);
    watch_toggle(&watch, 700);
    paint(1500, ten_past_ten);
    Frame open = frame;
    assert(watch_is_open(&watch, 1500));
    assert(changes(open, closed, 116,116,75) > 16000);
    assert(changes(partial, open, 116,116,75) > 1000);
    paint(1737, ten_past_ten);
    // Look inside each gear, away from the hands and the chapter ring.
    for (const auto center : {std::pair{82,114}, {134,75}, {167,119}, {140,155}, {82,161}})
        assert(changes(open, frame, center.first, center.second, 12) > 75);
    // Check the sliding wrist pin above the great wheel before and after a quarter turn.
    paint(4800, ten_past_ten);
    Frame crank_right = frame;
    paint(6000, ten_past_ten);
    assert(changes(crank_right, frame, 72,65,7) > 15);
    // The five wheel periods have a 336-second common cycle, independent of
    // the hands and of the ESP's 32-bit millisecond rollover.
    paint(1500 + 336000ull*14000, ten_past_ten);
    assert(frame == open);
    paint(1500, ten_past_ten + (3*3600+22*60+15)*1000);
    assert(changes(open, frame, 116,116,93) > 650);
    // Hold the mechanism still. At noon each hand points up. Advance one
    // quarter turn of each hand and check its own length along the right radius.
    paint(1500, 0);
    Frame noon = frame;
    paint(1500, 15000);
    assert(changes(noon, frame, 207, 116, 3) > 4); // seconds, outside the minute hand
    paint(1500, 15*60000);
    assert(changes(noon, frame, 188, 116, 3) > 8); // minutes, outside the hour hand
    paint(1500, 3*3600000);
    assert(changes(noon, frame, 161, 116, 3) > 8); // hours, other two hands at twelve
    if (argc == 3) {
        preview(argv[2], "closed", closed);
        preview(argv[2], "partial", partial);
        preview(argv[2], "open", open);
    }
    watch_toggle(&watch, 2000);
    paint(2800, ten_past_ten);
    assert(watch_is_closed(&watch, 2800) && frame == closed);
    puts("Watch cover reverses and closes, all five wheels turn and the hands follow the clock.");
}
