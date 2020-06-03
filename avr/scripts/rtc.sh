#!/bin/sh

BUS=0
ADDR=0x50

get_part () { i2cget -y "$BUS" "$ADDR" $1 | sed -e 's/^0x//' ; }

ss=`get_part 2`
mm=`get_part 3`
hh=`get_part 4`
dd=`get_part 5`
mo=`get_part 6`

echo "RTC time: $dd.$mo. $hh:$mm:$ss"

