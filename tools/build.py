#!/usr/bin/python3

import buildsprite, buildlevel, buildtileset, buildengine
import re
import traceback

import os, sys, os.path
import argparse

JSON_ASSET = re.compile(r"(?P<basename>[^.].*)\.(?P<type>[^.]+)\.json")

MSG_DEBUG = 3
MSG_VERYVERBOSE = 2
MSG_VERBOSE = 1
MSG_NORMAL = 0
MSG_WARN = -1
MSG_ERRORS = -2
MSG_SILENT = -3

def needs_bake(src, baked):
    srcstat = os.stat(src)
    try:
        binstat = os.stat(baked)
    except FileNotFoundError:
        return True

    return srcstat.st_mtime > binstat.st_mtime

def mv(src, dst):
    try:
        os.replace(src, dst)
        return True
    except:
        return False

def rm(f):
    try:
        os.remove(f)
        return True
    except:
        return False

def main(indir = "data", outdir = "assets", *args,
    do_backups = True, msglevel = MSG_NORMAL, force = False
):
    if msglevel >= MSG_VERYVERBOSE:
        print ("Building assets within '{}/' into '{}/'".format(indir, outdir))

    def build_one(infn, outfn, t):
        if force or needs_bake(infn, outfn):
            if do_backups:
                mv(outfn, outfn + ".bk")

            try:
                if t == "bootloader":
                    buildengine.bake(infn, outfn)
                elif t == 'sprite':
                    buildsprite.bake(infn, outfn)
                elif t == 'level':
                    buildlevel.bake(infn, outfn)
                elif t == 'tileset':
                    buildtileset.bake(infn, outfn)
                else:
                    raise Exception("Unsupported bake type")

                if msglevel >= MSG_VERBOSE:
                    print ("Built {}: {} --> {}".format(t, infn, outfn))

                if do_backups:
                    rm(outfn + '.bk')
                else:
                    rm(outfn)

                return 1

            except FileNotFoundError as e:
                raise e

            except Exception as e:
                if msglevel >= MSG_ERRORS:
                    print ("Build failed on {}:".format(infn), str(e))
                if msglevel >= MSG_DEBUG:
                    traceback.print_exc()
                if do_backups:
                    mv(outfn + '.bk', outfn)
                return 0

        else:
            if msglevel >= MSG_VERBOSE:
                print ("Nothing to build for", infn)
            return 0
    # end build_one

    n_built = 0
    try:
        n_built += build_one("engine.json", "engine.boot", "bootloader")
    except FileNotFoundError:
        if msglevel >= MSG_WARN:
            print ("WARNING: no engine.json in current directory")

    for root, dirs, files in os.walk(indir, followlinks=True):
        dirs[:] = [d for d in dirs if not d.startswith('.')]  # skip unix-style hidden folders
        for fn in files:
            match = JSON_ASSET.fullmatch(fn)
            if match:
                infn = os.path.join(root, fn)
                outfn = os.path.join(root.replace(indir, outdir, 1), match.group('basename') + '.' + match.group('type'))
                os.makedirs(os.path.dirname(outfn), exist_ok=True)
                n_built += build_one(infn, outfn, match.group('type'))

    if msglevel >= MSG_NORMAL:
        if n_built == 0:
            print ("Nothing to do.")
        else:
            print ("Successfully built", n_built, "asset(s).")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Build assets from JSON format to PlatE binary formats. This program will recursively search " +
            "the source directory for assets that need to be baked and put them into corresponding paths in the " +
            "target directory. Only json assets that have changed will need to be rebaked."
    )

    parser.add_argument("source",
        help="The source directory to recursively search for assets"
    )
    parser.add_argument("target",
        help="The target toplevel directory to put baked assets into"
    )

    parser.add_argument("--no-backup", action='store_false', dest='backups', default=True,
        help="Suppress automatic backups. Note that failed bakes will delete the incomplete file."
    )

    parser.add_argument("--verbose", "-v", action='count', default=0,
        help="Increases the kinds of messages that are printed. Stackable."
    )
    parser.add_argument("--quiet", "-q", action='count', default=0,
        help="Reduces the kinds of messages that are printed. Stackable."
    )
    parser.add_argument("--silent", action='store_const', dest='quiet', const=9001,
        help="Print nothing, no matter how important."
    )
    parser.add_argument("--debug", action='store_const', dest='verbose', const=9001,
        help="Maximum verbosity for the tool developer."
    )

    parser.add_argument("--force", '-f', action='store_true', default=False,
        help="Forces rebuild of all files"
    )

    args = parser.parse_args()

    main(args.source, args.target, do_backups=args.backups, msglevel = args.verbose - args.quiet, force=args.force)
