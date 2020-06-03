#
# Test-Script fuer libiic.so
# ==========================
#     Uwe Berger; 2012
#
#
load ./libiic.so

set device 		/dev/i2c-2
set adr_saa1064	0x38
set adr_lm75	0x49
set segm 		[list 0x3F 0x06 0x5B 0x4F 0x66 0x6D 0x7D 0x07 0x7F 0x6F]

#**************************************
proc do {} {
	global device adr_saa1064 adr_lm75 segm
	#
	# LM75 auslesen
	#
	set temp [i2c read $device $adr_lm75 0x00 w]
	# Low-/Hight-Byte
	set lb [expr $temp & 0x00FF]
	set hb [expr $temp >> 8]
	# Nachkommastelle
	if {[expr $hb & 0x80]} {set dec 5} else {set dec 0}
	# Vorzeichen
	if {[expr $lb & 0x80]} {set sign "-"} else {set sign "+"}
	# Vorkommawert
	set lb [expr $lb & 0x7f]
	# Ausgabe (Bildschirm)
	puts "$sign$lb.$dec°C"

	#
	# Ausgabe auf SAA1064
	#
	if {$sign == "+"} {set d1 0x00} else {set d1 0x40}
	set d2 [lindex $segm [expr $lb / 10]]
	set d3 [expr [lindex $segm [expr $lb % 10]] + 0x80]
	set d4 [lindex $segm $dec]
	i2c write $device $adr_saa1064 [list 0x00 0x77 $d1 $d2 $d3 $d4]
	after 1000 do
}

#**************************************
#**************************************
#**************************************
do									

vwait forever
