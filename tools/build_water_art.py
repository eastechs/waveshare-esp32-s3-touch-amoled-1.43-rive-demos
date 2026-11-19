"""Bake the vessel depth, underwater lighting and seamless refracted-light texture."""
import math
import random
from itertools import chain
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIZE = 466
TILE = 128


def clamp(value, hi=255):
    return min(hi, max(0, round(value)))


def caustics():
    # Warped cell boundaries approximate the light focused by small surface ripples.
    rng = random.Random(81)
    cells = [[(.2+.6*rng.random(), .2+.6*rng.random()) for _ in range(5)] for _ in range(5)]
    data = bytearray()
    for y in range(TILE):
        for x in range(TILE):
            u, v = x/TILE, y/TILE
            a = 5*u + .34*math.sin(math.tau*(v+u)) + .22*math.cos(math.tau*2*v)
            b = 5*v + .37*math.sin(math.tau*u) + .17*math.sin(math.tau*(u-2*v))
            distances = []
            for j in range(math.floor(b)-1, math.floor(b)+2):
                for i in range(math.floor(a)-1, math.floor(a)+2):
                    ox, oy = cells[j%5][i%5]
                    distances.append(math.hypot(i+ox-a, j+oy-b))
            nearest, second = sorted(distances)[:2]
            width = second-nearest
            data.append(clamp(180*math.exp(-width*width/.008)+68*math.exp(-width*width/.055)))
    return data


def main():
    art = bytearray()
    for row in range(SIZE):
        # Keep the even-column samples together for sequential reads from flash.
        for col in chain(range(0, SIZE, 2), range(1, SIZE, 2)):
            x, y = (row+.5)/233-1, 1-(col+.5)/233
            r2 = x*x+y*y
            if r2 >= 1:
                art.extend((0,0))
                continue
            z = math.sqrt(1-r2)
            # Broad transmitted light in the water, brightest in the shallows.
            a, b = .76*x+.65*y, -.65*x+.76*y
            pool = math.exp(-((a-.65)/.24)**2-(b/.75)**2)
            wet = 69 + 82*z - 47*y - 34*x + 50*pool
            dither = ((0,2),(3,1))[row%2][col%2]-1.5
            art.extend((max(1,round(233*z)),clamp(wet+dither)))
    art.extend(caustics())
    (ROOT/'assets/tilt-water/water.glass').write_bytes(art)


if __name__ == '__main__':
    main()
