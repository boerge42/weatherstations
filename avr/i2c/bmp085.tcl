#
#    BMP085 via I2C auslesen
#    =======================
#



set gvar(i2cset_cmd) /usr/sbin/i2cset
set gvar(i2cget_cmd) /usr/sbin/i2cget

set gvar(i2c_bus) 0
set gvar(bmp085_addr) 0x77

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



#***********************************************
proc read16_bmp085 {i2c_bus bmp085_addr bmp085_data sign} {
	global gvar
	set msb [exec $gvar(i2cget_cmd) -y $i2c_bus $bmp085_addr $bmp085_data b]
	set lsb [exec $gvar(i2cget_cmd) -y $i2c_bus $bmp085_addr [expr $bmp085_data + 1] b]
	set r [expr ($msb << 8) + $lsb]
	if {$sign} {
		if {$r >= 32768} {
			set r [expr $r - 32768 - 32768]
		}
	}
	return $r	
}

#***********************************************
proc write_bmp085 {i2c_bus bmp085_addr bmp085_control bmp085_cmd} {
	global gvar
	exec $gvar(i2cset_cmd) -y $i2c_bus $bmp085_addr $bmp085_control $bmp085_cmd
}


#***********************************************
proc read_bmp085_calibration {i2c_bus bmp085_addr} {
	global gvar
	set gvar(ac1) [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_AC1) 1]
	set gvar(ac2) [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_AC2) 1]
	set gvar(ac3) [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_AC3) 1]
	set gvar(ac4) [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_AC4) 0]
	set gvar(ac5) [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_AC5) 0]
	set gvar(ac6) [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_AC6) 0]
	set gvar(b1)  [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_B1)  1]
	set gvar(b2)  [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_B2)  1]
	set gvar(mb)  [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_MB)  1]
	set gvar(mc)  [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_MC)  1]
	set gvar(md)  [read16_bmp085 $i2c_bus $bmp085_addr $gvar(BMP085_CAL_MD)  1]
}

#***********************************************
proc get_bmp085_raw_temperature {} {
	
}

#***********************************************
proc get_bmp085_raw_pressure {} {
	
}


# Calibration auslesen
read_bmp085_calibration $gvar(i2c_bus) $gvar(bmp085_addr)

# Temperatur auslesen
write_bmp085 $gvar(i2c_bus) $gvar(bmp085_addr) $gvar(BMP085_CONTROL) $gvar(BMP085_READTEMPCMD)
after 5
set ut [read16_bmp085 $gvar(i2c_bus) $gvar(bmp085_addr) $gvar(BMP085_TEMPDATA) 0]
#puts $ut

# Luftdruck auslesen
write_bmp085 $gvar(i2c_bus) $gvar(bmp085_addr) $gvar(BMP085_CONTROL) $gvar(BMP085_READPRESSURECMD)
after 5
set up [read16_bmp085 $gvar(i2c_bus) $gvar(bmp085_addr) $gvar(BMP085_PRESSUREDATA) 0]
#puts $up


# reale Temperatur berechnen
set x1 [expr ($ut - $gvar(ac6)) * $gvar(ac5) / 32768]
set x2 [expr $gvar(mc) * 2048 / ($x1 + $gvar(md))]
set b5 [expr $x1 + $x2]
set t  [expr (($b5 + 8)/16)/10.0]

# realen Lufzdruck berechnen
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

# 47,21m ueber 0


puts ""
puts "Temperatur: $t°C"
puts "Luftdruck : $p hPa"
