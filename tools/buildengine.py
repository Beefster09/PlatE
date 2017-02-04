import struct
import json
import re
import itertools
from operator import itemgetter

from util import *

MAGIC_NUMBER = b"PlatEboot"

Color = struct.Struct("<3B")

NameSpec = re.compile(r'[A-Za-z_]\w*$')

# TODO: find some way to keep these synced with the C++
CollisionTypeBuiltins = []
CollisionChannelBuiltins = [
    "EntityDefault",
	"TilemapDefault"
]

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

def index_if(lst, pred):
    for i, x in enumerate(lst):
        if pred(x):
            return i
    raise ValueError("No item in list matching the predicate")

def find_if(lst, pred):
    try:
        return next(filter(pred, lst))
    except StopIteration:
        raise ValueError("No item in list matching the predicate")

def check_names(*lst, itemtype, context=None):
    invalid_names = list(itertools.filterfalse(NameSpec.match, lst))
    if len(invalid_names) > 0:
        raise ValueError("Illegal name{} for {}: {}".format(
            '' if len(invalid_names) == 1 else 's',
            itemtype,
            ', '.join(invalid_names)
        ))

def check_duplicates(lst, *_dummy_, itemtype, context=None):
    if not isinstance(lst, list):
        lst = list(lst)
    unique = set(lst)
    if len(unique) != len(lst):
        dupes = set()
        for item in lst:
            if item in unique:
                unique.remove(item)
            else:
                dupes.add(item)
        raise ValueError("Duplicate {type} name{s}: {dupes}{context}".format(
            type=itemtype,
            s=('' if len(dupes) == 1 else 's'),
            dupes=', '.join(sorted(dupes)),
            context=' (' + context + ')' if context is not None else ''
        ))

def input_spec(inp):
    if isinstance(inp, list):
        return ','.join(map(input_spec, inp))
    elif isinstance(inp, dict):
        return '???' #todo/later: for gamepads
    else:
        return inp

def build(infn, outfn):
    engine = None
    with open(infn, 'r') as f:
        try:
            engine = json.load(f)
        except Exception as e:
            raise Exception("Error in parsing '{}': {}".format(infile, str(e))) from e

    typenames = CollisionTypeBuiltins + [t['name'] for t in engine['collision']['types']]

    check_duplicates(typenames, itemtype="collision type")
    check_duplicates(engine['collision']['channels'], itemtype="collision channels")
    check_duplicates(map(itemgetter('name'), engine['controller_types']), itemtype="controller type")
    check_duplicates(map(itemgetter('name'), engine['controllers']), itemtype="controller instance")

    with open(outfn, "wb") as f:
        f.write(MAGIC_NUMBER)

        write_str(f, engine['title'])
        write_short(f, engine['screen']['width'])
        write_short(f, engine['screen']['height'])

        write_str(f, engine.get('asset_dir', ''))
        write_str(f, engine.get('script_dir', ''))

        cont_types = engine['controller_types']
        write_short(f, len(cont_types))
        for cont in cont_types:
            axes = cont.get('axes', [])
            buttons = cont.get('buttons', [])

            check_names(cont['name'], itemtype="controller type")
            check_names(*axes, itemtype="axis")
            check_names(*buttons, itemtype="button")
            check_duplicates(axes + buttons, itemtype='input',
                             context="controller type '{}'".format(cont['name']))

            write_str(f, cont['name'])
            write_str_list(f, axes)
            write_str_list(f, buttons)

        write_short(f, len(engine['controllers']))
        for cont in engine['controllers']:
            name = cont['name']
            check_names(name, itemtype="controller instance")
            try:
                ctype = find_if(engine['controller_types'], lambda x: x['name'] == cont['type'])
            except:
                raise NameError("Controller instance '{}' referencing undefined controller type '{}'".format(name, cont['type']))

            write_str(f, name)
            write_str(f, cont['type'])

            defaults = cont['defaults']

            for axis in ctype['axes']:
                if axis + '+' not in defaults and axis + '-' not in defaults:
                    raise KeyError("Missing default for axis '{}' in controller '{}'".format(
                        axis, name
                    ))
                write_str(f, input_spec(defaults.get(axis + '+', '')))
                write_str(f, input_spec(defaults.get(axis + '-', '')))

            for button in ctype['buttons']:
                if button not in defaults:
                    raise KeyError("Missing default for button '{}' in controller '{}'".format(
                        button, name
                    ))
                write_str(f, input_spec(defaults[button]))


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
                    raise NameError("acts_on list referencing name not in the collision type list: " + rel)

        check_names(*collision.get('channels', []), itemtype="collider channel")
        write_str_list(f, collision.get('channels', []))
