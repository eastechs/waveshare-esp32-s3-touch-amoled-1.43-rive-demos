"""Draw the brass VU meter and animate its needle over a demo signal."""
import math
import re
import xml.etree.ElementTree as ET

from outline_text import FONT, label
from rive_art import ROOT, add, animate, circle, fill, gradient, line, polygon, stroke

PROJECT = ROOT / "assets/vu-meter"
HEADER = (ROOT / "examples/baked-meter/main/vu_meter.h").read_text()
SECONDS = int(re.search(r"VU_METER_SECONDS = (\d+)", HEADER)[1])
X, Y, WIDTH, HEIGHT = 74, 134, 318, 212
SIZE, CENTER, PIVOT = 466, 233, 324
# Keep the outside of the brass at 230 and compress its old 45px section by a quarter.
GLASS_RADIUS = 196.25
TIMELINE_FPS = 60
BOLD = FONT.with_name("Inter_28pt-Bold.ttf")
INK, RED = "FF3E2716", "FFA62B19"
SCALE = [(-20, -52), (-10, -42), (-7, -31), (-5, -21), (-3, -10), (-2, 0),
         (-1, 10), (0, 20), (1, 30), (2, 40), (3, 51)]
STEEL = [(0, "FF4A3C34"), (.19, "FFCFC3AF"), (.42, "FF8D7B69"),
         (.62, "FF55514E"), (.83, "FF9B8A75"), (1, "FF352C27")]


def dial_point(angle, radius, y=PIVOT):
    a = math.radians(angle)
    return CENTER + radius*math.sin(a), y - radius*math.cos(a)


def scale_angle(db):
    for (lo, a), (hi, b) in zip(SCALE, SCALE[1:]):
        if db <= hi:
            return a + (b-a)*(db-lo)/(hi-lo)
    return SCALE[-1][1]


def disk(parent, name, x, y, radius, stops, axis=None, kind="LinearGradient"):
    shape = circle(parent, name, x, y, radius)
    start, end = axis or ((-.55*radius, -.85*radius), (.55*radius, .85*radius))
    gradient(add(shape, "Fill"), kind, start, end, stops)


def arc(parent, name, start, end, radius, width, color, y=PIVOT):
    shape = add(parent, "Shape", name=name)
    line(shape, [dial_point(a, radius, y) for a in range(start, end+1, 2)] +
         [dial_point(end, radius, y)])
    stroke(shape, width, color)


def screw(parent, x, y, angle, radius):
    slot = add(parent, "Shape", name="Screw slot", x=x, y=y, rotation=angle)
    line(slot, [(-.59*radius, -.6), (.59*radius, -.6)])
    stroke(slot, max(2, radius*.24), "FF352923")
    disk(parent, "Slotted screw", x, y, radius, STEEL)
    fill(circle(parent, "Countersink", x, y+.6, radius+2), "FF5F3D24")


def dial_print(parent):
    label(parent, "VU", CENTER, 248, 32, INK, BOLD)
    for db, angle in SCALE:
        ink = RED if db >= 0 else INK
        mark = add(parent, "Shape", name=f"{db:+d} VU graduation")
        line(mark, [dial_point(angle, 184), dial_point(angle, 197)])
        stroke(mark, 3, ink)
        x, y = dial_point(angle, 212)
        text = add(parent, "Node", name=f"{db:+d} VU label", x=x, y=y,
                   rotation=math.radians(angle*.72))
        label(text, str(abs(db)), 0, 7, 21 if db <= -10 else 23, ink, BOLD)
    for symbol, angle, ink in [("−", -59, INK), ("+", 58, RED)]:
        x, y = dial_point(angle, 184)
        label(parent, symbol, x, y+7, 24, ink, BOLD)
    for (lo, _), (hi, _) in zip(SCALE, SCALE[1:]):
        angle = scale_angle((lo+hi)/2)
        mark = add(parent, "Shape", name="Intermediate graduation")
        line(mark, [dial_point(angle, 184), dial_point(angle, 191)])
        stroke(mark, 1.9, RED if lo >= 0 else INK)
    arc(parent, "VU scale", -52, 20, 184, 3.3, INK)
    arc(parent, "Overload scale", 20, 51, 184, 3.3, RED)
    for percent in (20, 40, 60, 80, 100):
        angle = scale_angle(20*math.log10(percent/100))
        x, y = dial_point(angle, 173)
        fill(circle(parent, "Percent index", x, y, 2), INK)
        x, y = dial_point(angle, 156)
        label(parent, f"{percent}%" if percent == 100 else str(percent), x, y+4, 15, INK)


def movement(parent):
    screw(parent, CENTER, 380, -1.62, 14)
    # The lower guard meets the circular glass at its outer edge.
    start = math.ceil(math.degrees(math.acos((CENTER-344)/GLASS_RADIUS)))
    guard = add(parent, "Shape", name="Lower movement guard")
    polygon(guard, [dial_point(a, GLASS_RADIUS, CENTER) for a in range(start, 361-start)])
    gradient(add(guard, "Fill"), "LinearGradient", (225, 343), (233, 416),
             [(0, "FF9C7351"), (.055, "FF443329"), (.18, "FF2C241F"), (1, "FF110E0E")])
    for x, lean in [(151, .72), (193, -.22), (273, .24), (315, -.8)]:
        screw(parent, x, 325, lean, 8 if x in (151, 315) else 5.6)
    bridge = add(parent, "Shape", name="Polished movement bridge", x=CENTER, y=327)
    add(bridge, "Rectangle", width=189, height=30, cornerRadiusTL=11)
    gradient(add(bridge, "Fill"), "LinearGradient", (-8, -14), (6, 15), STEEL)
    for i in range(12):
        turn = add(parent, "Shape", name="Copper coil winding", x=194+i*7)
        line(turn, [(1, 301), (-2, 314)])
        stroke(turn, 2.7, "FFDC9D72" if i % 3 == 0 else "FF936385")
    coil = add(parent, "Shape", name="Coil former", x=CENTER, y=306)
    add(coil, "Rectangle", width=103, height=21, cornerRadiusTL=7)
    gradient(add(coil, "Fill"), "LinearGradient", (0, -10), (0, 10),
             [(0, "FF60434B"), (.44, "FF814F4A"), (1, "FF302125")])
    recess = add(parent, "Shape", name="Movement aperture")
    polygon(recess, [dial_point(a, 57) for a in range(-90, 91, 3)])
    gradient(add(recess, "Fill"), "LinearGradient", (221, 266), (242, 319),
             [(0, "FF211918"), (.56, "FF31282C"), (1, "FF715144")])
    screw(parent, 98, 286, -.84, 10)
    screw(parent, 368, 286, .61, 10)


def face(parent):
    movement(parent)
    dial_print(parent)
    disk(parent, "Warm lamp reflected through glass", CENTER, CENTER, GLASS_RADIUS,
         [(0, "5CFFD67E"), (.54, "1AFFB05F"), (1, "00FFB05F")],
         ((-98, 56), (119, 97)), "RadialGradient")
    disk(parent, "Translucent amber dial", CENTER, CENTER, GLASS_RADIUS,
         [(0, "FFD7A868"), (.27, "FFF7DA94"), (.55, "FFFFE7B1"), (.78, "FFF5C378"), (1, "FFC47936")],
         ((-110, -178), (95, 170)))
    arc(parent, "Glass edge reflection", -77, -25, 197.75, 1.6, "A6FFE5B2", CENTER)
    arc(parent, "Polished brass glint", 50, 133, 204.5, 2.6, "BFFFF0A9", CENTER)
    arc(parent, "Lower polished edge", 143, 234, 201.5, 1.8, "A6F9C477", CENTER)
    disk(parent, "Inner wall of the bezel", CENTER, CENTER, 201.5,
         [(0, "FF442411"), (.34, "FF603612"), (.54, "FFAA702D"), (.74, "FF774815"), (1, "FFF3B749")])
    disk(parent, "Bright inside bevel", CENTER, CENTER, 205.25,
         [(0, "FF75410B"), (.23, "FFFFD56B"), (.44, "FFFFED9B"), (.72, "FF895107"), (1, "FF43220D")],
         ((-126, 120), (107, -118)))
    for r, brightness in [(208.25, "4CECC36D"), (210.5, "3C703308"), (225.5, "58FFE18A")]:
        stroke(circle(parent, "Lathe line", CENTER, CENTER, r), 1.35, brightness)
    disk(parent, "Turned brass bezel", CENTER, CENTER, 227.0,
         [(0, "FF784105"), (.18, "FFC07D1C"), (.33, "FFEFAF31"), (.49, "FFA95D0A"),
          (.66, "FFDF941C"), (.81, "FFFFD454"), (.91, "FFFCE78D"), (1, "FFC48114")],
         ((-149, -72), (183, 48)))
    disk(parent, "Bezel outer chamfer", CENTER, CENTER, 230,
         [(0, "FF5B452B"), (.32, "FFB47F36"), (.55, "FF7F4918"), (1, "FF1D1410")])
    disk(parent, "Black instrument housing", CENTER, CENTER, 233,
         [(0, "FF2E2621"), (1, "FF120E0C")])


def needle(parent):
    machine = add(parent, "StateMachine", name="Meter demo", id="3:2")
    layer = add(machine, "StateMachineLayer", name="Demo signal")
    add(layer, "AnyState")
    add(layer, "ExitState")
    add(add(layer, "EntryState"), "StateTransition", stateToId="3:3")
    add(layer, "AnimationState", id="3:3", animationId="3:4")
    timeline = add(parent, "LinearAnimation", name="Demo levels", id="3:4",
                   duration=SECONDS*TIMELINE_FPS, fps=TIMELINE_FPS, loopValue="1")

    x, y = CENTER-X, PIVOT-Y
    fill(circle(parent, "Pivot center", x, y, 3.8), "FF665043")
    disk(parent, "Spindle cap", x, y, 11.5, STEEL)
    fill(circle(parent, "Spindle collar", x, y+1, 15), "FF534135")
    pointer = add(parent, "Node", name="Copper needle", x=x, y=y, id="3:5",
                  rotation=math.radians(SCALE[0][1]))
    for name, points, color in [
        ("Lit needle edge", [(-.4,-183),(.45,-183),(-.5,-8),(-2,-8)], "FFB45B37"),
        ("Tapered needle", [(0,-185),(2.6,-175),(3.6,-9),(-3.6,-9),(-2.2,-175)], "FF731F13"),
        ("Needle shadow", [(0,-184),(3.7,-171),(5.8,1),(-2,0),(-1.5,-173)], "4A44261A"),
    ]:
        shape = add(pointer, "Shape", name=name)
        polygon(shape, points)
        fill(shape, color)

    # Attacks land quickly and decay over several more frames, like a moving coil.
    levels = [(0,-20), (19,-20), (34,-6), (41,-4.6), (54,-8), (72,-10), (86,-2.8),
              (94,-2.4), (111,-6.3), (123,-3.4), (136,-5.5), (155,-8), (169,-1.7),
              (178,-1.2), (199,-5.4), (216,.7), (222,1.1), (241,-3.4), (256,-6),
              (266,-2.1), (283,-5), (302,-8.5), (319,2.5), (324,3), (343,-2), (363,-5.5),
              (375,-3), (393,-9), (414,-12), (448,-20), (SECONDS*TIMELINE_FPS,-20)]
    animate(timeline, pointer, rotation=[(t, math.radians(scale_angle(db))) for t, db in levels])


def main():
    root = ET.Element("Rive", version="1", kind="fragment")
    preview = add(root, "Artboard", name="VU meter", id="1:1", width=SIZE, height=SIZE)
    # Fit the brass diameter to the panel. Scale the needle about the same center.
    scale = CENTER/230
    offset = CENTER*(1-scale)
    instrument = add(preview, "Node", name="Panel fit", x=offset, y=offset, scaleX=scale, scaleY=scale)
    overlay = add(instrument, "NestedArtboard", name="Needle and pivot", artboardId="3:1", x=X, y=Y)
    add(overlay, "NestedStateMachine", animationId="3:2")
    add(instrument, "NestedArtboard", name="Meter face", artboardId="2:1")
    face(add(root, "Artboard", name="Meter face", id="2:1", width=SIZE, height=SIZE, isComponent="true"))
    needle(add(root, "Artboard", name="Needle and pivot", id="3:1", width=WIDTH, height=HEIGHT,
               isComponent="true", defaultStateMachineId="3:2"))
    add(root, "ComponentAsset", name="Meter face", artboardId="2:1")
    add(root, "ComponentAsset", name="Needle and pivot", artboardId="3:1")
    ET.indent(root, space="  ")
    (PROJECT/"scene.rml").write_text(ET.tostring(root, encoding="unicode")+"\n")


if __name__ == "__main__":
    main()
