

import struct
import json
import array
from hitbox import *

assert array.array('H').itemsize == 2

MAGIC_NUMBER = b"PlatElevel"

# Header data
#  length of name
#  4 floats for boundaries
#  number of tilemaps
#  number of static objects
#  number of entity spawn points
#  number of areas
#  number of edge triggers
#  total number of tiles in all tilemaps
#  total number of colliders
#  total number of hitboxes
Header = struct.Struct("<I4f5I3I")

# Tilemap struct
#  Length of filename of tileset
#  width x height of tilemap
#  z_order
#  top-left offset
#  scale
#  parallax
#  solidness of tile data
Tilemap = struct.Struct("<I2Ii2f2f2f?")

# SceneObject struct
#  length of filename of texture
#  SDL_Rect clip;
#  Vector2 position;
#  int z_order;
#  float rotation;
#  Vector2 scale;
#  CollisionData collision;
SceneObject = struct.Struct("<I4I2f2fif2fI")

# EntitySpawnPoint struct
#  location of spawn
#  length of name of entity class
EntitySpawnPoint = struct.Struct("<2fI")

# LevelArea
#  AABB of boundaries
#  priority
#  r, g, b color values
LevelArea = struct.Struct("<4fi3B")

# EdgeTrigger
#  side enum
#  position
#  size
#  strictness
EdgeTrigger = struct.Struct("<cfff")
EdgeTriggerSides = {
    "top": b't',
    "bottom": b'b',
    "left": b'l',
    "right": b'r'
}

def str2rgb(s):
    if s[0] == '#':
        s = s[1:]
    return int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)

def bake(infile, outfile):
    level = None
    with open(infile, 'r') as f:
        try:
            level = json.load(f)
        except Exception as e:
            raise Exception("Error in parsing '{}': {}".format(infile, str(e)))
        
    with open(outfile, "wb") as f:
        f.write(MAGIC_NUMBER)
        
        name = level["name"].encode()
        
        n_vertices = 0
        n_hitboxes = 0
        n_tiles = 0
        for tilemap in level["tilemaps"]:
            n_tiles += len(tilemap["tiles"][0]) * len(tilemap["tiles"])
        
        for obj in level["objects"]:
            for collider in obj["collision"]:
                h, v = count_hitboxes_and_vertices(collider["hitbox"])
                n_vertices += v
                n_hitboxes += h
        
        f.write(Header.pack(
            len(name),
            level["boundary"]["left"],
            level["boundary"]["right"],
            level["boundary"]["top"],
            level["boundary"]["bottom"],
            len(level["tilemaps"]),
            len(level["objects"]),
            len(level["entities"]),
            len(level["areas"]),
            len(level["edge_triggers"]),
            n_vertices,
            n_hitboxes,
            n_tiles
        ))
        
        f.write(name)
        
        for tilemap in level["tilemaps"]:
            tileset = tilemap["tileset"].encode()
            
            width = len(tilemap["tiles"][0])
            f.write(Tilemap.pack(
                len(tileset),
                width,
                len(tilemap["tiles"]),
                tilemap["z_order"],
                tilemap["offset"]["x"],
                tilemap["offset"]["y"],
                tilemap["scale"]["x"],
                tilemap["scale"]["y"],
                tilemap["parallax"]["x"],
                tilemap["parallax"]["y"],
                tilemap.get("solid")
            ))
            
            f.write(tileset)
            
            for row in tilemap["tiles"]:
                if len(row) != width:
                    raise Exception("Inconsistent tilemap width")
                array.array("H", row).tofile(f)
            
        for obj in level["objects"]:
            texture = obj["texture"].encode()
            
            f.write(SceneObject.pack(
                len(texture),
                obj["clip"]["x"],
                obj["clip"]["y"],
                obj["clip"]["w"],
                obj["clip"]["h"],
                obj["display"]["x"],
                obj["display"]["y"],
                obj["position"]["x"],
                obj["position"]["y"],
                obj["z_order"],
                obj.get("rotation", 0),
                obj.get("scale", {}).get("x", 1),
                obj.get("scale", {}).get("y", 1),
                len(obj["collision"])
            ))
            
            f.write(texture)
            
            for collider in obj["collision"]:
                write_collider(f, collider)
                
        for ent in level["entities"]:
            entityclass = ent["class"].encode()
            
            f.write(EntitySpawnPoint.pack(
                ent["location"]["x"],
                ent["location"]["y"],
                len(entityclass)
            ))
            
            f.write(entityclass)
            
        for area in level["areas"]:
            f.write(LevelArea.pack(
                area["boundary"]["left"],
                area["boundary"]["right"],
                area["boundary"]["top"],
                area["boundary"]["bottom"],
                area["priority"],
                str2rgb(area["ui_color"])
            ))
            
        for edgetrig in level["edge_triggers"]:
            f.write(EdgeTrigger.pack(
                EdgeTriggerSides[edgetrig["side"].lower()],
                edgetrig["position"],
                edgetrig["size"],
                edgetrig.get("strictness", 0)
            ))
            
            
            
            
            
            