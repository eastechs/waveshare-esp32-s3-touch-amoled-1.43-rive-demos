"""Bake the changed board layers into premultiplied ARGB for the firmware."""
from pathlib import Path
import shutil
import subprocess
import argparse
import tempfile
import xml.etree.ElementTree as ET
import zlib

from rive_art import ROOT, add, fill

PARTS = [("Hunter cover", "watch-lid", 233), ("Dial", "watch-dial", 233), ("Parts tray", "watch-tray", 466)]


def bake(cli):
    source = ROOT / "assets/pocket-watch"
    with tempfile.TemporaryDirectory() as tmp:
        project = Path(tmp)
        shutil.copyfile(source/"rive.yaml", project/"rive.yaml")
        pending = {}
        for board_name, asset, size in PARTS:
            root = ET.parse(source/"scene.rml")
            board = root.find(f".//Artboard[@name='{board_name}']")
            w, h = float(board.get("width")), float(board.get("height"))
            background = add(board, "Shape", name="Capture background", x=w/2, y=h/2)
            add(background, "Rectangle", width=w, height=h)
            fill(background, "FF000000")
            color = background.find("Fill/SolidColor")
            samples = []
            for matte in ("FF000000", "FFFFFFFF"):
                color.set("colorValue", matte)
                root.write(project/"scene.rml", encoding="utf-8")
                png = project/"capture.png"
                subprocess.run([cli, str(project), f"--artboard={board_name}", "--fit=contain",
                                f"--viewport={size}x{size}", f"--screenshot={png}"], check=True)
                samples.append(subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(png),
                                "-pix_fmt", "rgb24", "-f", "rawvideo", "-"]))
            # The black capture gives premultiplied RGB. Rendering on white adds
            # (1-alpha) to all three channels, including at antialiased edges.
            black, white = samples
            if len(black) != 3*size*size or len(white) != len(black):
                raise ValueError(f"Incomplete {board_name} capture")
            pixels = bytearray(4*size*size)
            for c in range(3):
                pixels[2-c::4] = black[c::3]
            pixels[3::4] = bytes(255-max(0, w-b) for w,b in zip(white[1::3], black[1::3]))
            pending[asset] = zlib.compress(pixels, level=9)
        for asset, data in pending.items():
            (source/(asset+".argb.z")).write_bytes(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rive", default="rive", help="path to the Rive CLI")
    bake(parser.parse_args().rive)
