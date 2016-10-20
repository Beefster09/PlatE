
import struct
import json
from hitbox import *
from util import *

MAGIC_NUMBER = b"PlatEsprite"

# Basic data to the sprite
#  length of the name
#  length of the filename of the texture
#  number of cliprects
#  number of frame data entries
#  number of animations
#  total number of offsets
#  total number of colliders
#  number of nested hitboxes
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

# Animation
#  length of the name of the animation
#  number of frames in the animation
Animation = struct.Struct("<II")

# Animation Frame
#  duration to display
#  index of the frame
AnimFrame = struct.Struct("<fI")


def bake(infile, outfile):
    sprite = None
    with open(infile, 'r') as f:
        try:
            sprite = json.load(f)
        except Exception as e:
            raise Exception("Error in parsing '{}': {}".format(infile, str(e)))
        
    with open(outfile, "wb") as f:
        f.write(MAGIC_NUMBER)
        
        name = sprite["name"].encode()
        texture = sprite["spritesheet"].encode()
            
        n_stringbytes = alignstrlen(name)
        n_offsets = 0
        n_colliders = 0
        n_vertices = 0
        nested_hitboxes = 0
        for frame in sprite["frames"]:
            n_offsets += len(frame["offsets"])
            n_colliders += len(frame["collision"])
            for collider in frame["collision"]:
                h, v = count_nested_hitboxes_and_vertices(collider["hitbox"])
                nested_hitboxes += h
                n_vertices += v
            
        n_frametimings = 0
        for anim in sprite["animations"]:
            n_stringbytes += alignstrlen(anim["name"])
            n_frametimings += len(anim["frames"])
        
        f.write(Header.pack(
            len(name),
            len(texture),
            len(sprite["clips"]),
            len(sprite["frames"]),
            len(sprite["animations"]),
            n_offsets,
            n_colliders,
            nested_hitboxes,
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
                
            for collider in frame["collision"]:
                write_collider(f, collider)
                
                
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
 




 