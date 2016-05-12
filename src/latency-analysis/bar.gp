#!/usr/bin/gnuplot

set term pdfcairo enhanced linewidth 3 rounded font "Arial,20" size 10in,3.0in dashed dashlength 2
set datafile separator ','

set xtics nomirror
set ytics nomirror
set grid ytics lc rgb "#BBBBBB" lw 1 lt 0


# Line styles: try to pick pleasing colors, rather
# than strictly primary colors or hard-to-see colors
# like gnuplot's default yellow.  Make the lines thick
# so they're easy to see in small plots in papers.
set style line 1 lt rgb "#A00000"
set style line 2 lt rgb "#5060D0"
set style line 3 lt rgb "#F25900"
set style line 4 lt rgb "#000000"
set style line 5 lt rgb "#9834A3"
set style line 6 lt rgb "#5A9C9C"

set key outside bottom center horizontal box font ",17" height 0.5
set xlabel x_label offset 0,0.5
set ylabel y_label

set style data histogram
set style histogram rowstacked
set style fill solid 1 noborder
set boxwidth 0.9 relative
right_xbound = num_bars + 0.3
set xrange [-0.3:right_xbound]

plot for [COL=1:num_cols] input using (column(COL)*scale) title columnheader ls COL
set output
