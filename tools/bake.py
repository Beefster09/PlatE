#!/usr/bin/python3

import bakesprite, bakelevel, baketileset
import sys
import re
import traceback

def main(*files):
    print ("Baking...")
    for infn in files:
        try:
            if infn.endswith(".sprite.json"):
                outfn = infn[:-5]
                bakesprite.bake(infn, outfn)
                print ("Baked sprite:", infn, "-->", outfn)
            elif infn.endswith(".level.json"):
                outfn = infn[:-5]
                bakelevel.bake(infn, outfn)
                print ("Baked level:", infn, "-->", outfn)
            elif infn.endswith(".tileset.json"):
                outfn = infn[:-5]
                baketileset.bake(infn, outfn)
                print ("Baked tileset:", infn, "-->", outfn)
            else:
                print ("No bake defined for", infn)
        except Exception as e:
            print ("Baking failed:", str(e))
            #traceback.print_exc()

if __name__ == "__main__":
    main(*sys.argv[1:])
