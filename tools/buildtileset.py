import struct
import json
import array
from hitbox import *
from util import *

assert array.array('H').itemsize == 2

MAGIC_NUMBER = b"PlatEtileset"

# Basic data for the tileset
#  Length of the name
#  Length of the filename for the texture
#  Tile width
#  Tile height
#  Number of tiles in the tileset
#  Total Number of tile frames (across all tiles)
#    (this will expand after introducing solidity and fleshing out properties)
#  Total number of hitboxes (complex collision)
#  Total number of vertices (complex collision)
Header = struct.Struct("<2I2H2I2I")

# Information about a tile
#  Number of animation frames
#  Number of properties
Tile = struct.Struct("<2I")

# Information about a frame within a tile's animation
#  Tile x index
#  Tile y index
#  Frame duration
#  Flip mask
TileFrame = struct.Struct("<2HfB")

SolidityNone    = b'\0'
SolidityFull    = b'F'
SolidityPartial = b'P'
SoliditySlope   = b'S'
SolidityComplex = b'C'

Partial = struct.Struct("<f??")
Slope = struct.Struct("<ff?")

def get_flip(frame):
    if "flip" in frame:
        code = frame[flip].upper()
        if code == 'H':
            return 1
        if code == 'V':
            return 2
        if code in ['HV', 'VH', 'BOTH', 'B']:
            return 3
    return 0

def write_solidity(f, solidity):
    type = solidity.get("type", "none").lower()
    if type == "none":
        f.write(SolidityNone)
    elif type == "full":
        f.write(SolidityFull)
    elif type == "partial":
        f.write(SolidityPartial)
        f.write(Partial.pack(
            solidity["position"],
            solidity.get("vertical", False),
            solidity.get("topleft", False)
        ))
    elif type == "slope":
        f.write(SoliditySlope)
        f.write(Slope.pack(
            solidity["position"],
            solidity["slope"],
            solidity.get("above", False)
        ))
    elif type == "complex":
        f.write(SolidityComplex)
        write_hitbox(f, solidity["hitbox"])
    else:
        raise Exception("Invalid tile solidity type: " + type)

def build(infile, outfile):
    level = None
    with open(infile, 'r') as f:
        try:
            tileset = json.load(f)
        except Exception as e:
            raise Exception("Error in parsing '{}': {}".format(infile, str(e)))

    with open(outfile, "wb") as f:
        f.write(MAGIC_NUMBER)

        name = tileset["name"].encode()
        texture = tileset["tilesheet"].encode()

        n_tileframes = 0
        tn_hitboxes = 0
        tn_vertices = 0
        for tile in tileset["tiles"]:
            n_tileframes += len(tile['frames'])
            if tile.get('solidity', {}).get('type', 'none').lower() == 'complex':
                h, v = count_nested_hitboxes_and_vertices(tile['solidity']['hitbox'])
                tn_hitboxes += 1 + h
                tn_vertices += v

        f.write(Header.pack(
            len(name),
            len(texture),
            tileset["tile_width"],
            tileset["tile_height"],
            len(tileset["tiles"]),
            n_tileframes,
            tn_hitboxes,
            tn_vertices
        ))

        f.write(name)
        f.write(texture)

        for tile in tileset["tiles"]:
            f.write(Tile.pack(
                len(tile["frames"]),
                len(tile.get("properties", []))
            ))

            write_solidity(f, tile.get("solidity", {}))

            for frame in tile['frames']:
                f.write(TileFrame.pack(
                    frame["x"],
                    frame["y"],
                    frame.get("duration", 1),
                    get_flip(frame)
                ))
