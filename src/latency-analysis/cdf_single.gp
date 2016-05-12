#!/usr/bin/gnuplot

set term pdfcairo enhanced linewidth 3 rounded font "Arial,20" size 5in,3.0in dashed dashlength 2
set datafile separator ','

set xtics nomirror
set ytics nomirror

# Line styles: try to pick pleasing colors, rather
# than strictly primary colors or hard-to-see colors
# like gnuplot's default yellow.  Make the lines thick
# so they're easy to see in small plots in papers.
set style line 1 lt rgb "#A00000" lw 4
set style line 2 lt rgb "#00A000" lw 4
set style line 3 lt rgb "#5060D0" lw 4
set style line 4 lt rgb "#F25900" lw 4
set style line 5 lt rgb "#000000" lw 4
set style line 6 lt rgb "#9834A3" lw 4 pi 200 pt 5
set style line 7 lt rgb "#5A9C9C" lw 4 pi 200 pt 7

set key bottom right font ",17" spacing 0.9
set yrange [0:1]
set xlabel x_label
set ylabel "CDF"

plot input u ($1*scale):2 w l ls 1 notitle
set output
