#!/usr/bin/gnuplot

set term pdfcairo enhanced linewidth 3 rounded font "Arial,20" size 5in,3.5in dashed dashlength 2
set datafile separator ','

set xtics nomirror
set ytics nomirror

# Line styles: try to pick pleasing colors, rather
# than strictly primary colors or hard-to-see colors
# like gnuplot's default yellow.  Make the points thick
# so they're easy to see in small plots in papers.
set style line 1 lt rgb "#A00000" pt 7 ps 0.6

set key bottom right font ",17" spacing 0.9
set xlabel x_label
set ylabel y_label

plot input u ($1*scale_x):($2*scale_y) w p ls 1 notitle
set output
