"""Shared XML primitives for the circular Rive scenes."""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def add(parent, tag, **attrs):
    return ET.SubElement(parent, tag, {
        k: f"{v:.4f}".rstrip("0").rstrip(".") if isinstance(v, float) else str(v)
        for k, v in attrs.items()
    })


def fill(shape, color, rule="evenOdd"):
    add(add(shape, "Fill", fillRule=rule), "SolidColor", colorValue=color)


def circle(parent, name, x, y, radius):
    shape = add(parent, "Shape", name=name, x=x, y=y)
    add(shape, "Ellipse", width=2*radius, height=2*radius)
    return shape


def gradient(paint, kind, start, end, stops):
    g = add(paint, kind, startX=start[0], startY=start[1], endX=end[0], endY=end[1])
    for position, color in stops:
        add(g, "GradientStop", position=position, colorValue=color)


def stroke(shape, width, color):
    paint = add(shape, "Stroke", thickness=width, cap="round", join="round")
    add(paint, "SolidColor", colorValue=color)


def line(shape, points):
    path = add(shape, "PointsPath", isClosed="false")
    for x, y in points:
        add(path, "StraightVertex", x=x, y=y)


def polygon(shape, points):
    path = add(shape, "PointsPath", isClosed="true")
    for x, y in points:
        add(path, "StraightVertex", x=x, y=y)


def animate(timeline, node, **properties):
    obj = add(timeline, "KeyedObject", objectId=node.get("id"))
    for name, keys in properties.items():
        prop = add(obj, "KeyedProperty", property=name)
        for frame, value in keys:
            add(prop, "KeyFrameDouble", frame=frame, value=value, interpolationType="linear")
