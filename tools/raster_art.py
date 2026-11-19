"""Read Rive CLI screenshots in the panel's native little-endian RGB565 format."""
from array import array
import subprocess
import sys


def _dither(rgb, width, height):
    # Apply the same 4x4 ordered thresholds to each channel in every frame.
    # Keep conversion in RGB so chroma subsampling cannot soften fine red edges.
    bayer = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))
    def table(bits, rank):
        return bytes(((v*((1<<bits)-1)*32 + (2*rank+1)*255)//(32*255)) << (8-bits)
                     for v in range(256))
    tables = [(table(5, i), table(6, i), table(5, i)) for i in range(16)]
    result = bytearray(rgb)
    stride = 3*width
    for start in range(0, len(rgb), stride):
        row = (start//stride) % height
        for x, rank in enumerate(bayer[row % 4]):
            for channel, lut in enumerate(tables[rank]):
                lane = slice(start+3*x+channel, start+stride, 12)
                result[lane] = rgb[lane].translate(lut)
    return result


def read_rgb565(images, width, height, frames=1, *, dither=False):
    rgb = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(images), "-frames:v", str(frames),
                                   "-pix_fmt", "rgb24", "-f", "rawvideo", "-"])
    if len(rgb) != width*height*frames*3:
        raise ValueError(f"Unexpected capture size for {images}")
    if dither:
        rgb = _dither(rgb, width, height)
    # Keep full chroma at hard edges when reducing each RGB triplet to 5/6/5 bits.
    colors = array("H", ((r & 248) << 8 | (g & 252) << 3 | b >> 3
                         for r, g, b in zip(rgb[0::3], rgb[1::3], rgb[2::3])))
    if sys.byteorder != "little":
        colors.byteswap()
    return colors.tobytes()
