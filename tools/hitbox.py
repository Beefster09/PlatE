
import struct

# Collider data without the Hitbox (due to hitboxes being variable length)
#  length of the name of the collider type
ColliderNoHitbox = struct.Struct("<I")

# Stuff for hitboxes
HitboxNone      = b'\0'
HitboxBox       = b'b'
HitboxLine      = b'l'
HitboxOneway    = b'o'
HitboxCircle    = b'c'
HitboxPolygon   = b'p'
HitboxComposite = b'?'

# These are padded to make it easier to predetermine how much space to allocate
# by ensuring that the binary is at least as large as the final representation
#  Currently, the size of the Hitbox struct is 33 bytes
Box = struct.Struct("<4f")
Line = struct.Struct("<4f")
Circle = struct.Struct("<3f")
HitboxArrayLen = struct.Struct("<I")

Vertex = struct.Struct("<2f")

def count_nested_hitboxes_and_vertices(hitbox):
    if hitbox["type"] == "polygon":
        return 1, len(hitbox["vertices"])
    elif hitbox["type"] == "composite":
        hitboxes = len(hitbox["hitboxes"])
        vertices = 0
        for sub in hitbox["hitboxes"]:
            h, v = count_nested_hitboxes_and_vertices(sub)
            hitboxes += h
            vertices += v
        return hitboxes, vertices
    else:
        return 0, 0

def write_hitbox(f, hitbox):
    type = hitbox.get("type", "none").lower()
    
    if type == "none":
        f.write(HitboxNone)
    elif type == "box":
        f.write(HitboxBox)
        f.write(Box.pack(
            hitbox["left"],
            hitbox["right"],
            hitbox["top"],
            hitbox["bottom"]
        ))
    elif type == "line":
        f.write(HitboxLine)
        f.write(Line.pack(
            hitbox["p1"]["x"],
            hitbox["p1"]["y"],
            hitbox["p2"]["x"],
            hitbox["p2"]["y"]
        ))
    elif type == "oneway":
        f.write(HitboxOneway)
        f.write(Line.pack(
            hitbox["p1"]["x"],
            hitbox["p1"]["y"],
            hitbox["p2"]["x"],
            hitbox["p2"]["y"]
        ))
    elif type == "circle":
        f.write(HitboxCircle)
        f.write(Circle.pack(
            hitbox["center"]["x"],
            hitbox["center"]["y"],
            hitbox["radius"]
        ))
    elif type == "polygon":
        f.write(HitboxPolygon)
        f.write(HitboxArrayLen.pack(len(hitbox["vertices"])))
        for vertex in hitbox["vertices"]:
            f.write(Vertex.pack(
                vertex["x"],
                vertex["y"]
            ))
    elif type == "composite":
        f.write(HitboxComposite)
        f.write(HitboxArrayLen.pack(len(hitbox["hitboxes"])))
        for sub in hitbox["hitboxes"]:
            write_hitbox(f, sub)
    else:
        raise Exception("Invalid hitbox type: " + type)

def write_collider(f, collider):
    ctype = collider["type"].encode()

    f.write(ColliderNoHitbox.pack(
        len(ctype)
    ))

    f.write(ctype)
    write_hitbox(f, collider["hitbox"])
