#!/usr/bin/python3

import bakesprite
import sys
import re

def main(*files):
    for infn in files:
        if infn.endswith(".sprite.json"):
            outfn = infn[:-5]
            bakesprite.bake(infn, outfn)

if __name__ == "__main__":
    main(*sys.argv)