#! /bin/tcsh -f
#
#        perform some c function on an electron density map
#        using the "float_func.c" program                                -James Holton 7-30-17
#
#

# defaults
set mapfile1 = signal.map
set mapfile2 = noise.map
set func     = divide
set param    = ""
set outfile  = output.map

set tempfile = ${CCP4_SCR}/map_func$$
set logfile = /dev/null

set tempfile = tempfileasdf
#set logfile = /dev/tty
set logfile = debuglog.log
#echo -n "" >! $logfile

set i = 0
set user_maps = 0
while( $i < $#argv )
    @ i = ( $i + 1 )
    set arg = "$argv[$i]"
    
    if( "$arg" =~ *.map ) then
        @ user_maps = ( $user_maps + 1 )
        if($user_maps == 1) then
            set mapfile1 = "$arg"
            set mapfile2 = ""
        endif
        if($user_maps == 2) set mapfile2 = "$arg"
        if($user_maps == 3) set outfile  = "$arg"
    endif
    set test = `echo $arg | awk -F "=" '/^func/{print $2}'`
    if( "$test" != "" ) set func  = "$arg"
    set test = `echo $arg | awk -F "=" '/^func/{print $2}'`
    if( "$arg" =~ "-func" && $i < $#argv ) then
        @ i = ( $i + 1 )
        set func  = "$argv[$i]"
    endif
    if( "$arg" =~ "-param" && $i < $#argv ) then
        @ i = ( $i + 1 )
        set param  = "-param $argv[$i]"
    endif
end

if("$*" == "" || "$*" =~ "-h"* ) then
    cat << EOF
usage $0 signal.map [noise.map] -param 1 -func divide  [output.map]

where:
signal.map   is a CCP4 format electron density map
noise.map    is an optional second map for binary functions
param        a value to serve as the second map, or a third argument to a ternary function
func         one of:
       sqrt, cbrt, ceil, floor, fabs,
       sin, asin, sinh, asinh, 
       cos, acos, cosh, acosh,
       tan, atan, tanh, atanh,
       erf, erfc, exp, log, log10,
       j0, j1, jn, y0, y1, yn, gamma, lgamma,
       pow, erfpow where erfpow is erf(rho)^n
       norm  the normal distribution
       urand, grand, lrand, prand, erand, trand for uniform, gaussian, lorentz, poisson, exponential or triangle-random values
       set = for the output map to have all one value
       add, subtract, multiply, divide, inverse, negate,
       maximum, minimum, nanzero,
       thresh output 1 or 0 if first map is greather than second
EOF
    exit 9
endif

# synonyms
if("$func" == "mult") set func = "multiply"
if("$func" == "sub") set func = "subtract"

echo "selected $func function with parameters: $param"

if("$func" == "add" || "$func" == "subtract" || "$func" == "multiply" || "$func" == "divide" || "$func" == "max" || "$func" == "min"  || "$func" == "pow") then
    # two maps are expected?
else
    if($user_maps == 2) then
        set outfile = "$mapfile2"
        set mapfile2 = ""
    endif
endif

if(-x ./float_func) then
    set path = ( . $path )
endif

if(! -e "$mapfile1") then
    set BAD = "$mapfile1 does not exist."
    goto exit
endif

echo go | mapdump mapin $mapfile1 | tee ${tempfile}mapdump.log |\
awk '/Grid sampling on x, y, z/{gx=$8;gy=$9;gz=$10}\
     /Maximum density /{max=$NF}\
     /Cell dimensions /{xs=$4/gx;ys=$5/gy;zs=$6/gz}\
     /Number of columns, rows, sections/{nc=$7;nr=$8;ns=$9}\
 END{print xs,ys,zs,nc,nr,ns,max}' >! ${tempfile}mapstuff.txt
echo "$mapfile1 :"
egrep dens ${tempfile}mapdump.log
if(-e "$mapfile2") then
    echo "$mapfile2 :"
    echo go | mapdump mapin $mapfile2 | egrep dens
else
    if("$mapfile2" != "") then
        echo "WARNING: $mapfile2 does not exist."
    endif
endif

set voxels = `awk '{print $4*$5*$6}' ${tempfile}mapstuff.txt`
set size = `echo $voxels | awk '{print 4*$1}'`
set head = `ls -l $mapfile1 | awk -v size=$size '{print $5-size}'`
set skip = `echo $head | awk '{print $1+1}'`
rm -f ${tempfile}mapstuff.txt
rm -f ${tempfile}mapdump.log

if("$param" == "-param voxels") then
    echo "$voxels voxels"
    set power = `echo "$voxels" | awk '{printf("%d",$1)}'`
    set param = "-param $power"
endif

# now perform the function on the map data
func:
rm -f ${outfile} ${tempfile}output.bin
head -c $head $mapfile1 >! ${tempfile}temp.map
tail -c +$skip $mapfile1 >! ${tempfile}input1.bin
if(-e "$mapfile2") then
    tail -c +$skip $mapfile2 >! ${tempfile}input2.bin
    set map2 = ${tempfile}input2.bin
else
    set map2 = ""
endif
ls -l ${tempfile}input1.bin $map2 >& $logfile
echo "float_func ${tempfile}input1.bin $map2 ${tempfile}output.bin -func $func $param" >> $logfile
float_func ${tempfile}input1.bin $map2 -output ${tempfile}output.bin -func $func $param >> $logfile
if(-e ${tempfile}output.bin) then
    echo "success! "
    cat ${tempfile}output.bin >> ${tempfile}temp.map
    echo "scale factor 1 0" | mapmask mapin ${tempfile}temp.map mapout $outfile >> $logfile
    rm -f ${tempfile}output.bin
    set map2 = ""
    if(-e "$mapfile2") set map2 = ",$mapfile2"
    echo "$outfile = ${func}(${mapfile1}${map2}) "
else
    echo "ack! "
    if(! -e float_func.c) goto compile_float_func
    set BAD = "unable to run float_func"
    goto exit
endif
rm -f ${tempfile}temp.map
rm -f ${tempfile}input1.bin
rm -f ${tempfile}input2.bin
rm -f ${tempfile}output.bin


echo "$outfile :"
echo "go" | mapdump mapin $outfile | egrep density


exit:
if($?BAD) then
    echo "ERROR: $BAD"
    exit 9
endif


exit


#############################################################################
#############################################################################


compile_float_func:
echo "attempting to generate float_func utility ..."
cat << EOF >! float_func.c

/* apply a C function to each value in a raw "float" input file                -James Holton                9-21-15

example:

gcc -O -O -o float_func float_func.c -lm
./float_func -func erf snr.bin occ.bin 

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <byteswap.h>

char *infile1name = NULL;
char *infile2name = NULL;
FILE *infile1 = NULL;
FILE *infile2 = NULL;
char *outfilename = "output.bin\\0";
FILE *outfile = NULL;

typedef enum { UNKNOWN, SQRT, CBRT, CEIL, FLOOR, FABS,
       SIN, ASIN, SINH, ASINH, 
       COS, ACOS, COSH, ACOSH,
       TAN, ATAN, TANH, ATANH,
       ERF, ERFC, EXP, LOG, LOG10,
       J0, J1, JN, Y0, Y1, YN, GAMMA, LGAMMA,
        POW, ERFPOW,
       NORM,
       URAND, GRAND, LRAND, PRAND, ERAND, TRAND,
       SET,
       ADD, SUBTRACT, MULTIPLY, DIVIDE, INVERSE, NEGATE,
       MAXIMUM, MINIMUM, NANZERO,
       THRESH,
       FFT, INVFFT, REALFFT, INVREALFFT, AB2PHIF, PHIF2AB,
       SWAB2, SWAB4,
       ODD, EVEN, ODDEVEN, EVENODD } func_name;

/* random deviate with Poisson distribution */
float poidev(float xm, long *idum);
/* random deviate with Gaussian distribution */
float gaussdev(long *idum);
/* random deviate with Lorentzian distribution */
float lorentzdev(long *idum);
/* random deviate with triangle-shaped distribution */
float triangledev(long *idum);
/* random deviate with exponential distribution (>0) */
float expdev(long *idum);
/* random deviate with uniform distribution */
float ran1(long *idum);
/* FFT function */
float *Fourier(float *data, unsigned long length, int direction);

long seed;

int main(int argc, char** argv)
{
     
    int n,i,j,k,pixels;
    float *outimage;
    float *inimage1;
    float *inimage2;
    func_name func = UNKNOWN;
    float param=1.0;
    int user_param = 0, map_param = 0;
    float sum,sumd,sumsq,sumdsq,avg,rms,rmsd,min,max;
    int ignore_values=0,valid_pixels=0;
    float ignore_value[70000];
    unsigned short int *invalid_pixel;
    int16_t *twobytes;
    int32_t *fourbytes;
    float phi,F,a,b;

    /* check argument list */
    for(i=1; i<argc; ++i)
    {
        if(strlen(argv[i]) > 4)
        {
            if(strstr(argv[i]+strlen(argv[i])-4,".bin"))
            {
//                printf("filename: %s\\n",argv[i]);
                if(infile1name == NULL){
                    infile1name = argv[i];
                }
                else
                {
                    if(infile2name == NULL){
                        infile2name = argv[i];
                    }
                    else
                    {
                        outfilename = argv[i];
                    }
                }
            }
        }

        if(argv[i][0] == '-')
        {
            /* option specified */
            if(strstr(argv[i], "-func") && (argc >= (i+1)))
            {
		func = UNKNOWN;
                if(strstr(argv[i+1],"sqrt")) func = SQRT;
                if(strstr(argv[i+1],"cbrt")) func = CBRT;
                if(strstr(argv[i+1],"ceil")) func = CEIL;
                if(strstr(argv[i+1],"floor")) func = FLOOR;
                if(strstr(argv[i+1],"abs")) func = FABS;
                if(strstr(argv[i+1],"fabs")) func = FABS;
                if(strstr(argv[i+1],"sin")) func = SIN;
                if(strstr(argv[i+1],"asin")) func = ASIN;
                if(strstr(argv[i+1],"sinh")) func = SINH;
                if(strstr(argv[i+1],"asinh")) func = ASINH;
                if(strstr(argv[i+1],"cos")) func = COS;
                if(strstr(argv[i+1],"acos")) func = ACOS;
                if(strstr(argv[i+1],"cosh")) func = COSH;
                if(strstr(argv[i+1],"acosh")) func = ACOSH;
                if(strstr(argv[i+1],"tan")) func = TAN;
                if(strstr(argv[i+1],"atan")) func = ATAN;
                if(strstr(argv[i+1],"tanh")) func = SINH;
                if(strstr(argv[i+1],"atanh")) func = ATANH;
                if(strstr(argv[i+1],"erf")) func = ERF;
                if(strstr(argv[i+1],"erfc")) func = ERFC;
                if(strstr(argv[i+1],"pow")) func = POW;
                if(strstr(argv[i+1],"erfpow")) func = ERFPOW;
                if(strstr(argv[i+1],"exp")) func = EXP;
                if(strstr(argv[i+1],"log")) func = LOG;
                if(strstr(argv[i+1],"log10")) func = LOG10;
                if(strstr(argv[i+1],"j0")) func = J0;
                if(strstr(argv[i+1],"j1")) func = J1;
                if(strstr(argv[i+1],"jn")) func = JN;
                if(strstr(argv[i+1],"y0")) func = Y0;
                if(strstr(argv[i+1],"y1")) func = Y1;
                if(strstr(argv[i+1],"yn")) func = YN;
                if(strstr(argv[i+1],"gamma")) func = GAMMA;
                if(strstr(argv[i+1],"lgamma")) func = LGAMMA;
                if(strstr(argv[i+1],"norm")) func = NORM;
                if(strstr(argv[i+1],"urand")) func = URAND;
                if(strstr(argv[i+1],"grand")) func = GRAND;
                if(strstr(argv[i+1],"lrand")) func = LRAND;
                if(strstr(argv[i+1],"prand")) func = PRAND;
                if(strstr(argv[i+1],"erand")) func = ERAND;
                if(strstr(argv[i+1],"trand")) func = TRAND;
                if(strstr(argv[i+1],"zero")) {   func = SET; param = 0.0; user_param=1;};
                if(strstr(argv[i+1],"one")) {    func = SET; param = 1.0; user_param=1;};
                if(strstr(argv[i+1],"unity")) {  func = SET; param = 1.0; user_param=1;};
                if(strstr(argv[i+1],"set"))      func = SET;
                if(strstr(argv[i+1],"const"))    func = SET;
                if(strstr(argv[i+1],"constant")) func = SET;
                if(strstr(argv[i+1],"add"))      func = ADD;
                if(strstr(argv[i+1],"subtract")) func = SUBTRACT;
                if(strstr(argv[i+1],"multiply")) func = MULTIPLY;
                if(strstr(argv[i+1],"mult"))     func = MULTIPLY;
                if(strstr(argv[i+1],"divide"))   func = DIVIDE;
                if(strstr(argv[i+1],"div"))      func = DIVIDE;
                if(strstr(argv[i+1],"inverse"))  func = INVERSE;
                if(strstr(argv[i+1],"inv"))      func = INVERSE;
                if(strstr(argv[i+1],"negative")) func = NEGATE;
                if(strstr(argv[i+1],"negate"))   func = NEGATE;
                if(strstr(argv[i+1],"max"))      func = MAXIMUM;
                if(strstr(argv[i+1],"maximum"))  func = MAXIMUM;
                if(strstr(argv[i+1],"min"))      func = MINIMUM;
                if(strstr(argv[i+1],"minimum"))  func = MINIMUM;
                if(strstr(argv[i+1],"nanzero"))  func = NANZERO;
                if(strstr(argv[i+1],"thresh"))   func = THRESH;
                if(strstr(argv[i+1],"fft"))      func = FFT;
                if(strstr(argv[i+1],"invfft"))   func = INVFFT;
                if(strstr(argv[i+1],"realfft"))  func = REALFFT;
                if(strstr(argv[i+1],"invrealfft")) func = INVREALFFT;
                if(strstr(argv[i+1],"ab2phif"))   func = AB2PHIF;
                if(strstr(argv[i+1],"phif2ab"))   func = PHIF2AB;
                if(strstr(argv[i+1],"swab"))     func = SWAB4;
                if(strstr(argv[i+1],"swab2"))    func = SWAB2;
                if(strstr(argv[i+1],"swap"))     func = SWAB4;
                if(strstr(argv[i+1],"swap2"))    func = SWAB2;
                if(strstr(argv[i+1],"odd"))      func = ODD;
                if(strstr(argv[i+1],"even"))     func = EVEN;
                if(strstr(argv[i+1],"oddeven"))  func = ODDEVEN;
                if(strstr(argv[i+1],"evenodd"))  func = EVENODD;
                if(strstr(argv[i+1],"+")) func = ADD;
                if(strstr(argv[i+1],"-")) func = SUBTRACT;
                if(strstr(argv[i+1],"*")) func = MULTIPLY;
                if(strstr(argv[i+1],"/")) func = DIVIDE;
                if(strstr(argv[i+1],"1/")) {func = INVERSE ; param = 1.0; user_param=1;};
                if(strstr(argv[i+1],"1-")) {func = NEGATE ; param = 1.0; user_param=1;};
		if(func == UNKNOWN) printf("WARNING: unknown function: %s\\n",argv[i+1]);
            }
            if(strstr(argv[i], "-param") && (argc >= (i+1)))
            {
                param = atof(argv[i+1]);
		 user_param=1;
            }
            if(strstr(argv[i], "-ignore") && (argc >= (i+1)))
            {
                ++ignore_values;
                ignore_value[ignore_values] = atof(argv[i+1]);
            }
            if(strstr(argv[i], "-seed") && (argc >= (i+1)))
            {
                seed = -atoi(argv[i+1]);
            }
            if(strstr(argv[i], "-out") && (argc >= (i+1)))
            {
                outfilename = argv[i+1];
            }
	    ++i;
        }
    }

    infile1 = fopen(infile1name,"r");
    if(infile1 == NULL){
            printf("ERROR: unable to open %s as input 1\\n", infile1name);
        perror("");
    }
    if(infile1 == NULL || func == UNKNOWN){
	printf("usage: float_func file.bin [file2.bin] [outfile.bin] -func jn -param 1.0 -ignore 0\\n");
        printf("options:\\n");
        printf("\\tfile.bin\\t binary file containing the arguments to the desired function\\n");
	printf("\\tfile2.bin\\t binary file containing second argument to the desired function\\n");
        printf("\\t-func\\t may be one of:\\n");
        printf("\\t sqrt cbrt ceil floor abs \\n");
        printf("\\t zero one set (one output value)\\n");
        printf("\\t add subtract multiply divide inverse negate maximum minimum \\n");
        printf("\\t thresh (1-or-0 threshold)\\n");
        printf("\\t nanzero (set all NaN and Inf values to zero)\\n");
        printf("\\t sin asin sinh asinh cos acos cosh acosh tan atan tanh atanh (trigonometry)\\n");
        printf("\\t pow erf erfpow norm erfc (power and error functions)\\n");
        printf("\\t exp log log10 (natural or base-10 log)\\n");
        printf("\\t j0 j1 jn y0 y1 yn gamma lgamma (Bessel and gamma functions)\\n");
        printf("\\t urand grand lrand prand erand trand (uniform gaussian lorentzian poisson exponential or triangle randomness)\\n");
        printf("\\t fft invfft realfft invrealfft (complex or real FFT - base 2)\\n");
        printf("\\t ab2phif phif2ab (cartesian to amplitude-phase conversion)\\n");
        printf("\\t swab4 swab2 (swap bytes)\\n");
        printf("\\t odd even oddeven evenodd (extract or interleave values)\\n");
        printf("\\t-param\\t second value for add, subtract, set, etc. or parameter needed by the function (such as jn)\\n");
        printf("\\t-seed\\t seed for random number functions\\n");
        printf("\\t-ignore\\t value found in either input file will pass through\\n");
        printf("\\toutput.bin\\t output binary file containing the results to the desired function\\n");
        exit(9);
    }

    printf("selected function: ");
    switch(func){
        case AB2PHIF: printf("AB2PHIF\\n"); break;
        case PHIF2AB: printf("PHIF2AB\\n"); break;
        case ACOS: printf("ACOS\\n"); break;
        case ACOSH: printf("ACOSH\\n"); break;
        case ADD: printf("ADD\\n"); break;
        case ASIN: printf("ASIN\\n"); break;
        case ASINH: printf("ASINH\\n"); break;
        case ATAN: printf("ATAN\\n"); break;
        case ATANH: printf("ATANH\\n"); break;
        case CBRT: printf("CBRT\\n"); break;
        case CEIL: printf("CEIL\\n"); break;
        case COS: printf("COS\\n"); break;
        case COSH: printf("COSH\\n"); break;
        case DIVIDE: printf("DIVIDE\\n"); break;
        case ERAND: printf("ERAND\\n"); break;
        case ERF: printf("ERF\\n"); break;
        case ERFC: printf("ERFC\\n"); break;
        case ERFPOW: printf("ERFPOW\\n"); break;
        case EVEN: printf("EVEN\\n"); break;
        case EXP: printf("EXP\\n"); break;
        case FABS: printf("FABS\\n"); break;
        case FFT: printf("FFT\\n"); break;
        case INVFFT: printf("INVFFT\\n"); break;
        case REALFFT: printf("REALFFT\\n"); break;
        case INVREALFFT: printf("INVREALFFT\\n"); break;
        case FLOOR: printf("FLOOR\\n"); break;
        case GAMMA: printf("GAMMA\\n"); break;
        case GRAND: printf("GRAND\\n"); break;
        case INVERSE: printf("INVERSE\\n"); break;
        case J0: printf("J0\\n"); break;
        case J1: printf("J1\\n"); break;
        case JN: printf("JN\\n"); break;
        case LGAMMA: printf("LGAMMA\\n"); break;
        case LOG: printf("LOG\\n"); break;
        case LOG10: printf("LOG10\\n"); break;
        case LRAND: printf("LRAND\\n"); break;
        case MAXIMUM: printf("MAXIMUM\\n"); break;
        case MINIMUM: printf("MINIMUM\\n"); break;
        case MULTIPLY: printf("MULTIPLY\\n"); break;
        case NANZERO: printf("NANZERO\\n"); break;
        case NEGATE: printf("NEGATE\\n"); break;
        case NORM: printf("NORM\\n"); break;
        case ODD: printf("ODD\\n"); break;
        case ODDEVEN: printf("ODDEVEN\\n"); break;
        case EVENODD: printf("EVENODD\\n"); break;
        case POW: printf("POW\\n"); break;
        case PRAND: printf("PRAND\\n"); break;
        case SET: printf("SET\\n"); break;
        case SIN: printf("SIN\\n"); break;
        case SINH: printf("SINH\\n"); break;
        case SQRT: printf("SQRT\\n"); break;
        case SUBTRACT: printf("SUBTRACT\\n"); break;
        case SWAB2: printf("SWAB2\\n"); break;
        case SWAB4: printf("SWAB4\\n"); break;
        case TAN: printf("TAN\\n"); break;
        case THRESH: printf("THRESH\\n"); break;
        case TRAND: printf("TRAND\\n"); break;
        case UNKNOWN: printf("UNKNOWN\\n"); break;
        case URAND: printf("URAND\\n"); break;
        case Y0: printf("Y0\\n"); break;
        case Y1: printf("Y1\\n"); break;
        case YN: printf("YN\\n"); break;
    }
/*
awk '/func = / && \$NF !~ /}/{print substr(\$NF,1,length(\$NF)-1);}' ~/Develop/src/float_stuff/float_func.c |\\
 sort -u | awk '{ print "\\tcase "\$1": printf(\\""\$1"\\\\n\\"); break;"}'
*/

    for(k=1;k<=ignore_values;++k)
    {
	printf("ignoring value %g in both files\\n",ignore_value[k]);
    }

    /* load first float-image */
    fseek(infile1,0,SEEK_END);
    n = ftell(infile1);
    rewind(infile1);
    inimage1 = calloc(n+10,1);
    invalid_pixel = calloc(n+10,1);
    printf("reading %d floats from %s\\n",n/sizeof(float),infile1name);
    fread(inimage1,n,1,infile1);
    fclose(infile1);

    pixels = n/sizeof(float);

    /* if the function takes two args ... */
    if( func == ADD || func == SUBTRACT || func == MULTIPLY || func == DIVIDE || func == POW ||
        func == MAXIMUM || func == MINIMUM || func == THRESH ||
        func == EVENODD || func == ODDEVEN ) {
        if(infile2name == NULL || user_param )
        {
            /* use the "param" as the second value */
            map_param = 0;
        }
        else
        {
            map_param = 1;
        }
    }
    else
    {
        if(infile2name != NULL)
        {
            /* second filename must be the output file */
            outfilename = infile2name;
            infile2name = NULL;
        }
    }

    /* open second file, if it was specified */
    if(infile2name != NULL){
        infile2 = fopen(infile2name,"r");
        if(infile2 == NULL){
            printf("ERROR: unable to open %s as input 2\\n", infile2name);
            perror("");
            exit(9);
        }            
            inimage2 = calloc(n,1);
            fread(inimage2,n,1,infile2);
            fclose(infile2);
    }

    outfile = fopen(outfilename,"w");
    if(outfile == NULL)
    {
        printf("ERROR: unable to open %s for output\\n", outfilename);
        perror("");
        exit(9);
    }
    printf("input1 is: %s\\n",infile1name);
    if(map_param){
    printf("input2 is: %s\\n",infile2name);
    }else{
        printf("input2 is: %g\\n",param);
    }
    printf("output to: %s\\n",outfilename);
    
    /* just to keep track */
    outimage = NULL;
    if( func == FFT || func == INVFFT || func == REALFFT )
    {
	/* must be a power of two */
	valid_pixels = (int) ceil(pow(2.0,floor(log(pixels)/log(2))));
	if(valid_pixels != pixels){
	    pixels = valid_pixels;
	    printf("truncating data to %d floats\\n",pixels);
	}
	valid_pixels = 0;
    }
    if( func == INVREALFFT )
    {
	/* must be a power of two plus 2 */
	valid_pixels = 2+(int) ceil(pow(2.0,floor(log(pixels-2)/log(2))));
	if(valid_pixels != pixels){
	    pixels = valid_pixels;
	    printf("truncating data to %d floats\\n",pixels);
	}
	valid_pixels = 0;
    }

    if( func == FFT ){
	/* assume input array is complex numbers (even=real, odd=imag) */
	/* fortranish, so zeroeth item will not be touched */
        outimage = Fourier(inimage1-1, pixels/2, 1);
	/* correct for fortranish */
	++outimage;
	/* correct coefficients so that they back-transform on the same scale */
	for(i=0;i<pixels;++i)
	{
	    outimage[i]/=(pixels/2);
	}
    }
    if( func == INVFFT ){
	/* assume input array is complex numbers (even=real, odd=imag) */
	/* fortranish, so zeroeth item will not be touched */
        outimage = Fourier(inimage1-1, pixels/2, -1);
	/* correct for fortranish */
	++outimage;
    }
    if( func == REALFFT ){
	/* need to convert to complex numbers */
	outimage = calloc(pixels+2,2*sizeof(float));
	for(i=0;i<pixels;++i)
	{
	    j = 2*i;
	    outimage[j] = inimage1[i]/(pixels);
	}
	/* output will be twice the size */
	/* fortranish, so zeroeth item will not be touched */
        outimage = Fourier(outimage-1, pixels, 1);
	/* correct for fortranish */
	++outimage;
	/* but we are only interested in first half (plus one more for the Nyquist frequency) */
	pixels=pixels+2;
    }
    if( func == INVREALFFT ){
	pixels-=2;
	outimage = calloc(pixels,2*sizeof(float));
	/* compensate for zero-value negative index coefficients by doubling non-trivial ones */
	outimage[0]=inimage1[0];
	outimage[pixels]=inimage1[pixels];
	for(i=1;i<pixels;++i)
	{
	    outimage[i] = 2*inimage1[i];
	}
	/* fortranish, so zeroeth item will not be touched */
        outimage = Fourier(outimage-1, pixels, -1);
	/* correct for fortranish */
	++outimage;
	/* but we are only interested in even numbered ones (real part) */
	for(i=0;i<pixels;++i)
	{
	    outimage[i] = outimage[2*i];
	}
    }
    if( func == ODD || func == EVEN ){
	/* output will be half the size */
	pixels = pixels/2;
    }
    if( func == ODDEVEN || func == EVENODD ){
	/* interleaving values, so output will be twice the size */
	outimage = calloc(2*pixels,sizeof(float));
    }
    if( func == AB2PHIF || func == PHIF2AB ){
	/* values of interest are two floats */
	outimage = calloc(pixels,sizeof(float));
	pixels = pixels/2;
	/* we will restore true float-count before output */
    }

    
    /* see if we need to allocate memory for output image */
    if( outimage == NULL ) {
printf("allocating %d pixels\\n",pixels);
        outimage = calloc(pixels+10,sizeof(float));
    }

    sum = sumsq = sumd = sumdsq = 0.0;
    min = 1e99;max=-1e99;
    for(i=0;i<pixels;++i)
    {
	/* skip any invalid values, propagate to output */
	for(k=1;k<=ignore_values;++k)
        {
	    if(inimage1[i]==ignore_value[k] || inimage2[i]==ignore_value[k]){
		outimage[i] = ignore_value[k];
	        ++invalid_pixel[i];
		/* no need to check others */
		break;
	    }
        }
	/* skip all calcs for this one */
	if(invalid_pixel[i]) continue;

        if( func == AB2PHIF ){
	    j=i*2;
	    a = inimage1[j];
	    b = inimage1[j+1];
	    F = sqrtf(a*a+b*b);
	    phi = atan2(b,a);
            outimage[j] = F;
            outimage[j+1] = phi;
        }
        if( func == PHIF2AB ){
	    j=i*2;
	    F = inimage1[j];
	    phi = inimage1[j+1];
	    a = F*cos(phi);
	    b = F*sin(phi);
            outimage[j] = a;
            outimage[j+1] = b;
        }

        if( func == SQRT ){
            outimage[i] = sqrtf(inimage1[i]);
        }
        if( func == CBRT ){
            outimage[i] = cbrtf(inimage1[i]);
        }
        if( func == CEIL ){
            outimage[i] = ceilf(inimage1[i]);
        }
        if( func == FLOOR ){
            outimage[i] = floorf(inimage1[i]);
        }
        if( func == FABS ){
            outimage[i] = fabsf(inimage1[i]);
        }
        if( func == SIN ){
            outimage[i] = sinf(inimage1[i]);
        }
        if( func == ASIN ){
            outimage[i] = asinf(inimage1[i]);
        }
        if( func == SINH ){
            outimage[i] = sinhf(inimage1[i]);
        }
        if( func == ASINH ){
            outimage[i] = asinhf(inimage1[i]);
        }
        if( func == COS ){
            outimage[i] = cosf(inimage1[i]);
        }
        if( func == ACOS ){
            outimage[i] = acosf(inimage1[i]);
        }
        if( func == COSH ){
            outimage[i] = coshf(inimage1[i]);
        }
        if( func == ACOSH ){
            outimage[i] = acoshf(inimage1[i]);
        }
        if( func == TAN ){
            outimage[i] = tanf(inimage1[i]);
        }
        if( func == ATAN ){
            outimage[i] = atanf(inimage1[i]);
        }
        if( func == TANH ){
            outimage[i] = tanhf(inimage1[i]);
        }
        if( func == ATANH ){
            outimage[i] = atanhf(inimage1[i]);
        }
        if( func == ERF ){
            outimage[i] = erff(inimage1[i]);
        }
        if( func == ERFC ){
            outimage[i] = erfcf(inimage1[i]);
        }
        if( func == EXP ){
            outimage[i] = expf(inimage1[i]);
        }
        if( func == LOG ){
            outimage[i] = logf(inimage1[i]);
        }
        if( func == LOG10 ){
            outimage[i] = log10f(inimage1[i]);
        }
        if( func == J0 ){
            outimage[i] = j0f(inimage1[i]);
        }
        if( func == J1 ){
            outimage[i] = j1f(inimage1[i]);
        }
        if( func == JN ){
            if(map_param) param=inimage2[i];
            outimage[i] = jnf((int) param,inimage1[i]);
        }
        if( func == Y0 ){
            outimage[i] = y0f(inimage1[i]);
        }
        if( func == Y1 ){
            outimage[i] = y1f(inimage1[i]);
        }
        if( func == YN ){
            if(map_param) param=inimage2[i];
            outimage[i] = ynf((int) param,inimage1[i]);
        }
        if( func == GAMMA ){
            outimage[i] = gammaf(inimage1[i]);
        }
        if( func == LGAMMA ){
            outimage[i] = lgammaf(inimage1[i]);
        }
        if( func == POW ){
            if(map_param) param=inimage2[i];
            outimage[i] = pow(inimage1[i],param);
        }
        if( func == ERFPOW ){
            outimage[i] = (float) pow(erf(inimage1[i]),param);
        }
        if( func == NORM ){
            outimage[i] = erff(inimage1[i]/sqrt(2.0))/2.0+0.5;
        }
        if( func == URAND ){
            outimage[i] = ran1(&seed);
        }
        if( func == GRAND ){
            outimage[i] = gaussdev(&seed);
        }
        if( func == LRAND ){
            outimage[i] = lorentzdev(&seed);
        }
        if( func == PRAND ){
            outimage[i] = poidev(fabsf(inimage1[i]),&seed);
        }
        if( func == ERAND ){
            outimage[i] = expdev(&seed);
        }
        if( func == ERAND ){
            outimage[i] = triangledev(&seed);
        }
        if( func == SET ){
            outimage[i] = param;
        }
        if( func == ADD ){
            if(map_param) param=inimage2[i];
            outimage[i] = inimage1[i]+param;
        }
        if( func == SUBTRACT ){
            if(map_param) param=inimage2[i];
            outimage[i] = inimage1[i]-param;
        }
        if( func == MULTIPLY ){
            if(map_param) param=inimage2[i];
            outimage[i] = inimage1[i]*param;
        }
        if( func == DIVIDE ){
            if(map_param) param=inimage2[i];
            outimage[i] = inimage1[i]/param;
        }
        if( func == INVERSE ){
            if(map_param) param=inimage2[i];
            outimage[i] = param/inimage1[i];
        }
        if( func == NEGATE ){
            if(map_param) param=inimage2[i];
            outimage[i] = param-inimage1[i];
        }
        if( func == MAXIMUM ){
            if(map_param) param=inimage2[i];
            outimage[i] = inimage1[i];
            if(outimage[i]<param) outimage[i]=param;
        }
        if( func == MINIMUM ){
            if(map_param) param=inimage2[i];
            outimage[i] = inimage1[i];
            if(outimage[i]>param) outimage[i]=param;
        }
        if( func == NANZERO ){
            outimage[i] = inimage1[i];
            if(isnan(outimage[i]) || isinf(outimage[i])) outimage[i]=0.0;
        }
        if( func == THRESH ){
            if(map_param) param=inimage2[i];
            outimage[i] = 1.0;
            if(inimage1[i]<param) outimage[i]=0.0;
        }
        if( func == SWAB4 ){
             outimage[i] = inimage1[i];
             fourbytes = (int32_t *) & outimage[i];
             *fourbytes = bswap_32(*fourbytes);
        }
        if( func == SWAB2 ){
             outimage[i] = inimage1[i];
             twobytes = (int16_t *) & outimage[i];
             *twobytes = bswap_16(*twobytes);
        }
        if( func == ODD ){
	    /* skip even pixels, for stats */
	    j = 2*i+1;
            outimage[i] = inimage1[j];
        }
        if( func == EVEN ){
	    /* skip odd pixels, for stats */
	    j = 2*i;
            outimage[i] = inimage1[j];
        }
        if( func == EVENODD ){
	    j = 2*i;
            if(map_param) param=inimage2[i];
            outimage[j]   = inimage1[i];
            outimage[j+1] = param;
        }
        if( func == ODDEVEN ){
	    j = 2*i;
            if(map_param) param=inimage2[i];
            outimage[j]   = param;
            outimage[j+1] = inimage1[i];
        }
        if(outimage[i]>max) max=outimage[i];
        if(outimage[i]<min) min=outimage[i];
        sum += outimage[i];
        sumsq += outimage[i]*outimage[i];
    }
    if( func == AB2PHIF || func == PHIF2AB || func == EVENODD || func == ODDEVEN )
    {
	pixels = 2*pixels;
    }

    avg = sum/pixels;
    rms = sqrt(sumsq/pixels);
    if(ignore_values) printf("%d invalid of ",pixels-valid_pixels);
    printf("%d pixels ",pixels);
    if(ignore_values) printf("( %d left)",valid_pixels);
    printf("\\n");
    for(i=0;i<pixels;++i)
    {
        sumd   += outimage[i] - avg;
        sumdsq += (outimage[i] - avg) * (outimage[i] - avg);
    }
    rmsd = sqrt(sumdsq/pixels);
    printf("max = %g min = %g\\n",max,min);
    printf("mean = %g rms = %g rmsd = %g\\n",avg,rms,rmsd);


    printf("writing %s as %d %d-byte floats\\n",outfilename,pixels,sizeof(float));
    outfile = fopen(outfilename,"w");
    fwrite(outimage,pixels,sizeof(float),outfile);
    fclose(outfile);


    return;
}



float poidev(float xm, long *idum)
{
    float gammln(float xx);
    float ran1(long *idum);
    /* oldm is a flag for whether xm has changed since last call */
    static float sq,alxm,g,oldm=(-1.0);
    float em,t,y;
        
    if (xm < 12.0) {
        /* use direct method: simulate exponential delays between events */
        if(xm != oldm) {
            /* xm is new, compute the exponential */
            oldm=xm;
            g=exp(-xm);
        }
        /* adding exponential deviates is equivalent to multiplying uniform deviates */
        /* final comparison is to the pre-computed exponential */
        em = -1;
        t = 1.0;
        do {
            ++em;
            t *= ran1(idum);
        } while (t > g);
    } else {
        /* Use rejection method */
        if(xm != oldm) {
            /* xm has changed, pre-compute a few things... */
            oldm=xm;
            sq=sqrt(2.0*xm);
            alxm=log(xm);
            g=xm*alxm-gammln(xm+1.0);
        }
        do {
            do {
                /* y is a deviate from a lorentzian comparison function */
                y=tan(M_PI*ran1(idum));
                /* shift and scale */
                em=sq*y+xm;
            } while (em < 0.0);                /* there are no negative Poisson deviates */
            /* round off to nearest integer */
            em=floor(em);
            /* ratio of Poisson distribution to comparison function */
            /* scale it back by 0.9 to make sure t is never > 1.0 */
            t=0.9*(1.0+y*y)*exp(em*alxm-gammln(em+1.0)-g);
        } while (ran1(idum) > t);
    }
        
    return em;
}


/* return gaussian deviate with rms=1 and FWHM = 2/sqrt(log(2)) */
float gaussdev(long *idum)
{
    float ran1(long *idum);
    static int iset=0;
    static float gset;
    float fac,rsq,v1,v2;
        
    if (iset == 0) {
        /* no extra deviats handy ... */
        
        /* so pick two uniform deviates on [-1:1] */
        do {
            v1=2.0*ran1(idum)-1.0;
            v2=2.0*ran1(idum)-1.0;
            rsq=v1*v1+v2*v2;
        } while (rsq >= 1.0 || rsq == 0);
        /* restrained to the unit circle */
        
        /* apply Box-Muller transformation to convert to a normal deviate */
        fac=sqrt(-2.0*log(rsq)/rsq);
        gset=v1*fac;
        iset=1;                /* we now have a spare deviate */
        return v2*fac;
    } else {
        /* there is an extra deviate in gset */
        iset=0;
        return gset;
    }
}


/* generate Lorentzian deviate with FWHM = 2 */
float lorentzdev(long *seed) {
    float ran1(long *idum);
 
    return tan(M_PI*(ran1(seed)-0.5));
}

/* return triangular deviate with FWHM = 1 */
float triangledev(long *seed) {
    float ran1(long *idum);
    float value;

    value = ran1(seed);
    if(value > 0.5){
        value = sqrt(2*(value-0.5))-1;
    }else{
        value = 1-sqrt(2*value);
    }

    return value;
}



float expdev(long *idum)
{
    float dum;
    
    do
    dum=ran1(idum);
    while( dum == 0.0);
    return -log(dum);
}



/* ln of the gamma function */
float gammln(float xx)
{
    double x,y,tmp,ser;
    static double cof[6]={76.18009172947146,-86.50532032941677,
    24.01409824083091,-1.231739572450155,
    0.1208650973866179e-2,-0.5395239384953e-5};
    int j;
    
    y=x=xx;
    tmp=x+5.5;
    tmp -= (x+0.5)*log(tmp);
    ser = 1.000000000190015;
    for(j=0;j<=5;++j) ser += cof[j]/++y;
    
    return -tmp+log(2.5066282746310005*ser/x);
}





/* returns a uniform random deviate between 0 and 1 */
#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV (1+(IM-1)/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

float ran1(long *idum)
{
    int j;
    long k;
    static long iy=0;
    static long iv[NTAB];
    float temp;
    
    if (*idum <= 0 || !iy) {
        /* first time around.  don't want idum=0 */
        if(-(*idum) < 1) *idum=1;
        else *idum = -(*idum);
        
        /* load the shuffle table */
        for(j=NTAB+7;j>=0;j--) {
            k=(*idum)/IQ;
            *idum=IA*(*idum-k*IQ)-IR*k;
            if(*idum < 0) *idum += IM;
            if(j < NTAB) iv[j] = *idum;
        }
        iy=iv[0];
    }
    /* always start here after initializing */
    k=(*idum)/IQ;
    *idum=IA*(*idum-k*IQ)-IR*k;
    if (*idum < 0) *idum += IM;
    j=iy/NDIV;
    iy=iv[j];
    iv[j] = *idum;
    if((temp=AM*iy) > RNMX) return RNMX;
    else return temp;
}




#define SWAP(a,b) swp_temp=(a);(a)=(b);(b)=swp_temp



/********************************************************************
*
*	Fourier()	Numerical Recipes's Fast Fourier Transform
*
*********************************************************************
*
*   local variables:
*	swp_temp		- used by SWAP macro
*
*	i1, i2			- indicies used in bit-reversal
*	data_length		- size (in floats) of the data buffer
*	new_FFT_len		- length (in cplx) of the next sub-FFT
*	last_FFT_size 	- size (in floats) of the last sub-FFT
*	sub_FFT_spacing - spacing between adjacent sub-FFTs
*
*   FFT_idx 		- index along sub-FFT
*	evn_idx			- index of even-data Fourier coefficient
*	odd_idx			- index of odd-data Fourier coefficient
*
*	w_temp
*	w_real, w_imag	- the complex value of the basis wave
*	w_p_real        - previous ''
*   w_p_imag
*	theta			- argument to complex exponential
*	temp_real		- temporary complex number
*	temp_imag
*
*	Description:
*		This function computes the Fast Fourier Transform of the
*	complex data represented in data[].  Odd indicies (starting
*	with 1) are the real values, and even indicies (starting with
*	2) are imaginaries.  Note the Fortranish array indexing.
*		The Fourier spectrum is returned as a series of similar
*	complex coefficients.  The first pair ([1],[2]) is the coefficient
*	of zero-frequency.  Higher and higher positive frequencies are
*	stored in the higher indexed pairs.  The Nyquist frequency
*	coefficient (which is the same for both positive and negative
*	frequencies) is stored at data[length+1]. Progressively lower
*	(more positive) negative frequencies are entered until
*	data[length+2];
*		It is not recommended to call this particular function
*	directly, but if you do, be sure to make length a power of
*	two, and pass your data pointer as data-1.
*
********************************************************************/

float *Fourier(float *data, unsigned long length, int direction)
{
	float swp_temp;

	unsigned long i1, i2, temp_int;

	unsigned long data_length;
	unsigned long new_FFT_len, last_FFT_size, sub_FFT_spacing;
	unsigned long FFT_idx, evn_idx, odd_idx;

	double w_temp, w_real, w_imag, w_p_real, w_p_imag, theta;
	double temp_real, temp_imag;

	/* data size is 2 * complex numbers */
	data_length = 2*length;
	i2 = 1;

	/* reorganize data in bit-reversed order */
	for(i1 = 1; i1 < data_length; i1 += 2)
	{
		/* swap if appropriate */
		if (i2 > i1)
		{
			SWAP(data[i2], data[i1]);
			SWAP(data[i2+1], data[i1+1]);
		}

		/* calculate bit-reverse of index1 in index2 */
		temp_int = data_length/2;
		while ((temp_int >= 2)&&(i2 > temp_int))
		{
			i2 -= temp_int;
			temp_int >>= 1;
		}
		i2 += temp_int;
	}

	/* FFTs of length 1 have been "calculated" and organized so
	   that the odd and even Fourier coefficents are grouped */

	/* first sub-FFT is of length 2 */
	last_FFT_size = (new_FFT_len = 2);

	/* for as long as sub-FFT is not full FFT */
	while(data_length > new_FFT_len)
	{
		/* separation of previous sub-FFT coefficients */
		sub_FFT_spacing = 2*last_FFT_size;

		/* this is a trig recurrence relation that will yield
		   the W number in the Danielson-Lanczos Lemma */
		theta = direction*(2*M_PI/new_FFT_len);
		w_temp = sin(0.5*theta);

		w_p_real = -2.0*w_temp*w_temp;
		w_p_imag = sin(theta);

		/* initialize W number */
		w_real = 1.0;
		w_imag = 0.0;

		/* recursively combine the sub-FFTs */
		for (FFT_idx = 1; FFT_idx < last_FFT_size; FFT_idx += 2)
		{
			for (evn_idx = FFT_idx;
				 evn_idx <= data_length;
				 evn_idx += sub_FFT_spacing)
			{
				/* FFT coefficients to combine */
				odd_idx = evn_idx + last_FFT_size;

				/* calculate W*F(odd) */
				temp_real = w_real*data[odd_idx] - w_imag*data[odd_idx+1];
				temp_imag = w_real*data[odd_idx+1] + w_imag*data[odd_idx];

				/* complete Lemma: F = F(even) + W*F(odd) */
				/* since F(even) and F(odd) are pereodic... */
				data[odd_idx] = data[evn_idx] - temp_real;
				data[odd_idx+1] = data[evn_idx+1] - temp_imag;

				/* "regular D/L here" */
				data[evn_idx] += temp_real;
				data[evn_idx+1] += temp_imag;
			}
			/* now calculate the next W */
			w_temp = w_real;
			w_real = w_temp*w_p_real - w_imag*w_p_imag + w_real;
			w_imag = w_imag*w_p_real + w_temp*w_p_imag + w_imag;
		}
		/* prepare to combine next FFT */
		new_FFT_len = (last_FFT_size = sub_FFT_spacing);
	}

	return data;
}

EOF
gcc -o float_func float_func.c -lm 
set path = ( . $path )
goto func


