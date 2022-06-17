#! /bin/tcsh -f
#
#
#

set map = $1
set reso = $2

set GRID = ( 32 32 32 )
set header = 1104

set prefix = `basename $map .map`

./float_add -header $header $map

set offset = `./float_add -header $header $map | awk '/^mean/{print $3}'`
echo "offset=$offset"

rm -f sfallme.map sfall.mtz temp.amp

echo "axis Z X Y" | mapmask mapin $map mapout sfallme.map > /dev/null
echo "mode sfcalc mapin" | sfall mapin sfallme.map hklout sfall.mtz > /dev/null
fft hklin sfall.mtz mapout temp.map << EOF > /dev/null
labin F1=FC PHI=PHIC
resolution $reso
GRID $GRID
EOF
mapmask mapin temp.map mapout ${prefix}_${reso}A.map << EOF > /dev/null
axis X Y Z
scale factor 1 $offset
xyzlim cell
EOF
./float_add -header $header ${prefix}_${reso}A.map
