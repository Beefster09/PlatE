
import struct
import json

MAGIC_NUMBER = b"PlatEsprite"

# Basic data to the sprite
#  length of the name
#  length of the filename of the texture
#  number of cliprects
#  number of frame data entries
#  number of animations
#  total number of offsets
#  total number of colliders
#  total number of hitboxes
#  total number of polygon vertices
#  total number of frame timings
#  total number of bytes needed for strings
Header = struct.Struct("<11I")

# x, y, w, h
ClipRect = struct.Struct("<4I")

# Frame Data
#  index of the clip
#  display offset
#  foot offset
#  number of additional offsets
#  number of colliders
Frame = struct.Struct("<I2f2fII")
Offset = struct.Struct("<2f")

# Collider data without the Hitbox (due to hitboxes being variable length)
#  length of the name of the collider type
#  solid
#  ccd
ColliderNoHitbox = struct.Struct("<I??")

# Stuff for hitboxes
HitboxBox = b'b'
HitboxLine = b'l'
HitboxOneway = b'o'
HitboxCircle = b'c'
HitboxPolygon = b'p'
HitboxComposite = b'?'

# These are padded to make it easier to predetermine how much space to allocate
# by ensuring that the binary is at least as large as the final representation
#  Currently, the size of the Hitbox struct is 33 bytes
Box = struct.Struct("<4f")
Line = struct.Struct("<4f")
Circle = struct.Struct("<3f")
HitboxArrayLen = struct.Struct("<I")

Vertex = struct.Struct("<2f")

# Animation
#  length of the name of the animation
#  number of frames in the animation
Animation = struct.Struct("<II")

# Animation Frame
#  duration to display
#  index of the frame
AnimFrame = struct.Struct("<fI")

def bake(infile, outfile):
    sprite = None;
    with open(infile, 'r') as f:
        sprite = json.load(f)
        
    with open(outfile, "wb") as f:
        f.write(MAGIC_NUMBER)
        
        name = sprite["name"].encode()
        texture = sprite["spritesheet"].encode()
            
        def count_hitboxes_and_vertices(hitbox):
            if hitbox["type"] == "polygon":
                return 1, len(hitbox["vertices"])
            elif hitbox["type"] == "composite":
                hitboxes = 1
                vertices = 0
                for sub in hitbox["hitboxes"]:
                    h, v = count_hitboxes_and_vertices(sub)
                    hitboxes += h
                    vertices += v
                return hitboxes, vertices
            else:
                return 1, 0
            
        n_stringbytes = len(name) + 1
        n_offsets = 0
        n_colliders = 0
        n_vertices = 0
        n_hitboxes = 0
        for frame in sprite["frames"]:
            n_offsets += len(frame["offsets"])
            n_colliders += len(frame["collision"])
            for collider in frame["collision"]:
                h, v = count_hitboxes_and_vertices(collider["hitbox"])
                n_hitboxes += h
                n_vertices += v
            
        n_frametimings = 0
        for anim in sprite["animations"]:
            n_stringbytes += len(anim["name"].encode()) + 1
            n_frametimings += len(anim["frames"])
        
        f.write(Header.pack(
            len(name),
            len(texture),
            len(sprite["clips"]),
            len(sprite["frames"]),
            len(sprite["animations"]),
            n_offsets,
            n_colliders,
            n_hitboxes,
            n_vertices,
            n_frametimings,
            n_stringbytes
        ))
        
        f.write(name)
        f.write(texture)
        
        for clip in sprite["clips"]:
            f.write(ClipRect.pack(
                clip["x"],
                clip["y"],
                clip["w"],
                clip["h"]
            ))
            
        for frame in sprite["frames"]:
            f.write(Frame.pack(
                frame["clip"],
                frame["display"]["x"],
                frame["display"]["y"],
                frame["foot"]["x"],
                frame["foot"]["y"],
                len(frame["offsets"]),
                len(frame["collision"])
            ))
            
            for offset in frame["offsets"]:
                f.write(Offset.pack(
                    offset["x"],
                    offset["y"]
                ))
                
            def write_hitbox(hitbox):
                type = hitbox["type"]
                if type == "box":
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
                    f.write(Line.pack(
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
                        write_hitbox(sub)
                
            for collider in frame["collision"]:
                ctype = collider["type"].encode()
            
                f.write(ColliderNoHitbox.pack(
                    len(ctype),
                    collider.get("solid"),
                    collider.get("ccd")
                ))
                
                f.write(ctype)
                write_hitbox(collider["hitbox"])
                
                
        for anim in sprite["animations"]:
            name = anim["name"].encode()
            
            f.write(Animation.pack(
                len(name),
                len(anim["frames"])
            ))
            f.write(name)
            
            for frame in anim["frames"]:
                f.write(AnimFrame.pack(
                    frame["duration"],
                    frame["frame"]
                ))
 




 