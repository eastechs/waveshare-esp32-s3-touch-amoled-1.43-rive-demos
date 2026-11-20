"""Draw Inter labels as vector paths for the embedded renderer."""
from math import atan2, hypot

from fontTools.pens.basePen import BasePen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

from rive_art import ROOT, add, fill

FONT = ROOT / "deps/rive-framebuffer/vendor/rive-thorvg/rive-runtime/tests/unit_tests/assets/fonts/Inter_18pt-Regular.ttf"


class RivePath(BasePen):
    """Send font or SVG pen commands to a closed Rive PointsPath."""
    def __init__(self, shape, glyphs=None):
        super().__init__(glyphs)
        self.shape = shape

    def _moveTo(self, p):
        self.nodes = [[p, None, None]]

    def _lineTo(self, p):
        self.nodes.append([p, None, None])

    def _curveToOne(self, a, b, end):
        self.nodes[-1][2] = a
        self.nodes.append([end, b, None])

    def _closePath(self):
        if self.nodes[-1][0] == self.nodes[0][0]:
            self.nodes[0][1] = self.nodes.pop()[1]
        path = add(self.shape, "PointsPath", isClosed="true")
        for (x, y), incoming, outgoing in self.nodes:
            if incoming is None and outgoing is None:
                add(path, "StraightVertex", x=x, y=y)
                continue
            attrs = {}
            for prefix, handle in (("in", incoming), ("out", outgoing)):
                a, b = handle or (x, y)
                attrs[prefix + "Distance"] = hypot(a - x, b - y)
                attrs[prefix + "Rotation"] = atan2(b - y, a - x)
            add(path, "CubicDetachedVertex", x=x, y=y, **attrs)

    _endPath = _closePath


def label(parent, text, cx, baseline, size, color="FF4D2C17", font_path=FONT):
    with TTFont(font_path) as font:
        glyphs, cmap = font.getGlyphSet(), font.getBestCmap()
        letters = [glyphs[cmap[ord(ch)]] for ch in text]
        scale = size / font["head"].unitsPerEm
        spacing = size * 0.12
        x = cx - (sum(g.width for g in letters)*scale + spacing*(len(letters)-1))/2
        shape = add(parent, "Shape", name=text)
        for glyph in letters:
            glyph.draw(TransformPen(RivePath(shape, glyphs), (scale, 0, 0, -scale, x, baseline)))
            x += glyph.width*scale + spacing
        fill(shape, color, rule="nonZero")
