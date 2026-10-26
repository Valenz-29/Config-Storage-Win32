#!/bin/sh
# Traces the reference board photo into replica.json (vector layers for the "Réplica" design).
# Usage: sh run.sh <reference-photo.png> <work-dir>
# Needs Python with numpy, scipy, pillow and potracer.
set -e
here=$(dirname "$0")
python "$here/rectify.py" "$1" "$2"     # straighten the board into a 600 x 3000 grid
python "$here/classify.py" "$2"         # label pixels: white, black, green, orange, blue, unknown
python "$here/mask.py" "$2"             # paint logos white; mark bindings and background unknown
python "$here/fill3.py" "$2"            # continue stripes under the bindings
python "$here/vectorize2.py" "$2" 2     # smooth each color and trace it with potrace
