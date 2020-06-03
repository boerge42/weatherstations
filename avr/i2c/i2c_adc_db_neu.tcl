#!/usr/bin/tclsh
#
#   i2c_adc_db.tcl
#  ================
#  Uwe Berger; 2012, 2013
#
#

package require sqlite3
load ./libiic.so


set tab_adc		"adc_log"
set tab_humidity	"humidity_log_neu"
set view_tab_humidity	"view_tab_humidity"
set tab_pressure	"pressure_log"
set tab_voltage		"voltage_log"

# set dbas		"/home/bergeruw/work/i2c/i2c_log"
#set dbas		"/media/fritzbox/WD-MyPassport0810-01/dockstar/wetter_db/i2c_log"
set dbas		"/media/fritzbox/WD-MyPassport0810-01/dockstar/wetter_db/weather_log_db"
# set temp_data		"/home/bergeruw/work/i2c/data.txt"
set temp_data		"/media/fritzbox/WD-MyPassport0810-01/dockstar/wetter_db/i2c_data.txt"

set plot_file_adc    	"/home/bergeruw/public_html/temp/adc_plot.png"
set plot_file_lux    	"/home/bergeruw/public_html/temp/lux_plot.png"
set plot_file_humidity	"/home/bergeruw/public_html/temp/humidity_plot.png"
set plot_file_pressure	"/home/bergeruw/public_html/temp/pressure_plot.png"
set plot_file_voltage	"/home/bergeruw/public_html/temp/voltage_plot.png"
set plot_config_file 	"/home/bergeruw/public_html/temp/plot_config.tcl"


set gvar(i2c_device)	/dev/i2c-0
set gvar(adc_0)			0x12
set gvar(adc_1)			0x13
set gvar(lm75)			0x49
set gvar(bmp085_addr)	0x77
set gvar(rfm12_rx)		0x28

set gvar(error_diff)	2
set gvar(counter)		0
set gvar(counter_last)	0
set gvar(counter_error) 0

set gvar(BMP085_CAL_AC1)			0xAA
set gvar(BMP085_CAL_AC2)			0xAC
set gvar(BMP085_CAL_AC3)			0xAE
set gvar(BMP085_CAL_AC4)			0xB0
set gvar(BMP085_CAL_AC5)			0xB2
set gvar(BMP085_CAL_AC6)			0xB4
set gvar(BMP085_CAL_B1)				0xB6
set gvar(BMP085_CAL_B2)				0xB8
set gvar(BMP085_CAL_MB)				0xBA
set gvar(BMP085_CAL_MC)				0xBC
set gvar(BMP085_CAL_MD)				0xBE
set gvar(BMP085_CONTROL)			0xF4
set gvar(BMP085_TEMPDATA) 			0xF6
set gvar(BMP085_PRESSUREDATA)		0xF6
set gvar(BMP085_READTEMPCMD)		0x2E
set gvar(BMP085_READPRESSURECMD)	0x34

set gvar(old_min) 99

set adc(xsize) 900
set adc(ysize) 400
set adc(font) "arial 8"
set adc(ymin) 0
set adc(ymax) 255
set adc(dt)   48

set humidity(xsize) 900
set humidity(ysize) 400
set humidity(font) "arial 8"
set humidity(ymin) 0
set humidity(ymax) 100
set humidity(dt)   48

set pressure(xsize) 900
set pressure(ysize) 400
set pressure(font) "arial 8"
set pressure(ymin) 980
set pressure(ymax) 1040
set pressure(dt)   48
set pressure(altitude)	47.5

set voltage(xsize) 900
set voltage(ysize) 400
set voltage(font) "arial 8"
set voltage(ymin) 2.8
set voltage(ymax) 4.7
set voltage(dt)   5500

#***********************************************
proc i2c_read {chip_address data_address mode} {
	global gvar	

	while {[catch {set r [i2c read $gvar(i2c_device) $chip_address $data_address $mode]}]} {
		after 100
		puts "...nochmal!"}
	return $r
}


#***********************************************
proc read16_bmp085 {bmp085_addr bmp085_data sign} {
	global gvar
	set msb [i2c read $gvar(i2c_device) $bmp085_addr $bmp085_data b]
	set lsb [i2c read $gvar(i2c_device) $bmp085_addr [expr $bmp085_data + 1] b]
	set r [expr ($msb << 8) + $lsb]
	if {$sign} {
		if {$r >= 32768} {
			set r [expr $r - 32768 - 32768]
		}
	}
	return $r	
}

#***********************************************
proc write_bmp085 {bmp085_addr bmp085_control bmp085_cmd} {
	global gvar
	i2c write $gvar(i2c_device) $bmp085_addr [list $bmp085_control $bmp085_cmd]
}


#***********************************************
proc read_bmp085_calibration {bmp085_addr} {
	global gvar
	set gvar(ac1) [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_AC1) 1]
	set gvar(ac2) [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_AC2) 1]
	set gvar(ac3) [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_AC3) 1]
	set gvar(ac4) [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_AC4) 0]
	set gvar(ac5) [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_AC5) 0]
	set gvar(ac6) [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_AC6) 0]
	set gvar(b1)  [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_B1)  1]
	set gvar(b2)  [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_B2)  1]
	set gvar(mb)  [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_MB)  1]
	set gvar(mc)  [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_MC)  1]
	set gvar(md)  [read16_bmp085 $bmp085_addr $gvar(BMP085_CAL_MD)  1]
}


#***********************************************
proc task {delay} {
	global gvar tab_adc tab_humidity tab_pressure tab_voltage
	catch {	
	# ===== RFM12-Sender da (...aendert sich Message-Counter...)? =====
	set lo [i2c_read $gvar(rfm12_rx) 0 b]
	set hi [i2c_read $gvar(rfm12_rx) 1 b]
	set gvar(counter_rx) [expr $hi * 256 + $lo]
	if {$gvar(counter_rx) != $gvar(counter_last)} {
		set gvar(counter_error) 0
		set gvar(counter_last) $gvar(counter_rx)
	} else {
		incr gvar(counter_error) 
	}	
	puts "$gvar(counter_rx), $gvar(counter_last) --> $gvar(counter_error)"
	if {$gvar(counter_error) <= $gvar(error_diff)} {
		set rfm12_ok 1
	} else {
		set rfm12_ok 0		
	}
	
	# ===== Versorgungsspannung RFM12 auslesen =====
	# Helligkeitswert RFM12_Rx
	if {$rfm12_ok} {
		set lo [i2c_read $gvar(rfm12_rx) 2 b]
		set hi [i2c_read $gvar(rfm12_rx) 3 b]
		set rfm12_voltage [expr ($hi * 256 + $lo)/1000.0]
	} else {
		set rfm12_voltage 0		
	}
	# Timestamp
	set timestamp [clock format [clock seconds] -format %Y-%m-%dT%H:%M:%S]
   	# in DB schreiben
	set sql [list insert into $tab_voltage values ('$timestamp', $rfm12_voltage)]
	db eval $sql
	# ...
	puts $sql
	
	# ===== ADC_0 auslesen (Licht) =====
	set adc0 [i2c_read $gvar(adc_0) 0 b]
	after 100 
	set adc1 [i2c_read $gvar(adc_0) 1 b]
	# Helligkeitswert RFM12_Rx
	if {$rfm12_ok} {
		set b1 [i2c_read $gvar(rfm12_rx) 4 b]
		set b2 [i2c_read $gvar(rfm12_rx) 5 b]
		set b3 [i2c_read $gvar(rfm12_rx) 6 b]
		set b4 [i2c_read $gvar(rfm12_rx) 7 b]
		set lx [expr $b4 * 16777216 + $b3 * 65536 + $b2 * 256 + $b1]
	} else {
		set lx 0		
	}
	# Timestamp
	set timestamp [clock format [clock seconds] -format %Y-%m-%dT%H:%M:%S]
   	# in DB schreiben
	set sql [list insert into $tab_adc values ('$timestamp', $adc0, $adc1, $lx)]
	db eval $sql
	# ...
	puts $sql
	
	# ===== ADC_1 und lm75 auslesen (Luftfeuchtigkeit) =====
	# ...adc...
	set adc [i2c_read $gvar(adc_1) 1 b]
	after 100 
	# ...lm75...
	set val [i2c_read $gvar(lm75) 0 w]
	# Low-/Hight-Byte
	set lb [expr $val & 0x00FF]
	set hb [expr $val >> 8]
	# Nachkommastelle
	if {[expr $hb & 0x80]} {set dec 5} else {set dec 0}
	# Vorzeichen
	if {[expr $lb & 0x80]} {set sign "-"} else {set sign "+"}
	# Vorkommawert
	set lb [expr $lb & 0x7f]
	# zusammensetzen
	set lm75 "$sign$lb.$dec"
	# SHT15-Luftfeuchtigkeit/-Temperatur RFM12_Rx
	if {$rfm12_ok} {
		set lo [i2c_read $gvar(rfm12_rx) 8 b]
		set hi [i2c_read $gvar(rfm12_rx) 9 b]
		set sht15_hum [expr ($hi * 256 + $lo)/100.0]	
		set lo [i2c_read $gvar(rfm12_rx) 10 b]
		set hi [i2c_read $gvar(rfm12_rx) 11 b]
		set sht15_temp [expr ($hi * 256 + $lo)/100.0]		
	} else {
		set sht15_hum 0
		set sht15_temp 0		
	}
	# Timestamp
	set timestamp [clock format [clock seconds] -format %Y-%m-%dT%H:%M:%S]
    	# in DB schreiben
	set sql [list insert into $tab_humidity values ('$timestamp', $adc, $lm75, $sht15_hum, $sht15_temp)]
	db eval $sql
	# ...
	puts $sql
	
	# ==== Luftdruck (BMP085) ====
	# Temperatur auslesen
	write_bmp085 $gvar(bmp085_addr) $gvar(BMP085_CONTROL) $gvar(BMP085_READTEMPCMD)
	after 5
	set ut [read16_bmp085 $gvar(bmp085_addr) $gvar(BMP085_TEMPDATA) 0]
	# Luftdruck auslesen
	write_bmp085 $gvar(bmp085_addr) $gvar(BMP085_CONTROL) $gvar(BMP085_READPRESSURECMD)
	after 5
	set up [read16_bmp085 $gvar(bmp085_addr) $gvar(BMP085_PRESSUREDATA) 0]
	# reale Temperatur berechnen
	set x1 [expr ($ut - $gvar(ac6)) * $gvar(ac5) / 32768]
	set x2 [expr $gvar(mc) * 2048 / ($x1 + $gvar(md))]
	set b5 [expr $x1 + $x2]
	set t  [expr (($b5 + 8)/16)/10.0]	
	# realen Luftdruck berechnen
	set b6 [expr $b5 - 4000]
	set x1 [expr ($gvar(b2) * ($b6 * $b6 / 4096)) / 2048]
	set x2 [expr $gvar(ac2) * $b6 / 2048]
	set x3 [expr $x1 + $x2]
	set b3 [expr (($gvar(ac1) * 4 + $x3) + 2) / 4]
	set x1 [expr $gvar(ac3) * $b6 / 8192]
	set x2 [expr ($gvar(b1) * ($b6 * $b6 / 4096)) / 65536]
	set x3 [expr (($x1 + $x2) + 2) / 4]
	set b4 [expr $gvar(ac4) * ($x3 + 32768) / 32768]
	set b7 [expr ($up - $b3) * 50000]
	if {$b7 < 0x80000000} {
		set p [format "%.0f" [expr ($b7 * 2.0) / $b4]]
	} else {
		set p [format "%.0f" [expr ($b7 / $b4) * 2.0]]
	}
	set x1 [expr ($p / 256) * ($p / 256)]
	set x1 [expr ($x1 * 3038) / 65536]
	set x2 [expr (-7357 * $p) / 65536]
	set p  [expr $p + ($x1 + $x2 + 3791) / 16]
	set p  [expr $p / 100.0]
	# Timestamp
	set timestamp [clock format [clock seconds] -format %Y-%m-%dT%H:%M:%S]
   	# in DB schreiben
	set sql [list insert into $tab_pressure values ('$timestamp', $p, $t)]
	db eval $sql
	# ...
	puts $sql
	}
	
	after $delay [list task $delay]
} 

#***********************************************
proc generate_plot {} {
	global g dbas tab_adc tab_humidity view_tab_humidity tab_pressure tab_voltage
	global plot_config_file temp_data 
	global plot_file_adc plot_file_humidity plot_file_pressure plot_file_lux plot_file_voltage
	global adc pressure humidity voltage
	catch {
	# Versuch ein vorhandenes Konfig-File zu lesen
	catch {
		source $plot_config_file
	}
	puts "Diagramm zeichnen"

	# Timestamp ermitteln
	set day     [clock format [clock seconds] -format %d]
	set month   [clock format [clock seconds] -format %m]
	set year    [clock format [clock seconds] -format %Y]
	set hour    [clock format [clock seconds] -format %H]
	set minute  [clock format [clock seconds] -format %M]
	set seconds [clock format [clock seconds] -format %S]
	
	# ====> Licht (Solarzelle/Fotowiderstand) <====
	# xmin/ymin setzen
	set temp_d [clock scan "$year-$month-$day\T$hour:$minute:00" -format %Y-%m-%dT%H:%M:%S]
    	set adc(xmax) "$year-$month-$day\T$hour:$minute:00"
   	set adc(xmin) [clock format [expr $temp_d - $adc(dt)*3600] -format %Y-%m-%dT%H:%M:%S]
	# temporaeres Datenfile erzeugen
	set sql "select * from $tab_adc where ts > '$adc(xmin)'"
	exec /usr/bin/sqlite3 $dbas $sql > $temp_data
	# gnuplot oeffnen...	
	catch {
	set pid [open "| gnuplot" "w"]	
	puts $pid "set title \"Helligkeitsverlauf (ADCs) über $adc(dt) Stunden\\naktuell: $day.$month.$year $hour:$minute:$seconds\""
	puts $pid "set size 1, 1"
	puts $pid "set ytics 20"
	puts $pid "set xtics autofreq"
	puts $pid "set xdata time"
	puts $pid "set timefmt '%Y-%m-%dT%H:%M:%S'"
	puts $pid "set format x \"%H:%M\\n%d.%m.%Y\""
	puts $pid "set autoscale y"
	puts $pid "set autoscale x"
	puts $pid "set xrange \['$adc(xmin)':'$adc(xmax)'\]"
	puts $pid "set yrange \[$adc(ymin):$adc(ymax)\]"
	puts $pid "set ylabel 'ADC-Wert'"
	puts $pid "set datafile separator '|'"
	puts $pid "set grid ytics xtics"
	puts $pid "set terminal png font $adc(font) size $adc(xsize), $adc(ysize)"
	puts $pid "set output '$plot_file_adc'"
	puts $pid "set key top horizontal right" 	
	puts -nonewline $pid "plot "
	puts -nonewline $pid "'$temp_data' using 1:2 title 'Fotowiderstand' with lines, "
	puts -nonewline $pid "'$temp_data' using 1:3 title 'Solarzelle' with lines "
	puts $pid ""
	close $pid
	}
	# ====> Licht (TSL45315) <====
	# xmin/ymin setzen
	set temp_d [clock scan "$year-$month-$day\T$hour:$minute:00" -format %Y-%m-%dT%H:%M:%S]
    	set adc(xmax) "$year-$month-$day\T$hour:$minute:00"
   	set adc(xmin) [clock format [expr $temp_d - $adc(dt)*3600] -format %Y-%m-%dT%H:%M:%S]
	# temporaeres Datenfile erzeugen
#	set sql "select * from $tab_adc where ts > '$adc(xmin)'"
#	exec /usr/bin/sqlite3 $dbas $sql > $temp_data
	# gnuplot oeffnen...
	catch {	
	set pid [open "| gnuplot" "w"]	
	puts $pid "set title \"Helligkeitsverlauf (in lux) über $adc(dt) Stunden\\naktuell: $day.$month.$year $hour:$minute:$seconds\""
	puts $pid "set size 1, 1"
	puts $pid "set xtics autofreq"
	puts $pid "set xdata time"
	puts $pid "set timefmt '%Y-%m-%dT%H:%M:%S'"
	puts $pid "set format x \"%H:%M\\n%d.%m.%Y\""
	puts $pid "set logscale y 10"	
	puts $pid "set ytics (1,5,10,50,75,100,500,1000,5000,10000,50000,100000)"
	puts -nonewline $pid "set ytics ("
	puts -nonewline $pid "\"1\" 1,\"\" 2.5,\"\" 5,\"\" 7.5,"
	puts -nonewline $pid "\"10\" 10,\"\" 25,\"\" 50,\"\" 75,"
	puts -nonewline $pid "\"100\" 100,\"\" 250,\"\" 500,\"\" 750,"
	puts -nonewline $pid "\"1000\" 1000,\"\" 2500,\"\" 5000,\"\" 7500,"
	puts -nonewline $pid "\"10000\" 10000,\"\" 25000,\"\" 50000,\"\" 75000,"
	puts -nonewline $pid "\"100000\" 100000"
	puts $pid ")"
	puts $pid "set autoscale x"
	puts $pid "set xrange \['$adc(xmin)':'$adc(xmax)'\]"
	puts $pid "set yrange \[1:100000\]"
	puts $pid "set ylabel 'lx'"
	puts $pid "set datafile separator '|'"
	puts $pid "set grid ytics xtics"
	puts $pid "set terminal png font $adc(font) size $adc(xsize), $adc(ysize)"
	puts $pid "set output '$plot_file_lux'"
	puts $pid "set key top horizontal right" 	
	puts -nonewline $pid "plot "
	puts -nonewline $pid "'$temp_data' using 1:4 title 'TSL45315' with lines "
	puts $pid ""
	close $pid
	}
	# ====> Luftfeuchtigkeit <====
	# xmin/ymin setzen
	set temp_d [clock scan "$year-$month-$day\T$hour:$minute:00" -format %Y-%m-%dT%H:%M:%S]
    set humidity(xmax) "$year-$month-$day\T$hour:$minute:00"
   	set humidity(xmin) [clock format [expr $temp_d - $humidity(dt)*3600] -format %Y-%m-%dT%H:%M:%S]
	# temporaeres Datenfile erzeugen
#	set sql "select ts, ((((adc/255.0)*5)-0.958062)*30.680)/(1.0546-(0.00216*temp)), sht15_hum from $tab_humidity where ts > '$humidity(xmin)'"
#	set sql "select ts, (((((adc/256.0)*5)/5)-0.16)/0.0062)/(1.0546-(0.00216*temp)), sht15_hum from $tab_humidity where ts > '$humidity(xmin)'"
	set sql "select ts, hih4030_hum, sht15_hum from $view_tab_humidity where ts > '$humidity(xmin)'"
	exec /usr/bin/sqlite3 $dbas $sql > $temp_data
	# gnuplot oeffnen...	
	catch {
	set pid [open "| gnuplot" "w"]	
	puts $pid "set title \"Luftfeuchtigkeitsverlauf über $humidity(dt) Stunden\\naktuell: $day.$month.$year $hour:$minute:$seconds\""
	puts $pid "set size 1, 1"
	puts $pid "set ytics 5"
	puts $pid "set xtics autofreq"
	puts $pid "set xdata time"
	puts $pid "set timefmt '%Y-%m-%dT%H:%M:%S'"
	puts $pid "set format x \"%H:%M\\n%d.%m.%Y\""
	puts $pid "set autoscale y"
	puts $pid "set autoscale x"
	puts $pid "set xrange \['$humidity(xmin)':'$humidity(xmax)'\]"
	puts $pid "set yrange \[$humidity(ymin):$humidity(ymax)\]"
	puts $pid "set ylabel 'relative Luftfeuchte in %'"
	puts $pid "set datafile separator '|'"
	puts $pid "set grid ytics xtics"
	puts $pid "set terminal png font $humidity(font) size $humidity(xsize), $humidity(ysize)"
	puts $pid "set output '$plot_file_humidity'"
	puts $pid "set key top horizontal right" 	
	puts -nonewline $pid "plot "
	puts -nonewline $pid "'$temp_data' using 1:2 title 'HIH-4030' with lines, "
	puts -nonewline $pid "'$temp_data' using 1:3 title 'SHT15' with lines "
	puts $pid ""
	close $pid
	}
	# ====> Luftdruck <====
	# xmin/ymin setzen
	set temp_d [clock scan "$year-$month-$day\T$hour:$minute:00" -format %Y-%m-%dT%H:%M:%S]
    set pressure(xmax) "$year-$month-$day\T$hour:$minute:00"
   	set pressure(xmin) [clock format [expr $temp_d - $pressure(dt)*3600] -format %Y-%m-%dT%H:%M:%S]
	# temporaeres Datenfile erzeugen
	set sql "select ts, pressure, (pressure+($pressure(altitude)/8)) from $tab_pressure where ts > '$pressure(xmin)'"
	exec /usr/bin/sqlite3 $dbas $sql > $temp_data
	# gnuplot oeffnen...	
	catch {
	set pid [open "| gnuplot" "w"]	
	puts $pid "set title \"Luftdruckverlauf über $pressure(dt) Stunden\\naktuell: $day.$month.$year $hour:$minute:$seconds\""
	puts $pid "set size 1, 1"
	puts $pid "set ytics 5"
	puts $pid "set xtics autofreq"
	puts $pid "set xdata time"
	puts $pid "set timefmt '%Y-%m-%dT%H:%M:%S'"
	puts $pid "set format x \"%H:%M\\n%d.%m.%Y\""
	puts $pid "set autoscale y"
	puts $pid "set autoscale x"
	puts $pid "set xrange \['$pressure(xmin)':'$pressure(xmax)'\]"
	puts $pid "set yrange \[$pressure(ymin):$pressure(ymax)\]"
	puts $pid "set ylabel 'Luftdruck in hPa'"
	puts $pid "set datafile separator '|'"
	puts $pid "set grid ytics xtics"
	puts $pid "set terminal png font $pressure(font) size $pressure(xsize), $pressure(ysize)"
	puts $pid "set output '$plot_file_pressure'"
	puts $pid "set key top horizontal right" 	
	puts -nonewline $pid "plot "
	puts -nonewline $pid "'$temp_data' using 1:2 title 'absoluter Luftdruck (in $pressure(altitude)m Höhe)' with lines, "
	puts -nonewline $pid "'$temp_data' using 1:3 title 'relativer Luftdruck' with lines "
	puts $pid ""
	close $pid	
	}
	# ====> Versorgungsspannung RFM12-Sender <====
	# xmin/ymin setzen
	set temp_d [clock scan "$year-$month-$day\T$hour:$minute:00" -format %Y-%m-%dT%H:%M:%S]
    set voltage(xmax) "$year-$month-$day\T$hour:$minute:00"
   	set voltage(xmin) [clock format [expr $temp_d - $voltage(dt)*3600] -format %Y-%m-%dT%H:%M:%S]
	# temporaeres Datenfile erzeugen
	set sql "select ts, rfm12_voltage from $tab_voltage where ts > '$voltage(xmin)'"
	exec /usr/bin/sqlite3 $dbas $sql > $temp_data
	# gnuplot oeffnen...	
	catch {
	set pid [open "| gnuplot" "w"]	
	puts $pid "set title \"Versorgungsspannungsverlauf über $voltage(dt) Stunden\\naktuell: $day.$month.$year $hour:$minute:$seconds\""
	puts $pid "set size 1, 1"
	puts $pid "set ytics 5"
	puts $pid "set xtics autofreq"
	puts $pid "set xdata time"
	puts $pid "set timefmt '%Y-%m-%dT%H:%M:%S'"
	puts $pid "set format x \"%d.%m.%Y\""
	puts $pid "set autoscale y"
	puts $pid "set autoscale x"
	puts $pid "set xrange \['$voltage(xmin)':'$voltage(xmax)'\]"
	puts $pid "set yrange \[$voltage(ymin):$voltage(ymax)\]"
	puts $pid "set ylabel 'Spannung in Volt'"
	puts $pid "set datafile separator '|'"
	puts $pid "set ytics 0.1"
	puts $pid "set grid xtics ytics"
	puts $pid "set terminal png font $voltage(font) size $voltage(xsize), $voltage(ysize)"
	puts $pid "set output '$plot_file_voltage'"
	puts $pid "set key top horizontal right" 	
	puts -nonewline $pid "plot "
	puts -nonewline $pid "'$temp_data' using 1:2 title 'RFM12-Sender' with lines"
	puts $pid ""
	close $pid	
	}
	}
	# nach 2min wieder aufrufen...
	after 120000 generate_plot
}



#***********************************************
#***********************************************
#***********************************************

# Datenbank oeffnen bzw. eventuell neu anlegen
sqlite3 db $dbas
db timeout 2500000

# gibt es Tabelle adc_log
if {[db exists {select 1 from sqlite_master where name=$tab_adc and type='table'}]} {
	# alles klar, Tabelle vorhanden
	puts "Tabelle $tab_adc vorhanden!" 
} else {
	# Tabelle anlegen
	db eval [list create table $tab_adc (ts text, adc0 integer, adc1 integer)]
}
# eventuell neue Spalte in Tabelle anlegen
if {[lsearch [db eval [list pragma table_info($tab_adc)]] tsl45315] > -1} {
	# Spalte vorhanden...
} else {
	db eval [list alter table $tab_adc add column 'tsl45315' integer]
}

# gibt es Tabelle humidity_log
if {[db exists {select 1 from sqlite_master where name=$tab_humidity and type='table'}]} {
	# alles klar, Tabelle vorhanden
	puts "Tabelle $tab_humidity vorhanden!" 
} else {
	# Tabelle anlegen
	db eval [list create table $tab_humidity (ts text, adc integer, temp real)]
}
# eventuell neue Spalten in Tabelle anlegen
if {[lsearch [db eval [list pragma table_info($tab_humidity)]] sht15_hum] > -1} {
	# Spalte vorhanden...
} else {
	db eval [list alter table $tab_humidity add column 'sht15_hum' real]
}
if {[lsearch [db eval [list pragma table_info($tab_humidity)]] sht15_temp] > -1} {
	# Spalte vorhanden...
} else {
	db eval [list alter table $tab_humidity add column 'sht15_temp' real]
}
# View anlegen, falls es ihn noch nicht gibt...
db eval [list create view if not exists $view_tab_humidity as select ts, (((((adc/256.0)*5)/5)-0.16)/0.0062)/(1.0546-(0.00216*temp)) as hih4030_hum, sht15_hum from $tab_humidity]

# gibt es Tabelle pressure_log
if {[db exists {select 1 from sqlite_master where name=$tab_pressure and type='table'}]} {
	# alles klar, Tabelle vorhanden
	puts "Tabelle $tab_pressure vorhanden!" 
} else {
	# Tabelle anlegen
	db eval [list create table $tab_pressure (ts text, pressure real, temp real)]
}

# gibt es Tabelle voltage_log
if {[db exists {select 1 from sqlite_master where name=$tab_voltage and type='table'}]} {
	# alles klar, Tabelle vorhanden
	puts "Tabelle $tab_voltage vorhanden!" 
} else {
	# Tabelle anlegen
	db eval [list create table $tab_voltage (ts text, rfm12_voltage real)]
}


# Calibration BMP085 auslesen
read_bmp085_calibration $gvar(bmp085_addr)

task 60000
generate_plot


# Endlos
vwait forever
