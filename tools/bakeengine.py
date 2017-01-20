import struct
import json
from util import *

MAGIC_NUMBER = b"PlatEboot"

def bake(infn, outfn):
    engine = None
    with open(infn, 'r') as f:
        try:
            engine = json.load(f)
        except Exception as e:
            raise Exception("Error in parsing '{}': {}".format(infile, str(e)))

    with open(outfn, "wb") as f:
        f.write(MAGIC_NUMBER)
