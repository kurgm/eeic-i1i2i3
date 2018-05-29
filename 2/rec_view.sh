#!/usr/bin/env bash

tempraw=$(mktemp)

rec -t raw -b 8 -c 1 -e un -r 44100 - > $tempraw
./read_data $tempraw | gnuplot -p -e "plot '-' with linespoints"

rm $tempraw
