#! /bin/tcsh -f
#
#	generate a CCP4 map header with given cell				-James Holton 11-22-11
#
#

# defaults
set CELL   = ( 20 20 20 90 90 90  )
set GRID   = ""
set AXIS   = ( X Y Z )
set SG     = 1
set outfile  = header.map

set tempfile = ${CCP4_SCR}/map_head$$
set logfile = /dev/null

set tempfile = tempfileasdf
#set logfile = /dev/tty
set logfile = debuglog.log
#echo -n "" >! $logfile

set i = 0
set user_maps = 0
while( $i < $#argv )
    @ i = ( $i + 1 )
    @ j = ( $i + 1 )
    set arg = "$argv[$i]"
    if("$arg" =~ [PpCcIiFfRrHh][1-6]*) then
        set temp = `echo $arg | awk '{print toupper($1)}'`
        set temp = `awk -v SG=$temp '$4 == SG {print $4}' $CLIBD/symop.lib | head -1`
        if("$temp" != "") then
            # add this SG to the space group list
            set SG = "$temp"
            continue
        endif
    endif
    
    if( "$arg" == "-grid" && ( $i + 3 <= $#argv )) then
        set GRID = "GRID"
        foreach x ( x y z )
            @ i = ( $i + 1 )
            set GRID = ( $GRID $argv[$i] )
        end
        continue
    endif

    if( "$arg" == "-cell" && ( $i + 6 <= $#argv )) then
        set CELL = ""
        foreach x ( a b c al be ga )
            @ i = ( $i + 1 )
            set CELL = ( $CELL $argv[$i] )
        end
        continue
    endif


end


cat << EOF >! ${tempfile}sfallme.pdb
ATOM      1  C   ALA     1       0.000   0.000   0.000  1.00 90.00
EOF

sfall xyzin ${tempfile}sfallme.pdb mapout ${tempfile}sfalled.map << EOF > $logfile
MODE ATMMAP 
CELL $CELL
SYMM $SG
$GRID
EOF

mapmask mapin ${tempfile}sfalled.map mapout ${tempfile}cell.map << EOF > $logfile
axis $AXIS
xyzlim cell
EOF

echo go | mapdump mapin ${tempfile}cell.map | tee ${tempfile}mapdump.log |\
awk '/Grid sampling on x, y, z/{gx=$8;gy=$9;gz=$10}\
     /Maximum density /{max=$NF}\
     /Cell dimensions /{xs=$4/gx;ys=$5/gy;zs=$6/gz}\
     /Number of columns, rows, sections/{nc=$7;nr=$8;ns=$9}\
 END{print xs,ys,zs,nc,nr,ns,max}' >! ${tempfile}mapstuff.txt

set voxels = `awk '{print $4*$5*$6}' ${tempfile}mapstuff.txt`
set size = `echo $voxels | awk '{print 4*$1}'`
set head = `ls -l ${tempfile}sfalled.map | awk -v size=$size '{print $5-size}'`
set skip = `echo $head | awk '{print $1+1}'`

head -c $head ${tempfile}cell.map >! $outfile

rm -f ${tempfile}sfalled.map
rm -f ${tempfile}cell.map
rm -f ${tempfile}mapstuff.txt
cat ${tempfile}mapdump.log
rm -f ${tempfile}mapstuff.txt

echo "now append $voxels 4-byte floats to $outfile ..."


exit:
if($?BAD) then
    echo "ERROR: $BAD"
    exit 9
endif


exit


#############################################################################
#############################################################################

