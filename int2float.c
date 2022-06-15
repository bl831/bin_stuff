
/* convert binary "int" file to floats                                           -James Holton  9-20-16

example:

gcc -O -O -o int2float int2float.c -lm
./int_float file1.img floatfile.bin

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>


char *infile1name = "";
FILE *infile1 = NULL;
char *outfilename = "floatfile.bin\0";
FILE *outfile = NULL;

int main(int argc, char** argv)
{

    int n,i,j,k,pixels;
    float *outimage;
    void  *inimage1;
    int8_t   *dint8inimage;
    int16_t  *dint16inimage;
    int32_t  *dint32inimage;
    uint8_t  *uint8inimage;
    uint16_t *uint16inimage;
    uint32_t *uint32inimage;
    float float1, float2, float3, float4;
    char *headerstuff;
    int header_bytes=512, outheader_bytes=0;
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
                printf("filename: %s\n",argv[i]);
                if(infile1 == NULL){
                    infile1 = fopen(argv[i],"r");
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
                header_bytes = outheader_bytes = atoi(argv[i+1]);
            }
            if(strstr(argv[i], "-outheader") && (argc >= (i+1)))
            {
                outheader_bytes = atoi(argv[i+1]);
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
        printf("usage: int2float file1.img [outfile.bin] -header 512\n");
        printf("options:\n");\
        printf("\t-header \tnumber of bytes to skip in header of each file\n");
        printf("\t-outheader \tnumber of bytes to transfer from input or pad with zeros in output file\n");
        printf("\t-infile \tspecify input file, use - for stdin\n");
        printf("\t-outfile \tspecify output file, use - for stdout\n");
        printf("\t-scale \tapply a scale factor to int data before converting to float\n");
        printf("\t-offset \tapply an offset after scale factor before converting to float\n");
        printf("\t-bits \t8, 16 or 32-bit integer input\n");
        printf("\t-signed \tsigned integer input\n");
        printf("\t-unsigned \tunsigned integer input\n");
        exit(9);
    }

    /* follow-up calcs */
    bytes = bits/8;

    /* load first int-image */
    if(header_bytes)
    {
	headerstuff = calloc(header_bytes+10,1);
	rewind(infile1);
	fread(headerstuff,header_bytes,1,infile1);
    }
    fseek(infile1,0,SEEK_END);
    n = ftell(infile1)-header_bytes;
    pixels = n/bytes;
    fseek(infile1,header_bytes,SEEK_SET);
    inimage1 = calloc(pixels,bytes);
    fread(inimage1,pixels,bytes,infile1);
    fclose(infile1);
    dint8inimage  = (int8_t *) inimage1;
    dint16inimage = (int16_t *) inimage1;
    dint32inimage = (int32_t *) inimage1;
    uint8inimage  = (uint8_t *) inimage1;
    uint16inimage = (uint16_t *) inimage1;
    uint32inimage = (uint32_t *) inimage1;

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

    if(outfile != stdout) printf("header = %d bytes  %d pixels\n",header_bytes,pixels);

    outimage = calloc(pixels,sizeof(float));

    sum = sumsq = sumd = sumdsq = 0.0;
    min = 0;
    max = 0;

    for(i=0;i<pixels;++i)
    {
        if(bits == 32 && sign == 0)
        {
	    float1 = (float) uint32inimage[i];
	    if(uint32inimage[i] == UINT_MAX) ++overflows;
	    if(uint32inimage[i] == 0)        ++underflows;
        }
        if(bits == 32 && sign == 1)
        {
	    float1 = (float) dint32inimage[i];
	    if(dint32inimage[i] == INT_MAX) ++overflows;
	    if(dint32inimage[i] == INT_MIN) ++underflows;
        }
        if(bits == 16 && sign == 0)
        {
	    float1 = (float) uint16inimage[i];
	    if(uint16inimage[i] == USHRT_MAX) ++overflows;
	    if(uint16inimage[i] == 0)         ++underflows;
        }
        if(bits == 16 && sign == 1)
        {
	    float1 = (float) dint16inimage[i];
	    if(dint16inimage[i] == SHRT_MAX) ++overflows;
	    if(dint16inimage[i] == SHRT_MIN) ++underflows;
        }
        if(bits == 8 && sign == 0)
        {
	    float1 = (float) uint8inimage[i];
	    if(uint8inimage[i] == CHAR_MAX) ++overflows;
	    if(uint8inimage[i] == 0)        ++underflows;
        }
        if(bits == 8 && sign == 1)
        {
	    float1 = (float) dint8inimage[i];
	    if(dint8inimage[i] == SCHAR_MAX) ++overflows;
	    if(dint8inimage[i] == SCHAR_MIN) ++underflows;
        }
	outimage[i] = float1;
        if(float1>max || i==0) max=float1;
        if(float1<min || i==0) min=float1;
        sum += float1;
        sumsq += float1*float1;
    }
    avg = sum/pixels;
    rms = sqrt(sumsq/pixels);
    for(i=0;i<pixels;++i)
    {
        sumd   += outimage[i] - avg;
        sumdsq += (outimage[i] - avg) * (outimage[i] - avg);
    }
    rmsd = sqrt(sumdsq/pixels);
    if(outfile != stdout) printf("max = %g min = %g\n",max,min);
    if(outfile != stdout) printf("mean = %g rms = %g rmsd = %g\n",avg,rms,rmsd);

    if(outfile != stdout) printf("writing %s as a %d-byte header and %d %d-byte floats\n",outfilename,outheader_bytes,pixels,sizeof(float));
    if(outheader_bytes)
    {
	/* copy header_bytes from first file */
	if(header_bytes > outheader_bytes)
	{
	    /* truncate the original header */
	    fwrite(headerstuff,outheader_bytes,1,outfile);
	}
	else
	{
	    /* write full original header */
	    fwrite(headerstuff,header_bytes,1,outfile);
	    /* pad the rest with zeroes */
	    j = header_bytes;
	    k = 0;
	    while(j < outheader_bytes)
	    {
	        fwrite(&k,1,1,outfile);
	        ++j;
	    }
	}
    }
    fwrite(outimage,pixels,sizeof(float),outfile);
    fclose(outfile);


    return 0;
}

