// Render an artboard snapshot with the same software backend as the board.
#include "rive_thorvg.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Options {
    fs::path input, output;
    std::string artboard;
    int width = 233, height = 233;
    double seconds = 0;
};

static int dimension(const std::string& value)
{
    int result = 0;
    const char* end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end || result < 1 || result > 4096)
        throw std::invalid_argument("each dimension must be between 1 and 4096");
    return result;
}

static Options options(int argc, char** argv)
{
    if (argc < 3 || (argc - 3) % 2)
        throw std::invalid_argument("usage: riv-render INPUT.riv OUTPUT.{ppm,rgb565} [--artboard NAME] [--size WIDTHxHEIGHT] [--time SECONDS]");
    Options opts;
    opts.input = argv[1];
    opts.output = argv[2];
    if (opts.output.extension() != ".ppm" && opts.output.extension() != ".rgb565")
        throw std::invalid_argument("use .ppm or .rgb565 for the output filename");
    if (fs::exists(opts.output) && fs::equivalent(opts.input, opts.output))
        throw std::invalid_argument("input and output must be different files");
    for (int i = 3; i < argc; i += 2) {
        const std::string flag = argv[i], value = argv[i+1];
        if (flag == "--artboard") opts.artboard = value;
        else if (flag == "--size") {
            const auto split = value.find('x');
            if (split == std::string::npos) throw std::invalid_argument("expected WIDTHxHEIGHT");
            opts.width = dimension(value.substr(0, split));
            opts.height = dimension(value.substr(split + 1));
        } else if (flag == "--time") {
            size_t used = 0;
            opts.seconds = std::stod(value, &used);
            if (used != value.size() || !std::isfinite(opts.seconds) || opts.seconds < 0 || opts.seconds > 600)
                throw std::invalid_argument("time must be between 0 and 600 seconds");
        } else throw std::invalid_argument("unknown option " + flag);
    }
    return opts;
}

static void write_frame(const Options& opts, const std::vector<uint32_t>& pixels)
{
    std::ofstream output(opts.output, std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    const bool ppm = opts.output.extension() == ".ppm";
    if (ppm) output << "P6\n" << opts.width << ' ' << opts.height << "\n255\n";
    for (uint32_t p : pixels) {
        // Premultiplied RGB gives the pixel color composited on black.
        if (ppm) {
            output.put(static_cast<char>(p >> 16));
            output.put(static_cast<char>(p >> 8));
            output.put(static_cast<char>(p));
        } else {
            uint16_t color = ((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0) | ((p >> 3) & 0x1f);
            output.put(static_cast<char>(color));
            output.put(static_cast<char>(color >> 8));
        }
    }
    output.close();
}

static void render(const Options& opts)
{
    std::ifstream input(opts.input, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open " + opts.input.string());
    std::vector<uint8_t> file{std::istreambuf_iterator<char>(input), {}};
    if (file.empty() || input.bad()) throw std::runtime_error("cannot read the Rive file");

    std::vector<uint32_t> pixels(opts.width * opts.height);
    std::unique_ptr<rive_instance_t, decltype(&rive_destroy)> scene(
        rive_create(pixels.data(), opts.width, opts.height, RIVE_PIXEL_FORMAT_ARGB8888), rive_destroy);
    if (!scene) throw std::runtime_error("cannot allocate the renderer");
    const char* board = opts.artboard.empty() ? nullptr : opts.artboard.c_str();
    if (!rive_load_ex(scene.get(), file.data(), file.size(), board, nullptr))
        throw std::runtime_error("could not load the Rive file or requested artboard");
    rive_set_fit(scene.get(), RIVE_FIT_CONTAIN, RIVE_ALIGN_CENTER);
    // Give transitions the same intermediate ticks they would see in playback.
    for (double remaining = opts.seconds; remaining > 0; remaining -= 1.0/60)
        rive_advance_state(scene.get(), static_cast<float>(std::min(remaining, 1.0/60)));
    rive_render(scene.get());
    write_frame(opts, pixels);
}

int main(int argc, char** argv)
{
    int status = EXIT_SUCCESS;
    rive_thorvg_init(0);
    try {
        render(options(argc, argv));
    } catch (const std::exception& err) {
        std::cerr << "riv-render: " << err.what() << '\n';
        status = EXIT_FAILURE;
    }
    rive_thorvg_term();
    return status;
}
