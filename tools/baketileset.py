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
Header = struct.Struct("<2I2H2I")

# Information about a tile
#  Number of animation frames
#  Number of properties
#    (this will expand after introducing solidity)
Tile = struct.Struct("<2I")

# Information about a frame within a tile's animation
#  Tile x index
#  Tile y index
#  Frame duration
TileFrame = struct.Struct("<2Hf")

def bake(infile, outfile):
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
        for tile in tileset["tiles"]:
            n_tileframes += len(tile['frames'])

        f.write(Header.pack(
            len(name),
            len(texture),
            tileset["tile_width"],
            tileset["tile_height"],
            len(tileset["tilesheet"]),
            n_tileframes
        ))

        f.write(name)
        f.write(texture)

        for tile in tileset["tiles"]:
            f.write(Tile.pack(
                len(tile["frames"]),
                len(tile.get("properties", []))
            ))

            for frame in tile['frames']:
                f.write(TileFrame.pack(
                    frame["x"],
                    frame["y"],
                    frame.get("duration", 1)
                ))
