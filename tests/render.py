"""Check the CLI output, animation clock, packing and error behavior on real art."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

exe, asset = sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    ppm = Path(directory) / "watch.ppm"
    raw = ppm.with_suffix(".rgb565")
    opts = ["--size", "160x120", "--artboard", "Movement"]
    subprocess.run([exe, asset, ppm, *opts], check=True)
    header = b"P6\n160 120\n255\n"
    frame = ppm.read_bytes()
    assert frame.startswith(header)
    rgb = frame[len(header):]
    assert len(rgb) == 160*120*3 and len(set(rgb)) > 32
    subprocess.run([exe, asset, raw, *opts], check=True)
    expected = b"".join(struct.pack("<H", (r & 248) << 8 | (g & 252) << 3 | b >> 3)
                        for r, g, b in zip(rgb[::3], rgb[1::3], rgb[2::3]))
    assert raw.read_bytes() == expected

    subprocess.run([exe, asset, ppm, *opts, "--time", "1"], check=True)
    after = ppm.read_bytes()
    assert frame != after, "the wheel train timeline should move"
    for args in (["--artboard", "missing"], ["--time", "nan"], ["--size", "0x100"]):
        assert subprocess.run([exe, asset, ppm, *args]).returncode != 0
        assert ppm.read_bytes() == after
