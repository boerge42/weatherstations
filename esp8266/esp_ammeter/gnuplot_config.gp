
set terminal tkcanvas size ::dx::, ::dy::
set output '::canvas_filename::'


set autoscale
set datafile separator ","

set xlabel "Runtime in ms"
set ylabel "mA"
set y2label "mAh"

set grid y2tics lc rgb "#bbbbbb" lw 1 lt 0
set grid xtics lc rgb "#bbbbbb" lw 1 lt 0

set yrange [0:300]
set y2range [0:0.06]

set xtics 100
#set ytics 10
set y2tics 0.002

plot "::csv_filename::" using 1:3 axes x1y1 with lines lw 2, "" using 1:($3/1000/3600) axes x1y2 smooth cumulative lw 2
