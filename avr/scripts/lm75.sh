#!/bin/bash
#
# Kodierung der Temperatur im LM75
# --------------------------------
#
#   High-Byte          Low-Byte
# 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0
# x 1 1 1 1 1 1 1   x x x x x x x x 
# |                 | |...........|
# |                 | |
# |                 | +-- Temperatur (Vorkomma): Bit 0...6
# |                 +---- Vorzeichen: 1 -> Temperatur < 0°C
# +---------------------- Nachkommastelle: 0 -> ,0°C; 1 -> ,5°C
#

BUS=0
ADDR=0x49

get_part () { i2cget -y "$BUS" "$ADDR" $1 w | sed -e 's/^0x//' ; }

# lm75 auslesen und in dezimal
val=$((16#`get_part 0`))

echo "val: $val"

# Nachkommawert bestimmen
if [ $val -ge 65280 ]
then
	nk=5
	val=`expr $val - 65280` # FF00
else
	nk=0
	val=`expr $val - 32512` # 7F00
fi

echo "val: $val"

# Vorzeichen und Temperatur selbst ermitteln
if [ $val -ge 128 ]
then
	vz="-"
	vk= `expr $val - 128`
else
	vz="+"
	vk=$val
fi

echo "Thermo-Board: $vz$vk,$nk °C"

