"""Capture the VU animation as a base RGB565 image and sparse per-pose patches."""
import argparse
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from raster_art import read_rgb565
from rive_art import ROOT

PROJECT = ROOT / "assets/vu-meter"
HEADER = (ROOT / "examples/baked-meter/main/vu_meter.h").read_text()
FPS, SECONDS, PATCH_X, PATCH_Y, PATCH_WIDTH, PATCH_HEIGHT = (
    int(re.search(rf"\bVU_METER_{key} = (\d+)", HEADER)[1])
    for key in ("FPS", "SECONDS", "PATCH_X", "PATCH_Y", "PATCH_WIDTH", "PATCH_HEIGHT")
)
SIZE = 466


def capture(cli, build):
    # The first GPU capture can differ from subsequent frames even at rest.
    commands = ["screenshot warmup.png", "advance 1"]
    for frame in range(FPS * SECONDS):
        if frame:
            commands.append(f"advance {1000/FPS:g}ms")
        commands.append(f"screenshot vu-{frame:03}.png")
    run = subprocess.run([cli, str(PROJECT), "--fit=contain", f"--viewport={SIZE}x{SIZE}", "--commands"],
                         input="\n".join(commands)+"\n", cwd=build, stdout=subprocess.PIPE, text=True, check=True)
    if any(line.startswith("err ") for line in run.stdout.splitlines()):
        raise RuntimeError(run.stdout)

    return read_rgb565(build/"vu-%03d.png", SIZE, SIZE, FPS*SECONDS)


def bake(cli):
    with tempfile.TemporaryDirectory() as temp:
        raw = capture(cli, Path(temp))
    frame_bytes = 2*SIZE*SIZE
    base = raw[:frame_bytes]
    if raw[-frame_bytes:] != base:
        raise ValueError("The needle must return to its starting pose")
    base_pixels = tuple(color for (color,) in struct.iter_unpack("<H", base))
    # Store each pose against the same base so a slow frame can jump to the right time.
    patches, offsets = bytearray(), [0]
    for start in range(0, len(raw), frame_bytes):
        pixels = struct.iter_unpack("<H", raw[start:start+frame_bytes])
        for p, (color,) in enumerate(pixels):
            if color == base_pixels[p]:
                continue
            row, column = divmod(p, SIZE)
            x, y = column-PATCH_X, row-PATCH_Y
            if not (0 <= x < PATCH_WIDTH and 0 <= y < PATCH_HEIGHT):
                raise ValueError(f"Needle change outside the patch area at ({column}, {row})")
            patches.extend(struct.pack("<HH", y*PATCH_WIDTH+x, color))
        offsets.append(len(patches))
    if not patches:
        raise ValueError("The needle must move during the demo")
    index = struct.pack(f"<{len(offsets)}I", *offsets)
    (PROJECT/"vu-meter.rgb565").write_bytes(base + index + patches)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rive", default="rive", help="path to the Rive CLI")
    bake(parser.parse_args().rive)
