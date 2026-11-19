#include "display.h"
#include "rive_thorvg.h"
extern "C" {
#include "ring_drag.h"
#include "thermostat_view.h"
}
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

using Frame = std::vector<uint32_t>;

static void gestures()
{
    ring_drag_t ring{};
    thermostat_drag_t dial{};
    assert(!ring_begin(&ring, 233, 233));
    assert(ring_begin(&ring, 233, 20));
    assert(thermostat_begin(&dial, ring.angle) && dial.setpoint == THERMOSTAT_INITIAL);
    assert(thermostat_move(&dial, ring_turn(&ring, 446, 233)));
    assert(dial.setpoint > THERMOSTAT_INITIAL);
    assert(thermostat_move(&dial, 180) && dial.setpoint == THERMOSTAT_HIGH);
    assert(!thermostat_move(&dial, 180));
    assert(thermostat_move(&dial, -360) && dial.setpoint == THERMOSTAT_LOW);
    assert(!thermostat_begin(&dial, 180));
}

// Count changed bright glyph pixels independently of the changing landscape.
static int ink_difference(const Frame& a, const Frame& b, int x0, int y0, int x1, int y1)
{
    auto is_ink = [](uint32_t c) {
        return ((c>>16)&255) + ((c>>8)&255) + (c&255) > 510;
    };
    int count = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            int i = y*RENDER_SIZE+x;
            count += is_ink(a[i]) != is_ink(b[i]);
        }
    return count;
}

static void save(const Frame& pixels, const char *prefix, int temp)
{
    std::ofstream out(std::string(prefix)+std::to_string(temp)+".ppm", std::ios::binary);
    out << "P6\n" << RENDER_SIZE << ' ' << RENDER_SIZE << "\n255\n";
    for (uint32_t c : pixels) { out.put(c>>16); out.put(c>>8); out.put(c); }
    assert(out.good());
}

int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
    gestures();
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
    Frame pixels(RENDER_SIZE*RENDER_SIZE);
    rive_thorvg_init(0);
    auto *scene = rive_create(pixels.data(),RENDER_SIZE,RENDER_SIZE,RIVE_PIXEL_FORMAT_ARGB8888);
    assert(scene && rive_load_ex(scene,bytes.data(),bytes.size(),"Thermostat","Climate"));
    rive_set_fit(scene,RIVE_FIT_CONTAIN,RIVE_ALIGN_CENTER);
    assert(rive_advance(scene,0));
    auto initial=pixels;
    thermostat_view_t view{};
    assert(thermostat_bake(&view, scene, pixels.data()));
    rive_vm_set_number(scene,"surfaceOpacity",1);
    Frame composite(pixels.size());
    Frame cold, holding, previous;
    for (int temp=THERMOSTAT_LOW; temp<=THERMOSTAT_HIGH; ++temp) {
        rive_vm_set_number(scene,"setpoint",temp);
        assert(rive_advance(scene,0));
        assert(pixels.front() == 0);
        assert(pixels[116*RENDER_SIZE+116] >> 24 == 255);
        float radians=(temp-THERMOSTAT_INITIAL)*12*.017453293f;
        int x=lroundf(116+106*sinf(radians)), y=lroundf(116-106*cosf(radians));
        uint32_t selected=pixels[y*RENDER_SIZE+x];
        assert(((selected>>16)&255)>225 && ((selected>>8)&255)>225 && (selected&255)>225);
        if (!previous.empty()) assert(ink_difference(pixels,previous,70,75,151,124)>100);
        if (temp==THERMOSTAT_LOW) cold=pixels;
        if (temp==DEMO_ROOM_F) holding=pixels;
        if (temp==THERMOSTAT_INITIAL) assert(pixels==initial);
        thermostat_paint(&view,temp,composite.data());
        unsigned total_error = 0;
        for (size_t p=0; p<pixels.size(); ++p) {
            for (int shift : {0,8,16}) {
                int error = std::abs(int((composite[p]>>shift)&255)-int((pixels[p]>>shift)&255));
                total_error += error;
                assert(error < 12);
            }
        }
        assert(total_error < pixels.size()*3);
        if (argc==3) save(composite,argv[2],temp);
        previous=pixels;
    }
    const int sky = 45*RENDER_SIZE+116;
    assert(((cold[sky]>>16)&255)+16 < ((pixels[sky]>>16)&255));
    assert((cold[sky]&255) > ((cold[sky]>>16)&255)+20);
    assert(ink_difference(cold,holding,79,187,151,198)>50);
    assert(ink_difference(holding,pixels,79,187,151,198)>50);
    thermostat_free(&view);
    rive_destroy(scene);
    rive_thorvg_term();
    puts("All 25 thermostat settings, ring capture, end stops and palette shifts passed.");
}
