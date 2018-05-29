#!/usr/bin/env bash

tempdat=$(mktemp)

../2/sin 10000 3528 500 | ../2/read_data2 > $tempdat
gnuplot -p -e "plot '$tempdat' with linespoints, '' every 10 with linespoints"

rm $tempdat
