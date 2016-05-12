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
set style line 2 lt rgb "#F25900" pt 7 ps 0.6
set style line 3 lt rgb "#000000" pt 7 ps 0.6
set style line 4 lt rgb "#9834A3" pt 7 ps 0.6
set style line 5 lt rgb "#5A9C9C" pt 7 ps 0.6
set style line 6 lt rgb "#5060D0" lw 3

set key bottom right font ",17" spacing 0.9
set xlabel x_label
set ylabel y_label

# Linear fit function (incorporating the scaling factors)
f(x) = (c_0 + c_1 * (x/(scale_x))) * scale_y

plot "<(sed -n '1,100p' ".input.")" u ($1 * scale_x):($2 * scale_y) w p ls 1 notitle, \
     "<(sed -n '100,200p' ".input.")" u ($1 * scale_x):($2 * scale_y) w p ls 2 notitle, \
     "<(sed -n '200,300p' ".input.")" u ($1 * scale_x):($2 * scale_y) w p ls 3 notitle, \
     "<(sed -n '300,400p' ".input.")" u ($1 * scale_x):($2 * scale_y) w p ls 4 notitle, \
     f(x) w l ls 6 t 'Linear fit'

set output
