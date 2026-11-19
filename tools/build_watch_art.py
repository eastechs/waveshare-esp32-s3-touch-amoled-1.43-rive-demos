"""Author the hunter case, enamel dial and individually pivoted watch components."""
import math
import re
import xml.etree.ElementTree as ET

from rive_art import ROOT, add, animate, circle, fill, gradient, line, polygon, stroke

PROJECT = ROOT / "assets/watch"
C = 233
TAU = math.tau
BRASS = [(0, "FFFDE3A4"), (.22, "FFC49653"), (.46, "FFEDC67B"),
         (.64, "FFAC793B"), (.84, "FFE3B66A"), (1, "FF80512B")]
COPPER = [(0, "FFFFD5AA"), (.28, "FFCA8556"), (.5, "FFF1B58B"),
          (.78, "FFB7744D"), (1, "FF6F4132")]
STEEL = [(0, "FF4D8797"), (.46, "FF224858"), (1, "FF102733")]
INK = "FF302C25"
GOLD = "FFF0CD87"


def polar(r, a):
    return r * math.cos(a), r * math.sin(a)


def disk(parent, name, x, y, r, color):
    shape = circle(parent, name, x, y, r)
    if isinstance(color, str):
        fill(shape, color)
    else:
        gradient(add(shape, "Fill"), "LinearGradient", (-r*.8, -r*.85), (r*.8, r*.85), color)


def rim(parent, name, x, y, r, width, color):
    stroke(circle(parent, name, x, y, r), width, color)


def face(parent, name, points, color):
    shape = add(parent, "Shape", name=name)
    polygon(shape, points)
    fill(shape, color)


def screw(parent, x, y, angle):
    slot = add(parent, "Shape", name="Screw slot", x=x, y=y, rotation=angle)
    line(slot, [(-3.6, 0), (3.6, 0)])
    stroke(slot, 1.8, "FF392E29")
    disk(parent, "Screw head", x, y, 5, BRASS)
    disk(parent, "Countersink", x, y, 6.4, "FF41382A")


def case_lid(board):
    center = add(board, "Node", name="Engine-turned cover", x=C, y=C)
    for y in (-22, 0, 22):
        barrel = add(center, "Shape", name="Hinge knuckle", x=-218, y=y)
        add(barrel, "Rectangle", width=14, height=19, cornerRadiusTL=3)
        gradient(add(barrel, "Fill"), "LinearGradient", (-7,0), (7,0),
                 [(0,"FF664027"), (.32,"FFF6D297"), (.52,"FFE5B877"), (1,"FF76502F")])
    clasp = add(center, "Shape", name="Thumb catch", x=218)
    polygon(clasp, [(-4,-14),(2,-17),(7,-11),(7,11),(2,17),(-4,14)])
    gradient(add(clasp, "Fill"), "LinearGradient", (-4,-8), (7,10), BRASS)
    disk(center, "Cabochon glint", -2, -2, 2, "FFF7DAB4")
    disk(center, "Garnet", 0, 0, 7, [(0, "FFF5AA82"), (.35, "FFAF4842"), (1, "FF54282A")])
    rim(center, "Jewel setting", 0, 0, 8, 2.4, GOLD)
    for i in range(16):
        a = TAU*i/16 - TAU/4
        tip = polar(94 if i % 4 == 0 else 69 if i % 2 == 0 else 46, a)
        left, right = polar(24, a-TAU/32), polar(24, a+TAU/32)
        face(center, "Rose light facet", [(0, 0), left, tip], "FFF7DA99")
        face(center, "Rose shadow facet", [(0, 0), tip, right], "FFAE763F")
    for r, w, col in [(101, 2, GOLD), (107, 2, "FF94703E"), (130, 1.8, GOLD),
                       (135, 3, "FF7B542E"), (142, 2, "FFFBE4AC")]:
        rim(center, "Medallion beading", 0, 0, r, w, col)
    lace = add(center, "Shape", name="Interlaced rose engine engraving")
    for phase in (0, math.pi):
        line(lace, [polar(116 + 9*math.sin(24*TAU*j/432+phase), TAU*j/432) for j in range(433)])
    stroke(lace, 1.8, "FFBA9A60")
    disk(center, "Petrol enamel", 0, 0, 133, [(0, "FF274E50"), (.45, "FF13383A"), (1, "FF091E25")])
    cuts = add(center, "Shape", name="Sunray fluting")
    for j in range(90):
        a = TAU*j/90
        line(cuts, [polar(144, a), polar(168, a+.018), polar(193, a+.045)])
    stroke(cuts, 1.8, "AA8D6236")
    for r, w, col in [(197, 3, "FF7C522D"), (200, 2, "FFFFE4A5"), (210, 2, "FF876135"),
                       (216, 2, "FFFFDF97"), (226, 3, "FF6C482A")]:
        rim(center, "Turned brass", 0, 0, r, w, col)
    disk(center, "Polished hunter case", 0, 0, 229, BRASS)


def numerals(parent):
    romans = ("XII", "I", "II", "III", "IIII", "V", "VI", "VII", "VIII", "IX", "X", "XI")
    glyphs = {"I": (4, [[(2,-9),(2,9)]]), "V": (12, [[(0,-9),(6,9),(12,-9)]]),
              "X": (12, [[(0,-9),(12,9)], [(12,-9),(0,9)]])}
    for index, roman in enumerate(romans):
        a = TAU*index/12
        x, y = polar(189, a-TAU/4)
        # Turn the lower numerals upright, like a double-sunk enamel dial.
        shape = add(parent, "Shape", name=roman, x=x, y=y, rotation=a-(math.pi if 4 <= index <= 8 else 0))
        offset = -(sum(glyphs[ch][0] for ch in roman) + (len(roman)-1)*3.5)/2
        for ch in roman:
            width, paths = glyphs[ch]
            for path in paths:
                line(shape, [(offset+px, py) for px, py in path])
                for px, py in (path[0], path[-1]):
                    line(shape, [(offset+px-2.2,py), (offset+px+2.2,py)])
            offset += width+3.5
        stroke(shape, 2.6, INK)


def dial(board):
    p = add(board, "Node", name="Chapter ring and mainplate", x=C, y=C)
    numerals(p)
    for major in (False, True):
        ticks = add(p, "Shape", name="Hour batons" if major else "Railway minutes")
        for j in range(60):
            if (j % 5 == 0) == major:
                a = TAU*j/60
                line(ticks, [polar(202 if major else 205, a), polar(213, a)])
        stroke(ticks, 3.5 if major else 1.7, INK)
    rim(p, "Railway outer track", 0, 0, 214, 1.5, INK)
    rim(p, "Railway inner track", 0, 0, 202, 1, "FFBCA980")
    for x, y, a in [(-105,-83,.6), (91,-82,-.4), (-121,43,-.3), (8,132,1)]:
        screw(p, x, y, a)
    # The bridge feet extend beyond the wheels into the recess. Each has a
    # countersunk fastener and a narrow highlight above the milled shoulder.
    for path in [[(-105,-83),(-110,-58),(-100,-29),(-69,-5)],
                 [(8,132),(27,123),(40,103),(47,77)],
                 [(91,-82),(108,-68),(123,-48),(121,-14),(101,5)]]:
        for name, dy, width, color in [("Bridge bevel",-1,12,"FF789999"),
                                      ("Oxidized steel",1,17,"FF435F65"), ("Bridge shadow",4,20,"FF0A191F")]:
            shape = add(p, "Shape", name=name, y=dy)
            line(shape, path)
            stroke(shape, width, color)
    for rr in (148, 153):
        rim(p, "Machined plate", 0, 0, rr, 1.3, "FF647367")
    disk(p, "Skeleton recess", 0, 0, 163, [(0, "FF293F3D"), (.55, "FF132D31"), (1, "FF0B1C21")])
    disk(p, "Cutout lip", 0, 0, 166, BRASS)
    disk(p, "Recess shadow", 0, 0, 169, "FF614A32")
    disk(p, "Ivory enamel", 0, 0, 216, [(0, "FFFFF3CF"), (.55, "FFF4E3BA"), (1, "FFD5BE91")])
    for rr, col in [(218, "FF6F4B2A"), (222, GOLD), (226, "FF5A432F")]:
        rim(p, "Bezel engraving", 0, 0, rr, 2, col)
    disk(p, "Case band", 0, 0, 232, BRASS)


def wheel(board, teeth, copper):
    size = 5*teeth+16
    r = 2.5*teeth
    center = add(board, "Node", name="Wheel blank", x=size/2, y=size/2)
    disk(center, "Ruby highlight", -1.5, -1.8, 1.5, "FFFFDBB7")
    disk(center, "Ruby bearing", 0, 0, 4.5, "FFA74141")
    disk(center, "Bearing collet", 0, 0, 8.1, BRASS)
    flank = add(center, "Shape", name="Hobbed rim and pierced spokes")
    polygon(flank, [polar(r+dr, TAU*(i+fraction)/teeth) for i in range(teeth)
                    for fraction, dr in [(-.5,-3),(-.32,-3),(-.16,3.4),(.16,3.4),(.32,-3)]])
    for j in range(5):
        # With even-odd fill each sector is a cut through the metal wheel.
        a = TAU*j/5
        polygon(flank, [polar(r*.27, a+.17), polar(r*.79, a+.09)] +
                [polar(r*.79, a+.09+(TAU/5-.18)*k/9) for k in range(1,10)] +
                [polar(r*.27, a+TAU/5-.17)])
    gradient(add(flank, "Fill", fillRule="evenOdd"), "LinearGradient", (-r,-r), (r,r), COPPER if copper else BRASS)
    rim(center, "Wheel burnish", 0, 0, r*.84, 1.6, "FFF1CC8B")


def hand(board, kind):
    p = add(board, "Node", name=kind, x=C, y=C)
    if kind == "Sweep hand":
        disk(p, "Sweep cap", 0, 0, 6, BRASS)
        face(p, "Vermilion sweep", [(-1.2,-198),(1.2,-198),(2,43),(-2,43)], "FFE95235")
        rim(p, "Counterbalance", 0, 32, 6, 3, "FFE95235")
        return
    length, width = (117, 10) if kind == "Hour hand" else (166, 7)
    # Blued steel with a pale inlaid ridge makes the hands legible over the gears.
    face(p, "Luminous ridge", [(0,-length+6),(width*.32,-length*.72),(1,4),(-1,4)], "FFFBE9B9")
    outline = [(0,-length), (width,-length*.73), (width*.36,-16), (width*.78,6),
               (0,16),(-width*.78,6),(-width*.36,-16),(-width,-length*.73)]
    shape = add(p, "Shape", name="Blued steel")
    polygon(shape, outline)
    gradient(add(shape, "Fill"), "LinearGradient", (-width,0), (width,0), STEEL)
    stroke(shape, 2.2, GOLD)


def main():
    root = ET.Element("Rive", version="1", kind="fragment")
    def board(name, idx, size=466, component=True):
        return add(root, "Artboard", name=name, id=f"{idx}:1", width=size, height=size,
                   isComponent=str(component).lower())
    closed = board("Pocket watch", 1, component=False)
    inside = board("Movement", 2, component=False)
    def nested(parent, idx, name, **attrs):
        return add(parent, "NestedArtboard", name=name, artboardId=f"{idx}:1", **attrs)
    nested(closed, 3, "Hunter cover")
    nested(closed, 4, "Case band")
    case_lid(board("Hunter cover", 3))
    dial(board("Dial", 4))
    timeline = add(inside, "LinearAnimation", name="Wheel train", duration=6720, fps=20, loopValue="1")
    layout = (PROJECT/"parts.def").read_text()
    def parts(kind):
        for name, numbers in re.findall(rf'^{kind}\("([^"]+)",([^)]*)\)', layout, re.M):
            yield (name, *map(int, numbers.split(",")))
    wheels = list(parts("WHEEL"))
    tray = board("Parts tray", 30)
    for j, (name, tx, ty, *_bounds) in reversed(list(enumerate(parts("HAND")))):
        angle = (-TAU/6, TAU/6, 2.7)[j]
        pivot = add(inside, "Node", name=name+" pivot", x=C, y=C, rotation=angle)
        nested(pivot, 20+j, name, x=-C, y=-C)
        nested(tray, 20+j, name, x=tx-C, y=ty-C)
        hand(board(name, 20+j), name)
    phases, signs = [], []
    for i, (name,x,y,n,driver,tx,ty) in enumerate(wheels):
        size = 5*n+16
        phase, sign = 0, 1
        if driver >= 0:
            _, px, py, pn, *_ = wheels[driver]
            a = math.atan2(y-py, x-px)
            phase = (a+math.pi+(pn*(a-phases[driver])-math.pi)/n) % (TAU/n)
            sign = -signs[driver]
        phases.append(phase)
        signs.append(sign)
        pivot = add(inside, "Node", name=name+" arbor", x=x, y=y, id=f"2:{i+2}", rotation=phase)
        nested(pivot, 5+i, name, x=-size/2, y=-size/2)
        animate(timeline, pivot, rotation=[(0, phase), (6720, phase+sign*TAU*1680/n)])
        nested(tray, 5+i, name, x=tx-size/2, y=ty-size/2)
        wheel(board(name, 5+i, size), n, i in (1,4))
    nested(inside, 4, "Enamel chapter ring")
    for component in root.findall("Artboard"):
        if component.get("isComponent") == "true":
            add(root, "ComponentAsset", name=component.get("name"), artboardId=component.get("id"))
    ET.indent(root, space="  ")
    (PROJECT/"scene.rml").write_text(ET.tostring(root, encoding="unicode")+"\n")


if __name__ == "__main__":
    main()
