
/* convert binary float file to one of the ints                                           -James Holton  9-20-16

example:

gcc -O -O -o float2int float2int.c -lm
./float2int floatfile.bin file1.img 

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>


char *infile1name = "";
FILE *infile1 = NULL;
char *outfilename = "intfile.bin\0";
FILE *outfile = NULL;

int main(int argc, char** argv)
{

    int n,i,j,k,pixels;
    float *inimage1;
    void *outimage;
    int8_t   *dint8outimage;
    int16_t  *dint16outimage;
    int32_t  *dint32outimage;
    uint8_t  *uint8outimage;
    uint16_t *uint16outimage;
    uint32_t *uint32outimage;
    float float1, float2, float3, float4;
    char *headerstuff;
    int header=0, outheader=0;
    int overflows=0;
    int underflows=0;
    int bits = 16;
    int bytes = 2;
    int sign = 0;
    float scale=1, offset=0;
    float sum,sumd,sumsq,sumdsq,avg,diff,rms,rmsd,max,min;


    /* check argument list */
    for(i=1; i<argc; ++i)
    {
        if(strlen(argv[i]) > 4)
        {
            if(strstr(argv[i]+strlen(argv[i])-4,".bin") || strstr(argv[i]+strlen(argv[i])-4,".img"))
            {
                //printf("filename: %s\n",argv[i]);
                if(infile1 == NULL){
                    infile1 = fopen(argv[i],"rb");
                    if(infile1 != NULL) infile1name = argv[i];
                }
                else
                {
                    outfilename = argv[i];
                }
            }
        }

        if(argv[i][0] == '-')
        {
            /* option specified */
            if(strstr(argv[i], "-header") && (argc >= (i+1)))
            {
                header = outheader = atoi(argv[i+1]);
            }
            if(strstr(argv[i], "-outheader") && (argc >= (i+1)))
            {
                outheader = atoi(argv[i+1]);
            }
	    /* how many bits in output? */
            if(strstr(argv[i], "-bits") && (argc >= (i+1)))
            {
                bits = atoi(argv[i+1]);
            }
	    /* signed values in output? */
            if(strstr(argv[i], "-signed") )
            {
                sign = 1;
            }
            if(strstr(argv[i], "-unsigned") )
            {
                sign = 0;
            }
	    /* scale and offset? */
            if(strstr(argv[i], "-scale") && (argc >= (i+1)))
            {
                scale = atof(argv[i+1]);
            }
            if(strstr(argv[i], "-offset") && (argc >= (i+1)))
            {
                offset = atof(argv[i+1]);
            }
            if((strstr(argv[i], "-output") || strstr(argv[i], "-outfile")) && (argc >= (i+1)))
            {
                outfilename = argv[i+1];
		++i;
		continue;
            }
            if((strstr(argv[i], "-input") || strstr(argv[i], "-infile")) && (argc >= (i+1)))
            {
                infile1name = argv[i+1];
		if(strstr(infile1name, "-") && strlen(infile1name) == 1)
		{
		    infile1 = stdin;
		}
		else
		{
                    infile1 = fopen(infile1name,"rb");
                }
		++i;
		continue;
            }
        }
    }

    if(infile1 == NULL ){
        printf("usage: float2int file1.bin [outfile.img] -header 512\n");
        printf("options:\n");\
        printf("\t-header \tnumber of bytes to skip in header of each file\n");
        printf("\t-outheader \tnumber of bytes to transfer from input or pad with zeros in output file\n");
        printf("\t-infile \tspecify input file, use - for stdin\n");
        printf("\t-outfile \tspecify output file, use - for stdout\n");
        printf("\t-scale \tapply a scale factor to float data before converting to int\n");
        printf("\t-offset \tapply an offset after scale factor before converting to int\n");
        printf("\t-bits \t8, 16 or 32-bit integer output\n");
        printf("\t-signed \tsigned integer output\n");
        printf("\t-unsigned \tunsigned integer output\n");
        exit(9);
    }

    /* follow-up calcs */
    bytes = bits/8;

    /* load first 4-byte float image */
    if(header)
    {
	headerstuff = calloc(header+10,1);
	rewind(infile1);
	fread(headerstuff,header,1,infile1);
    }
    fseek(infile1,0,SEEK_END);
    n = ftell(infile1);
    pixels = n/sizeof(float);
    fseek(infile1,header,SEEK_SET);
    inimage1 = calloc(pixels,sizeof(float));
    fread(inimage1,pixels,sizeof(float),infile1);
    fclose(infile1);

    /* NOW open the output file */
    if( strstr(outfilename, "-") && strlen(outfilename) == 1 ){
        outfile = stdout;
    }
    else {
        outfile = fopen(outfilename,"wb");
    }
    if(outfile == NULL){
        printf("ERROR: unable to open %s as output\n", outfilename);
        perror("");
    }

    if(outfile != stdout) printf("header = %d bytes  %d pixels\n",header,pixels);

    outimage = calloc(pixels,bytes);
    dint8outimage  = (int8_t *) outimage;
    dint16outimage = (int16_t *) outimage;
    dint32outimage = (int32_t *) outimage;
    uint8outimage  = (uint8_t *) outimage;
    uint16outimage = (uint16_t *) outimage;
    uint32outimage = (uint32_t *) outimage;

    sum = sumsq = sumd = sumdsq = 0.0;
    min = pow(2.0,bits);
    max = 0.0;

    for(i=0;i<pixels;++i)
    {
        float1 = ceil( scale * inimage1[i] + offset  -0.5 );
        if(bits == 32 && sign == 0)
        {
	    uint32outimage[i] = (uint32_t) float1;
	    float2 = uint32outimage[i];
	    if(float1 > (float) UINT_MAX) ++overflows;
	    if(float1 < 0) ++underflows;
        }
        if(bits == 32 && sign == 1)
        {
	    dint32outimage[i] = (int32_t) float1;
	    float2 = dint32outimage[i];
	    if(float1 > (float) INT_MAX) ++overflows;
	    if(float1 < (float) INT_MIN) ++underflows;
        }
        if(bits == 16 && sign == 0)
        {
	    uint16outimage[i] = (uint16_t) float1;
	    float2 = uint16outimage[i];
	    if(float1 > (float) USHRT_MAX) ++overflows;
	    if(float1 < 0) ++underflows;
        }
        if(bits == 16 && sign == 1)
        {
	    dint16outimage[i] = (int16_t) float1;
	    float2 = dint16outimage[i];
	    if(float1 > SHRT_MAX) ++overflows;
	    if(float1 < SHRT_MIN) ++underflows;
        }
        if(bits == 8 && sign == 0)
        {
	    uint8outimage[i] = (uint8_t) float1;
	    float2 = uint8outimage[i];
	    if(float1 > CHAR_MAX) ++overflows;
	    if(float1 < 0) ++underflows;
        }
        if(bits == 8 && sign == 1)
        {
	    dint8outimage[i] = (int8_t) float1;
	    float2 = dint8outimage[i];
	    if(float1 > SCHAR_MAX) ++overflows;
	    if(float1 < SCHAR_MIN) ++underflows;
        }

        if(float2>max) max=float2;
        if(float2<min) min=float2;
        sum += float2;
        sumsq += float2*float2;
    }
    avg = sum/pixels;
    rms = sqrt(sumsq/pixels);
    for(i=0;i<pixels;++i)
    {
        if(bits == 32 && sign == 0) float2 = uint32outimage[i];
        if(bits == 32 && sign == 1) float2 = dint32outimage[i];
        if(bits == 16 && sign == 0) float2 = uint16outimage[i];
        if(bits == 16 && sign == 1) float2 = dint16outimage[i];
        if(bits == 8  && sign == 0) float2 = uint8outimage[i];
        if(bits == 8  && sign == 1) float2 = dint8outimage[i];
        sumd   += float2 - avg;
        sumdsq += (float2 - avg) * (float2 - avg);
    }
    rmsd = sqrt(sumdsq/pixels);
    if(outfile != stdout) printf("max = %g min = %g\n",max,min);
    if(outfile != stdout) printf("mean = %g rms = %g rmsd = %g\n",avg,rms,rmsd);
    if(outfile != stdout) printf("%d overflows, %d underflows\n",overflows,underflows);

    if(outfile != stdout)
    {
        if(sign == 0) printf("writing %s as a %d-byte header and %d %ld-byte unsigned ints\n",outfilename,outheader,pixels,bytes);
        if(sign == 1) printf("writing %s as a %d-byte header and %d %ld-byte signed ints\n",outfilename,outheader,pixels,bytes);
    }
    if(outheader)
    {
	/* copy header from first file */
	if(header > outheader)
	{
	    /* truncate the original header */
	    fwrite(headerstuff,outheader,1,outfile);
	}
	else
	{
	    /* write full original header */
	    fwrite(headerstuff,header,1,outfile);
	    /* pad the rest with zeroes */
	    j = header;
	    k = 0;
	    while(j < outheader)
	    {
	        fwrite(&k,1,1,outfile);
	        ++j;
	    }
	}
    }
    fwrite(outimage,pixels,bytes,outfile);
    fclose(outfile);


    return 0;
}

