#
#     Uwe Berger; 2013
#
#
load ./libiic.so

set device 		/dev/i2c-7
set adr_rfm12	0x28

#**************************************
proc read_lm75 {} {
	global device adr_rfm12
	set temp [i2c read $device $adr_rfm12 0x04 w]
	# Low-/Hight-Byte
	set lb [expr $temp & 0x00FF]
	set hb [expr $temp >> 8]
	# Nachkommastelle
	if {[expr $hb & 0x80]} {set dec 5} else {set dec 0}
	# Vorzeichen
	if {[expr $lb & 0x80]} {set sign "-"} else {set sign "+"}
	# Vorkommawert
	set lb [expr $lb & 0x7f]
	# Return-Wert zusammensetzen
	return "$sign$lb.$dec"
}

#**************************************
proc read_counter {} {
	global device adr_rfm12
	set lo [i2c read $device $adr_rfm12 0 b]
	set hi [i2c read $device $adr_rfm12 1 b]
	return [expr $hi * 256 + $lo]
}

#**************************************
proc read_brightness {} {
	global device adr_rfm12
	set b1 [i2c read $device $adr_rfm12 4 b]
	set b2 [i2c read $device $adr_rfm12 5 b]
	set b3 [i2c read $device $adr_rfm12 6 b]
	set b4 [i2c read $device $adr_rfm12 7 b]
	return [expr $b4 * 16777216 + $b3 * 65536 + $b2 * 256 + $b1]
}

#**************************************
proc read_vcc {} {
	global device adr_rfm12
	set lo [i2c read $device $adr_rfm12 2 b]
	set hi [i2c read $device $adr_rfm12 3 b]
	return [expr ($hi * 256 + $lo)/1000.0]
}

#**************************************
proc read_sht15_humidity {} {
	global device adr_rfm12
	set lo [i2c read $device $adr_rfm12 8 b]
	set hi [i2c read $device $adr_rfm12 9 b]
	return [expr ($hi * 256 + $lo)/100.0]	
}

#**************************************
proc read_sht15_temperature {} {
	global device adr_rfm12
	set lo [i2c read $device $adr_rfm12 10 b]
	set hi [i2c read $device $adr_rfm12 11 b]
	return [expr ($hi * 256 + $lo)/100.0]		
}

#**************************************
proc read_tmp36 {} {
	global device adr_rfm12
	set lo [i2c read $device $adr_rfm12 12 b]
	set hi [i2c read $device $adr_rfm12 13 b]
	return [expr (($hi * 256 + $lo)-500)/10.0]		
}


#**************************************
proc do {} {
	puts "Counter.........: [read_counter]"
	puts "Vcc (Sender)....: [read_vcc]V"
	puts "Helligkeit......: [read_brightness]lux"
	puts "SHT15-Humidity..: [read_sht15_humidity]%"
	puts "SHT15_Temperatur: [read_sht15_temperature]°C"
	puts "TMP36...........: [read_tmp36]°C"
	puts "********************"
	after 60000 do

}


#**************************************
#**************************************
#**************************************
do	

vwait forever								
