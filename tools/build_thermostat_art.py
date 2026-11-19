"""Author the round climate dial, including its setpoint states and twilight landscape."""
import math
import re
import xml.etree.ElementTree as ET

from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

from outline_text import FONT, RivePath
from rive_art import ROOT, add, circle, fill, gradient, line, polygon, stroke

PROJECT = ROOT / "assets/thermostat"
HEADER = (ROOT / "examples/thermostat/main/thermostat.h").read_text()
LOW, HIGH, INITIAL, ARC, ROOM, HUMIDITY = (
    int(re.search(rf"{name} = (\d+)", HEADER)[1]) for name in
    ("THERMOSTAT_LOW", "THERMOSTAT_HIGH", "THERMOSTAT_INITIAL", "THERMOSTAT_HALF_ARC", "DEMO_ROOM_F", "DEMO_HUMIDITY"))
C = 233
WHITE = "FFF4F7F6"
SECONDARY = "FFC2D1D7"
# Each material has a cool-night and a warm-evening finish.
MATERIALS = {
    "Sky zenith": ("0C1D39", "252039"), "Alpine glow": ("274867", "77505C"),
    "Sky horizon": ("598DA1", "C78067"), "Distant summit": ("31566C", "80565A"),
    "Dusk ridge": ("244659", "62474E"), "Valley mist": ("335A68", "795658"),
    "Spruce hills": ("1B3748", "483942"), "Pine shadows": ("112A37", "332D36"),
    "Forest floor": ("0A202B", "25252D"), "Dial accent": ("A8E6F0", "FFD5AF"),
}


def mix(first, second, t):
    a, b = bytes.fromhex(first), bytes.fromhex(second)
    return "FF" + bytes(round(x+(y-x)*t) for x, y in zip(a,b)).hex().upper()


def point(a, radius):
    a = math.radians(a)
    return C+radius*math.sin(a), C-radius*math.cos(a)


def caption(parent, font, text, cx, baseline, size, color, tracking=0):
    glyphs = font.getGlyphSet()
    glyph_list = [glyphs[font.getBestCmap()[ord(ch)]] for ch in text]
    scale = size/font["head"].unitsPerEm
    width = sum(g.width*scale for g in glyph_list) + (len(text)-1)*tracking
    x = cx-width/2
    shape = add(parent, "Shape", name=text)
    for glyph in glyph_list:
        glyph.draw(TransformPen(RivePath(shape, glyphs), (scale,0,0,-scale,x,baseline)))
        x += glyph.width*scale+tracking
    fill(shape, color, rule="nonZero")
    return shape


def circular_polygon(shape, points):
    # Clip the silhouette at authoring time. A shared live mask makes ThorVG
    # allocate a full-screen clip for every foreground shape on the ESP32.
    for i in range(192):
        angle = math.tau*i/192
        nx, ny = math.cos(angle), math.sin(angle)
        cut = []
        a = points[-1]
        da = (a[0]-C)*nx+(a[1]-C)*ny-C
        for b in points:
            db = (b[0]-C)*nx+(b[1]-C)*ny-C
            if (da > 0) != (db > 0):
                t = da/(da-db)
                cut.append((a[0]+t*(b[0]-a[0]), a[1]+t*(b[1]-a[1])))
            if db <= 0:
                cut.append(b)
            a, da = b, db
        if not cut:
            return
        points = cut
    polygon(shape, points)


def landscape(parent, materials):
    def paint(obj, name):
        color = add(add(obj, "Fill", fillRule="nonZero"), "SolidColor")
        assign(color, name)

    def assign(prop, name):
        prop.set("id", f"12:{list(MATERIALS).index(name)+1}")
        prop.set("colorValue", mix(*MATERIALS[name], (INITIAL-LOW)/(HIGH-LOW)))
        materials.append((prop.get("id"), name))

    def ridge(name, points):
        shape = add(parent, "Shape", name=name)
        circular_polygon(shape, points + [(480,470),(-14,470)])
        paint(shape, name)
        return shape

    # RML lists the foreground first. Each silhouette bleeds into a circular clip.
    ridge("Forest floor", [(-14,329),(30,346),(75,358),(111,365),(158,375),(193,375),
                           (229,381),(264,379),(313,367),(341,362),(388,340),(437,331),(480,318)])
    trees = add(parent, "Shape", name="Pine shadows")
    for x, base, h, w in [(-1,353,69,15),(12,348,48,12),(27,350,70,15),(39,353,43,10),
                           (54,358,57,13),(67,359,31,8),(79,364,40,9),(97,365,27,7),
                           (374,350,27,7),(389,352,39,9),(403,351,52,12),(417,346,36,8),
                           (431,339,58,13),(445,335,71,16),(459,336,49,12),(475,334,75,16)]:
        circular_polygon(trees, [(x,base-h),(x+w*.48,base-h*.61),(x+w*.24,base-h*.64),
                        (x+w*.77,base-h*.35),(x+w*.43,base-h*.39),(x+w,base-h*.1),
                        (x+w*.13,base-h*.16),(x+w*.13,base),(x-w*.13,base),
                        (x-w*.13,base-h*.16),(x-w,base-h*.1),(x-w*.43,base-h*.39),
                        (x-w*.77,base-h*.35),(x-w*.24,base-h*.64),(x-w*.48,base-h*.61)])
    paint(trees, "Pine shadows")
    ridge("Spruce hills", [(-14,283),(20,292),(54,315),(94,324),(132,341),(171,346),(213,359),
                           (258,357),(296,350),(334,337),(379,322),(413,303),(454,295),(480,280)])
    ridge("Valley mist", [(-14,273),(34,279),(86,297),(139,310),(188,337),(235,340),(286,328),
                          (337,315),(377,301),(420,288),(480,275)])
    ridge("Dusk ridge", [(-14,245),(25,251),(53,237),(83,246),(114,268),(131,265),(176,293),
                         (216,312),(257,302),(291,284),(316,282),(348,256),(377,248),
                         (404,230),(438,251),(465,238),(480,246)])
    ridge("Distant summit", [(-14,214),(16,227),(53,215),(76,224),(103,205),(123,221),
                             (143,217),(188,251),(220,266),(259,251),(291,246),(325,211),
                             (342,219),(365,196),(392,218),(421,212),(449,228),(480,209)])
    sky = circle(parent, "Twilight sky", C,C,C)
    ramp = add(add(sky,"Fill"), "LinearGradient", startX=-42,startY=-186,endX=18,endY=143)
    for pos, name in [(0,"Sky zenith"),(.57,"Alpine glow"),(1,"Sky horizon")]:
        assign(add(ramp, "GradientStop", position=pos), name)


def spectral_ring(parent):
    colors = ["538FFF", "68C6F7", "87D9DA", "D5DFC9", "F4CB9D", "FF9C77", "FA736F"]
    tick_count = (HIGH-LOW)*3
    for i in range(tick_count+1):
        pos = i/tick_count
        scaled = pos*(len(colors)-1)
        index = min(int(scaled), len(colors)-2)
        shape = add(parent, "Shape", name=f"Graduation {i}")
        theta = -ARC+2*ARC*pos
        line(shape, [point(theta,199 if i%3 == 0 else 204),point(theta,219)])
        stroke(shape, 5.8 if i%3 == 0 else 4.7, mix(colors[index], colors[index+1], scaled-index))
    track = add(parent, "Shape", name="Recessed track")
    line(track, [point(-ARC+2*ARC*i/120,210) for i in range(121)])
    stroke(track, 30, "6E091322")


def keyed(timeline, obj, prop, kind, value):
    target = add(add(timeline, "KeyedObject", objectId=obj), "KeyedProperty", property=prop)
    add(target, kind, value=value, interpolationType="hold")


def main():
    root = ET.Element("Rive", version="1", kind="fragment")
    vm = add(root, "ViewModel", name="Thermostat", id="1:1", defaultInstanceId="1:3")
    add(vm, "ViewModelPropertyNumber", name="setpoint", id="1:2")
    for name, ident in [("surfaceOpacity","1:4"),("controlsOpacity","1:5")]:
        add(vm,"ViewModelPropertyNumber",name=name,id=ident)
    inst = add(vm, "ViewModelInstance", name="Default", id="1:3", exports="true")
    add(inst, "ViewModelInstanceNumber", viewModelPropertyId="1:2", propertyValue=INITIAL)
    for ident in ("1:4", "1:5"):
        add(inst,"ViewModelInstanceNumber",viewModelPropertyId=ident,propertyValue=1)
    board = add(root,"Artboard", name="Thermostat", width=466,height=466, id="2:1",
                defaultStateMachineId="2:2", viewModelId="1:1", viewModelInstanceId="1:3")
    panel = add(board,"Node",name="Climate dial")
    controls = add(panel,"Node",name="Moving controls")
    add(controls,"DataBindContext",sourcePathIds="1:1-1:5",propertyKey=18)
    surface = add(panel,"Node",name="Dial surface")
    add(surface,"DataBindContext",sourcePathIds="1:1-1:4",propertyKey=18)
    marker = add(controls,"Node",name="Setpoint index",id="2:4",x=C,y=C)
    for name, width, col in [("Porcelain index",7.5,WHITE),("Index shadow",13,"FF26313C")]:
        s = add(marker,"Shape",name=name)
        line(s, [(0,-225),(0,-193)])
        stroke(s,width,col)
    nose = add(marker,"Shape",name="Index tip")
    polygon(nose, [(-5,-193),(0,-185),(5,-193)])
    fill(nose,WHITE)
    materials = []
    with TTFont(FONT) as regular, TTFont(FONT.with_name("Inter_28pt-Bold.ttf")) as bold:
        for group, x, digit in [(3,175,INITIAL//10), (4,263,INITIAL%10)]:
            solo = add(controls,"Solo",name="Tens" if group == 3 else "Units",id=f"{group}:1",
                       activeComponentId=f"{group}:{digit+2}")
            digits = range(LOW//10,HIGH//10+1) if group == 3 else range(10)
            for n in digits:
                node = add(solo, "Node", name=str(n), id=f"{group}:{n+2}")
                caption(node,regular,str(n),x,237,156,WHITE)
        cap = caption(surface,bold,"°F",333,154,29,"FFD3E7E8")
        color = cap.find("Fill/SolidColor")
        color.set("id", "12:10")
        materials.append(("12:10", "Dial accent"))
        divider = add(surface,"Shape",name="Reading divider")
        line(divider, [(233,278),(233,326)])
        stroke(divider,1.5,"768DA7AE")
        for label, value, x in [("ROOM",f"{ROOM}°",167),("HUMIDITY",f"{HUMIDITY}%",299)]:
            caption(surface,bold,label,x,287,18,SECONDARY,1.55)
            caption(surface,regular,value,x,327,39,WHITE)
        modes = add(controls,"Solo",name="Operating mode",id="5:1",activeComponentId="5:4")
        for i, (name, col) in enumerate([("Cooling","FFA6DDF7"),("Holding","FFBBDCCF"),("Heating","FFFFC6A2")]):
            node = add(modes,"Node",name=name,id=f"5:{2+i}")
            caption(node,bold,name.upper(),C+11,391,18,col,2)
            icon = add(node,"Shape",name=name+" symbol",x=C-56,y=384)
            if i == 0:
                for a in [0, math.pi/3, 2*math.pi/3]:
                    dx,dy=8*math.cos(a),8*math.sin(a)
                    line(icon,[(-dx,-dy),(dx,dy)])
                stroke(icon,2.6,col)
            elif i == 1:
                line(icon,[(-6,-3),(6,-3)])
                line(icon,[(-6,3),(6,3)])
                stroke(icon,2.6,col)
            else:
                polygon(icon,[(0,-10),(6,-2),(7,2),(5,6),(1,8),(-4,7),(-7,3),(-6,-1),(-3,-5),(-2,0)])
                fill(icon,col)
        for n,a in [(LOW,-ARC),(HIGH,ARC)]:
            x,y=point(a,211)
            caption(surface,regular,str(n),x+(18 if n==LOW else -18),y+21,21,SECONDARY)
    spectral_ring(surface)
    # A quiet central vignette gives the small white readings contrast over the peaks.
    shade = circle(surface,"Dial glass",C,C,C)
    gradient(add(shade,"Fill"),"RadialGradient",(0,4),(201,83),
             [(0,"99202B34"),(.62,"75152430"),(1,"0012202B")])
    landscape(surface,materials)

    machine=add(board,"StateMachine",name="Climate",id="2:2")
    layer=add(machine,"StateMachineLayer",name="Setpoint")
    any_state=add(layer,"AnyState")
    entry=add(layer,"EntryState")
    add(entry,"StateTransition",stateToId=f"6:{INITIAL}")
    add(layer,"ExitState")
    for temp in range(LOW,HIGH+1):
        transition=add(any_state,"StateTransition",stateToId=f"6:{temp}")
        cond=add(transition,"TransitionViewModelCondition",opValue="equal")
        source=add(add(cond,"TransitionPropertyViewModelComparator"),"BindablePropertyNumber")
        add(source,"DataBindContext",sourcePathIds="1:1-1:2",propertyKey=636)
        add(cond,"TransitionValueNumberComparator",value=temp)
        add(layer,"AnimationState",id=f"6:{temp}",animationId=f"7:{temp}")
        timeline=add(board,"LinearAnimation",name=f"{temp} Fahrenheit",id=f"7:{temp}",duration=1)
        warmth=(temp-LOW)/(HIGH-LOW)
        keyed(timeline,"3:1","activeComponentId","KeyFrameId",f"3:{2+temp//10}")
        keyed(timeline,"4:1","activeComponentId","KeyFrameId",f"4:{2+temp%10}")
        mode=2 if temp<ROOM else 4 if temp>ROOM else 3
        keyed(timeline,"5:1","activeComponentId","KeyFrameId",f"5:{mode}")
        keyed(timeline,"2:4","rotation","KeyFrameDouble",math.radians(-ARC+2*ARC*warmth))
        for target,name in materials:
            keyed(timeline,target,"colorValue","KeyFrameColor",mix(*MATERIALS[name],warmth))
    ET.indent(root,space="  ")
    (PROJECT/"scene.rml").write_text(ET.tostring(root,encoding="unicode")+"\n")


if __name__ == "__main__":
    main()
