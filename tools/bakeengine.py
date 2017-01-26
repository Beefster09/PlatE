import struct
import json
import re
import itertools

from util import *

MAGIC_NUMBER = b"PlatEboot"

Color = struct.Struct("<3B")

NameSpec = re.compile(r'[A-Za-z_]\w*$')

CollisionTypeIntrinsics = []

def write_short(f, i):
    f.write(int(i).to_bytes(2, "little"))

def write_str(f, s):
    enc = s.encode()
    write_short(f, len(enc))
    f.write(enc)

def write_str_list(f, l):
    write_short(f, len(l))
    for s in l:
        write_str(f, s)

def index_pred(lst, pred):
    for i, x in enumerate(lst):
        if pred(x):
            return i
    raise ValueError("Predicate failed for list.")

def check_names(*lst, itemtype='item'):
    if not all(map(NameSpec.match, lst)):
        raise Exception("Illegal name for {}: {}".format(
            itemtype,
            next(itertools.dropwhile(NameSpec.match, lst)))
        )

def bake(infn, outfn):
    engine = None
    with open(infn, 'r') as f:
        try:
            engine = json.load(f)
        except Exception as e:
            raise Exception("Error in parsing '{}': {}".format(infile, str(e)))

    typenames = CollisionTypeIntrinsics + [t['name'] for t in engine['collision']['types']]
    # check for uniqueness
    unique = set(typenames)
    if len(unique) != len(typenames):
        dupes = set()
        for tname in typenames:
            if tname in unique:
                unique.remove(tname)
            else:
                dupes.add(tname)
        raise Exception("Duplicate Collision Type name(s): {}".format(dupes))

    with open(outfn, "wb") as f:
        f.write(MAGIC_NUMBER)

        write_str(f, engine['title'])
        write_str(f, engine.get('asset_dir', ''))

        write_short(f, len(engine['controller_types']))
        for cont in engine['controller_types']:
            check_names(cont['name'], itemtype="controller type")
            check_names(*cont.get('axes', []), itemtype="axis")
            check_names(*cont.get('buttons', []), itemtype="button")

            write_str(f, cont['name'])
            write_str_list(f, cont.get('axes', []))
            write_str_list(f, cont.get('buttons', []))

        collision = engine['collision']

        write_short(f, len(collision['types']))
        for t in collision['types']:
            check_names(t['name'], itemtype="collider type")

            write_str(f, t['name'])
            f.write(Color.pack(*str2rgb(t['color'])))
            relations = t.get('acts_on', [])
            write_short(f, len(relations))
            for rel in relations:
                try:
                    write_short(f, typenames.index(rel))
                except ValueError as e:
                    raise Exception("acts_on list referencing name not in the collision type list: " + rel)

        check_names(*collision.get('channels', []), itemtype="collider channel")
        write_str_list(f, collision.get('channels', []))
