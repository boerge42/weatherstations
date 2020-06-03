
set terminal tkcanvas size ::dx::, ::dy::
set output '::canvas_filename::'


set autoscale
set datafile separator ","

set xlabel "Runtime [ms]"
set ylabel "Strom [mA]"
set y2label "Level"

set grid y2tics lc rgb "#bbbbbb" lw 1 lt 0
set grid xtics lc rgb "#bbbbbb" lw 1 lt 0

set yrange [0:408]
set y2range [0:12]

set xtics 100
set ytics 10
set y2tics 1

plot "::csv_filename::" using 1:3 axes x1y1 title 'Current in mA' with lines lw 2, "" using 1:2 axes x1y2 title 'Level' with lines lw 3
