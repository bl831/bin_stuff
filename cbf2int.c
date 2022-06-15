
/* convert dectris-style cbf file to one of the ints                                           -James Holton  5-2-17

example:

gcc -O -O -o cbf2int cbf2int.c -lm
./cbf2int file.cbf file1.img 

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

#include <sys/time.h>
#include <sys/resource.h>

double get_time();
double starttime;

char *infile1name = "";
FILE *infile1 = NULL;
char *outfilename = "intfile.bin\0";
FILE *outfile = NULL;
int append_output = 0;


/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned int UINT4;


/* MD5 context. */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
void MD5Final(unsigned char [16], MD5_CTX *);
int cbf_md5digest_to64 (char *, const unsigned char *);





int main(int argc, char** argv)
{
    int n,i,j,k,pixels;
    int8_t *inimage1;
    int32_t *inimage2;
    int32_t *decompressed_data;
    void *outimage;
    int8_t   *dint8outimage;
    int16_t  *dint16outimage;
    int32_t  *dint32outimage;
    uint8_t  *uint8outimage;
    uint16_t *uint16outimage;
    uint32_t *uint32outimage;
    float *float32outimage;
    double *float64outimage;
    float float1, float2, float3, float4;
    long long1, long2;
    char *headerstuff, *string;
    int header_lines=0, outheader_lines=0;
    int header_bytes=0, outheader_bytes=0;
    int file_bytes_read=0, data_bytes_read=0;
    int compressed_size=0, uncompressed_size=0;
    char *start_of_data;
    int overflows=0;
    int underflows=0;
    int bits = 32;
    int bytes = 4;
    int sign = 1;
    int floatout = 0;
    int downsample = 0;
    int pixx,pixy,outpixx,outpixy,outi,outxpixels,outypixels,outpixels;
    int nonlinear = 0;
    int nomd5check = 0;
    int md5checkonly = 0;
    int print_timing = 1;
    int nowrite = 0;
    int nostats = 0;
    float scale=1, offset=0;
    float sum,sumd,sumsq,sumdsq,avg,diff,rms,rmsd;
    long max,min,isum=0,valid_pixels=0;
    int max_x=0,max_y=0,xpixels,ypixels;
    int start_x=-1,stop_x=-1,start_y=-1,stop_y=-1;

    union {
        int8_t *int8ptr;
	uint8_t *uint8ptr;
        int16_t *int16ptr;
	uint16_t *uint16ptr;
	int32_t *int32ptr;
        float *floatptr;
        double *doubleptr;
    } bitshuffle;
    int32_t cval,cdiff;

starttime = get_time();

    /* check argument list */
    for(i=1; i<argc; ++i)
    {
        if(strlen(argv[i]) > 4)
        {
            if(strstr(argv[i]+strlen(argv[i])-4,".cbf"))
            {
                printf("filename: %s\n",argv[i]);
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
                header_lines = atoi(argv[i+1]);
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
            if(strstr(argv[i], "-float") )
            {
                floatout = 1;
            }
            if(strstr(argv[i], "-notfloat") )
            {
                floatout = 0;
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
            if(strstr(argv[i], "-downsample") && (argc >= (i+1)))
            {
                downsample = atoi(argv[i+1]);
            }
            if(strstr(argv[i], "-nonlinear"))
            {
                nonlinear = 1;
            }
            if(strstr(argv[i], "-nowrite") )
            {
                nowrite = 1;
            }
            if(strstr(argv[i], "-nocheck") || strstr(argv[i], "-nomd5"))
            {
                nomd5check = 1;
            }
            if(strstr(argv[i], "-checkonly") )
            {
                md5checkonly = 1;
            }
            if(strstr(argv[i], "-notim") )
            {
                print_timing = 0;
            }
            if(strstr(argv[i], "-nostat") )
            {
                nostats = 1;
            }
            if(strstr(argv[i], "-append") )
            {
                append_output = 1;
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
	    if(strstr(argv[i], "-box") && (argc >= (i+4)))
	    {
		start_x = atol(argv[i+1]);
		start_y = atol(argv[i+2]);
		 stop_x = atol(argv[i+3]);
		 stop_y = atol(argv[i+4]);
	    }
        }
    }

    if(infile1 == NULL ){
        printf("usage: cbf2int file1.cbf [outfile.img]\n");
        printf("options:\n");\
        printf("\t-header \tnumber of lines to skip in header of each file\n");
        printf("\t-outheader \tnumber of bytes to transfer from input or pad with zeros in output file\n");
        printf("\t-infile \tspecify input file, use - for stdin\n");
        printf("\t-outfile \tspecify output file, use - for stdout\n");
        printf("\t-append \tappend to the specified output file\n");
        printf("\t-scale \tapply a scale factor to cbf data before converting to int\n");
        printf("\t-offset \tapply an offset after scale factor before converting to int\n");
        printf("\t-bits \t8, 16 or 32-bit integer output\n");
        printf("\t-signed \tsigned integer output\n");
        printf("\t-unsigned \tunsigned integer output\n");
        printf("\t-nocheck \tspeed up: do not check MD5 checksum\n");
        printf("\t-notime \tdo not print out timing\n");
        printf("\t-nostat \tspeed up: by skipping computation of statistics\n");
        printf("\t-nowrite \tspeed up: do not write output data\n");
        printf("\t-downsample 8\tdown-sample the data with a maximum-value filter over 8x8 pixels\n");
        printf("\t-nonlinear \toutput the square root of the pixel value\n");
        exit(9);
    }

    /* follow-up calcs */
    if(floatout && bits<32) bits = 32;
    bytes = bits/8;
    if(downsample == 0) downsample = 1;

    /* decide if we should NOT be printing stuff to stdout */
    if( strstr(outfilename, "-") && strlen(outfilename) == 1 ){
        outfile = stdout;
    }

    /* start reading the cbf image */

    /* load what should be more than a header on the first try */
    headerstuff = calloc(sizeof(char),65535);
    file_bytes_read = fread(headerstuff,sizeof(char),65534,infile1);
    //printf("GOTHERE: about to overwrite char:%c%c\n",headerstuff[file_bytes_read-1],headerstuff[file_bytes_read]);
    /* make sure this is a zero-terminated string? */
    headerstuff[file_bytes_read] = 0;
    //printf("GOTHERE: read in %d %d-byte blocks\n",file_bytes_read,sizeof(char));
    string=strstr(headerstuff,"\f\032\004\325");
    while( string == NULL )
    {
        /* TBD: keep reading if we don't see it? */
        printf("This does not look like a CBF file %s\n",string);
        exit(9);
    }
    /* how many bytes are in the header? */
    header_bytes = string - headerstuff + 4;
    if(print_timing && outfile != stdout) printf("reading input file delta-T: %g\n",get_time()-starttime);
    printf("found %d header bytes\n",header_bytes);
    start_of_data = string + 4;
    /* calculate number of data bytes already read in as part of the "header" */
    data_bytes_read = file_bytes_read - header_bytes;
    //printf("GOTHERE: start of data chars:%c%c%c%c\n",start_of_data[0],start_of_data[1],start_of_data[2],start_of_data[3]);

    /* now use the header to figure out how many bytes we still need to read */
    string = strstr(headerstuff,"X-Binary-Size:");
    if(string == NULL)
    {
	printf("*** Bad header - cannot find X-Binary-Size\n");
        exit(9);
    }
    string += strlen("X-Binary-Size:");
    sscanf(string, "%d", &compressed_size);
    printf(" compressed_size = %d\n",compressed_size);
    inimage1 = calloc(compressed_size,sizeof(char));
    
    /* copy over stuff we already read up to the current point in the input file */
    memcpy(inimage1,start_of_data,data_bytes_read);
    /* now finish the read */
    fread(inimage1+data_bytes_read,sizeof(char),compressed_size-data_bytes_read,infile1);
    /* and that is all we need to know */
    fclose(infile1);

    /* how big is the output file going to be? */
    string = strstr(headerstuff,"X-Binary-Number-of-Elements:");
    if(string != NULL)
    {
	string += strlen("X-Binary-Number-of-Elements:");
	sscanf(string, "%d", &pixels);
        uncompressed_size = pixels*sizeof(int32_t);
        printf(" uncompressed_size = %d\n",uncompressed_size);
    }
//    if(pixels == 0) pixels = 6224001;
    inimage2 = calloc(pixels,sizeof(int32_t));

    /* x-y size */
    string = strstr(headerstuff,"X-Binary-Size-Fastest-Dimension:");
    if(string != NULL)
    {
	string += strlen("X-Binary-Size-Fastest-Dimension:");
	sscanf(string, "%d", &xpixels);
        printf(" x size = %d\n",xpixels);
    }
    string = strstr(headerstuff,"X-Binary-Size-Second-Dimension:");
    if(string != NULL)
    {
	string += strlen("X-Binary-Size-Second-Dimension:");
	sscanf(string, "%d", &ypixels);
        printf(" y size = %d\n",ypixels);
    }

    string = strstr(headerstuff,"Content-MD5: ");
    if(! nomd5check && string != NULL)
    {
	if(print_timing && outfile != stdout) printf("starting MD5sum delta-T: %g\n",get_time()-starttime);
	// calculate the Eikenberry md5 checksum
	MD5_CTX context;
	unsigned char rawdigest[17];
	char digest[25];
	char header_digest[25];

	string += strlen("Content-MD5: ");
        strncpy(header_digest,string,24);
        header_digest[24]=0;
        printf(" header MD5: %s\n",header_digest);

	MD5Init(&context);
	MD5Update(&context, (unsigned char *) inimage1, compressed_size);
	MD5Final(rawdigest, &context);
	cbf_md5digest_to64 (digest, rawdigest);

	printf("content MD5: %24s\n",digest);

        if(0 == strncmp(header_digest,digest,24))
        {
            printf("MD5 sums match.\n");
        }
        else
        {
	    printf("MD5 integrity check failed! \n");
            exit(9);
        }
    }

    if(md5checkonly)
    {
	if(print_timing) printf("finished delta-T: %g\n",get_time()-starttime);
	exit(0);
    }

    /* decompres the CBF data */
    if(1)
    {
	if(print_timing && outfile != stdout) printf("starting decompression delta-T: %g\n",get_time()-starttime);
        sum = isum = 0;
        max=0;
        i = 0;
        min=INT_MAX;
        printf("decompressing...\n");
        /* intialize pointer union to start of compressed data */
        bitshuffle.int8ptr = inimage1;
        decompressed_data = inimage2;
        cval = 0;
        while (bitshuffle.int8ptr-inimage1 < compressed_size)
        {
	    /*
	    ** If the file is corrupted with a lot of zeroes, 
	    ** then (*t.ucp != 0x80) is true too many times,
	    ** and 'dp' tries to go beyond the buffer end.
        	so: if ((char *)dp >= (ObjPtr->Data+size))
	    */
	    if (decompressed_data >= inimage2+uncompressed_size) break;
	
	    if (*bitshuffle.uint8ptr != 0x80)
            {
                /* signals one-byte value storing change in intensity */
                cdiff = (int)*bitshuffle.int8ptr++;
            }
	    else
	    {
                /* exact value of 0x80 signals move on to next byte */
		bitshuffle.int8ptr++;
		if (*bitshuffle.int16ptr==0)
		{
                    /* 16-bit zero signals that cval needs to be reset to zero */
		    cval = 0;
                    /* and we advance two bytes to skip over this flag */
		    bitshuffle.int16ptr++;
                    /* next bit field could be anything */
		    continue;
		}
		if (*bitshuffle.uint16ptr != 0x8000)
                {
                    /* this is a 2-byte difference, record it and advance 2 bytes */
                    cdiff = (int)*bitshuffle.int16ptr++;
                }
		else
                {
                    /* exact value of 0x8000 signals move on two more bytes */
                    bitshuffle.int16ptr++;
                    /* and the next 4 bytes are a 32-bit intensity change */
                    cdiff = *bitshuffle.int32ptr++;
		}
            }
            /* finally, accumulate the intensity value and store it */
	    cval = *decompressed_data++ = cval + cdiff;
            if(! nostats && cval >= 0)
            {
                ++valid_pixels;
                sum += cval;
                isum += cval;
                if(cval>max) {
                    max=cval;
                    max_x = i % xpixels;
                    max_y = i / xpixels;
                }
                if(cval<min) min=cval;
            }
            ++i;
	}
        /* end of decompression */
        //printf("decompression done.\n");
	if(print_timing && outfile != stdout)
        {
            printf("decompression done. delta-T: %g\n",get_time()-starttime);
            if(! nostats) printf("sum: %ld max: %ld (@ %d %d) min: %ld valid_pixels: %ld\n",isum,max,max_x,max_y,min,valid_pixels);
        }
    }

    if(outfile != stdout) printf("%d pixels\n",pixels);

    if(nowrite && nostats)
    {
	if(print_timing) printf("early exit delta-T: %g\n",get_time()-starttime);
        exit(0);
    }

    /* now allocate memory for the output/stat image */
    outimage = calloc(pixels,bytes);
    dint8outimage  = (int8_t *) outimage;
    dint16outimage = (int16_t *) outimage;
    dint32outimage = (int32_t *) outimage;
    uint8outimage  = (uint8_t *) outimage;
    uint16outimage = (uint16_t *) outimage;
    uint32outimage = (uint32_t *) outimage;
    float32outimage = (float *) outimage;
    float64outimage = (double *) outimage;

    outxpixels = xpixels / downsample;
    outypixels = ypixels / downsample;
    outpixels = outxpixels*outypixels;
    if(outfile != stdout) printf("output dimensions: %d %d\n",outxpixels,outypixels);

    sum = sumsq = sumd = sumdsq = 0.0;

    if(print_timing && outfile != stdout) printf("starting conversion and statistics delta-T: %g\n",get_time()-starttime);
    for(i=0;i<pixels;++i)
    {
        outi = i;
//        if(downsample != 1)
        {
            /* get mapping of old image onto new image */
            pixx = i % xpixels;
            pixy = i / xpixels;
            outpixx = pixx/downsample;
            outpixy = pixy/downsample;
            outi = outpixy*outxpixels+outpixx;
        }
        if(scale == 1.0 && offset == 0.0)
        {
            long1 = inimage2[i];
        }
        else {
            long1 = scale * inimage2[i] + offset;
        }
        if(nonlinear)
        {
            float1 = inimage2[i];
            if(float1 <= 0.0)
            {
                float1=0.0;
            }
            else
            {
                float1=sqrt(float1);
            }
            long1 = float1;
        }
        if(! floatout)
        {
            if(bits == 32 && sign == 0)
            {
	        uint32outimage[outi] = (uint32_t) long1;
	        long2 = uint32outimage[outi];
	        if(! nostats && long1 > UINT_MAX) ++overflows;
	        if(! nostats && long1 < 0) ++underflows;
            }
            if(bits == 32 && sign == 1)
            {
                /* no need to convert native decompressed format */
                if(downsample)
                {
                    /* use max filter */
                    if(dint32outimage[outi] > (int32_t) long1) long1 = dint32outimage[outi];
                }
	        dint32outimage[outi] = (int32_t) long1;
	        long2 = dint32outimage[outi];
	        if(! nostats && long1 > INT_MAX) ++overflows;
	        if(! nostats && long1 < INT_MIN) ++underflows;
            }
            if(bits == 16 && sign == 0)
            {
	        uint16outimage[outi] = (uint16_t) long1;
	        long2 = uint16outimage[outi];
	        if(! nostats && long1 > USHRT_MAX) ++overflows;
	        if(! nostats && long1 < 0) ++underflows;
            }
            if(bits == 16 && sign == 1)
            {
	        dint16outimage[outi] = (int16_t) long1;
	        long2 = dint16outimage[outi];
	        if(! nostats && long1 > SHRT_MAX) ++overflows;
	        if(! nostats && long1 < SHRT_MIN) ++underflows;
            }
            if(bits == 8 && sign == 0)
            {
                if(downsample)
                {
                    /* use max filter */
                    if(uint8outimage[outi] > (uint8_t) long1) long1 = uint8outimage[outi];
                }
	        uint8outimage[outi] = (uint8_t) long1;
	        long2 = uint8outimage[outi];
	        if(! nostats && long1 > CHAR_MAX) ++overflows;
	        if(! nostats && long1 < 0) ++underflows;
            }
            if(bits == 8 && sign == 1)
            {
	        dint8outimage[outi] = (int8_t) long1;
	        long2  = dint8outimage[outi];
	        if(! nostats && long1 > SCHAR_MAX) ++overflows;
	        if(! nostats && long1 < SCHAR_MIN) ++underflows;
            }
        }
        else
        {
            if(bits == 32)
            {
                float32outimage[outi] = (float) long1;
                long2 = float32outimage[outi];
	        if(! nostats && long1 > INT_MAX) ++overflows;
	        if(! nostats && long1 < INT_MIN) ++underflows;                
            }
            if(bits == 64)
            {
                float64outimage[outi] = (double) long1;
                long2 = float64outimage[outi];
	        if(! nostats && long1 > INT_MAX) ++overflows;
	        if(! nostats && long1 < INT_MIN) ++underflows;
            }
        }
	if(! nostats && inimage1[i] > 0)
        {
            sum += long2;
            float2 = long2;
            sumsq += float2*float2;
        }
    }
    avg = sum/valid_pixels;
    rms = sqrt(sumsq/valid_pixels);
    if(print_timing && outfile != stdout) printf("endofstat1 delta-T: %g\n",get_time()-starttime);

    /* no need to compute stats if we can't print them out */
    if(! nostats && outfile != stdout) 
    {
        /* final pass to compute RMSD */
        for(i=0;i<outpixels;++i)
        {
            if(bits == 32 && sign == 0) float2 = uint32outimage[i];
            if(bits == 32 && sign == 1) float2 = dint32outimage[i];
            if(bits == 16 && sign == 0) float2 = uint16outimage[i];
            if(bits == 16 && sign == 1) float2 = dint16outimage[i];
            if(bits == 8  && sign == 0) float2 = uint8outimage[i];
            if(bits == 8  && sign == 1) float2 = dint8outimage[i];
            if(bits == 32 && floatout == 1) float2 = float32outimage[i];
            if(bits == 64 && floatout == 1) float2 = float64outimage[i];
            if(inimage1[i] > 0)
            {
                sumd   += float2 - avg;
                sumdsq += (float2 - avg) * (float2 - avg);
            }
        }
        rmsd = sqrt(sumdsq/outpixels);
        printf("output sum = %g avg = %g\n",(float) sum,(float) avg);
        printf("output max = %g min = %g\n",(float) max,(float) min);
        printf("output mean = %g rms = %g rmsd = %g\n",avg,rms,rmsd);
        printf("%d overflows, %d underflows\n",overflows,underflows);
    }

    if(nowrite)
    {
	if(print_timing) printf("early exit delta-T: %g\n",get_time()-starttime);
        exit(0);
    }


    /* NOW open the output file */
    if ( outfile != stdout ) {
        if(append_output) {
            outfile = fopen(outfilename,"ab");
        }
        else
        {
            outfile = fopen(outfilename,"wb");
        }
        if(floatout)
        {
            printf("writing %s as a %d-byte header and %d %d-byte floats\n",outfilename,outheader_bytes,outpixels,bytes);
        }
        else
        {
            if(sign == 0) printf("writing %s as a %d-byte header and %d %d-byte unsigned ints\n",outfilename,outheader_bytes,outpixels,bytes);
            if(sign == 1) printf("writing %s as a %d-byte header and %d %d-byte signed ints\n",outfilename,outheader_bytes,outpixels,bytes);
        }
    }
    if(outfile == NULL){
        printf("ERROR: unable to open %s as output\n", outfilename);
        perror("");
    }

    if(outheader_bytes)
    {
        header_bytes = strlen(headerstuff);
	/* copy header from first file */
	if(header_bytes > outheader_bytes)
	{
	    /* truncate the original header */
	    fwrite(headerstuff,outheader_bytes,1,outfile);
	}
	/* pad the rest with zeroes */
        j = header_bytes;
	k = 0;
	while(header_bytes < outheader_bytes)
	{
	    fwrite(&k,1,1,outfile);
	    ++j;
	}
    }

    if(print_timing && outfile != stdout) printf(" about to write, delta-T: %g\n",get_time()-starttime);

    fwrite(outimage,outpixels,bytes,outfile);
    fclose(outfile);

    if(print_timing && outfile != stdout) printf("total run delta-T: %g\n",get_time()-starttime);

    return 0;
}


double get_time()
{
    struct timeval t;
    struct timezone tzp;
    gettimeofday(&t, &tzp);
    return t.tv_sec + t.tv_usec*1e-6;
}














/*
Eric F. Eikenberry's MD5 thing that doesn't obey the MD5 standard and nobody semes to use


   MD5.H - header file for MD5C.C
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */




/* Constants for MD5Transform routine.
 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void MD5Transform (UINT4 [4], unsigned char [64]);
static void Encode(unsigned char *, UINT4 *, unsigned int);
static void Decode(UINT4 *, unsigned char *, unsigned int);
static void MD5_memcpy(POINTER, POINTER, unsigned int);
static void MD5_memset(POINTER, int, unsigned int);

static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
/* #define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n)))) */

#define ROTATE_LEFT(x, n) (((x) << (n)) | (((x) & 0x0FFFFFFFF) >> (32 - (n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

/* MD5 initialization. Begins an MD5 operation, writing a new context.
 */
void MD5Init (context)
MD5_CTX *context;                                        /* context */
{
  context->count[0] = context->count[1] = 0;
  /* Load magic initialization constants.
*/
  context->state[0] = 0x67452301;
  context->state[1] = 0xefcdab89;
  context->state[2] = 0x98badcfe;
  context->state[3] = 0x10325476;
}

/* MD5 block update operation. Continues an MD5 message-digest
  operation, processing another message block, and updating the
  context.
 */
void MD5Update (context, input, inputLen)
MD5_CTX *context;                                        /* context */
unsigned char *input;                                /* input block */
unsigned int inputLen;                     /* length of input block */
{
  unsigned int i, index, partLen;
  UINT4 I1, I2, S;

  /* Compute number of bytes mod 64 */
  index = (unsigned int)((context->count[0] >> 3) & 0x3F);

  /* Update number of bits */
  I1 = ((UINT4) inputLen) << 3;
  I2 = ((UINT4) context->count [0]);
  context->count[0] = S = I1 + I2;
  if (((~S & (I1 | I2)) | (I1 & I2)) & 0x080000000)
    context->count[1]++;
  context->count[1] += ((UINT4) inputLen >> 29);

  partLen = 64 - index;

  /* Transform as many times as possible.
*/
  if (inputLen >= partLen) {
 MD5_memcpy
   ((POINTER)&context->buffer[index], (POINTER)input, partLen);
 MD5Transform (context->state, context->buffer);

 for (i = partLen; i + 63 < inputLen; i += 64)
   MD5Transform (context->state, &input[i]);

 index = 0;
  }
  else
 i = 0;

  /* Buffer remaining input */
  MD5_memcpy
 ((POINTER)&context->buffer[index], (POINTER)&input[i],
  inputLen-i);
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
  the message digest and zeroizing the context.
 */
void MD5Final (digest, context)
unsigned char digest[16];                         /* message digest */
MD5_CTX *context;                                       /* context */
{
  unsigned char bits[8];
  unsigned int index, padLen;

  /* Save number of bits */
  Encode (bits, context->count, 8);
  /* Pad out to 56 mod 64.
*/
  index = (unsigned int)((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD5Update (context, PADDING, padLen);

  /* Append length (before padding) */
  MD5Update (context, bits, 8);
  /* Store state in digest */
  Encode (digest, context->state, 16);

  /* Zeroize sensitive information.
*/
  MD5_memset ((POINTER)context, 0, sizeof (*context));
}

/* MD5 basic transformation. Transforms state based on block.
 */
static void MD5Transform (state, block)
UINT4 state[4];
unsigned char block[64];
{
  UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  Decode (x, block, 64);

  /* Round 1 */
  FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
  FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
  FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
  FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
  FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

 /* Round 2 */
  GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

  /* Round 3 */
  HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
  HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
  HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
  HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
  HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
  HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
  HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
  HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
  HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
  HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
  HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
  HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
  HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
  HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
  HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
  HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

  /* Round 4 */
  II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;

  /* Zeroize sensitive information.
*/
  MD5_memset ((POINTER)x, 0, sizeof (x));
}

/* Encodes input (UINT4) into output (unsigned char). Assumes len is
  a multiple of 4.
 */
static void Encode (output, input, len)
unsigned char *output;
UINT4 *input;
unsigned int len;
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4) {
 output[j] = (unsigned char)(input[i] & 0xff);
 output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
 output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
 output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
  }
}

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
  a multiple of 4.
 */
static void Decode (output, input, len)
UINT4 *output;
unsigned char *input;
unsigned int len;
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4)
 output[i] = ((UINT4)input[j]) | (((UINT4)input[j+1]) << 8) |
   (((UINT4)input[j+2]) << 16) | (((UINT4)input[j+3]) << 24);
}

/* Note: Replace "for loop" with standard memcpy if possible.
 */

static void MD5_memcpy (output, input, len)
POINTER output;
POINTER input;
unsigned int len;
{
  unsigned int i;

  for (i = 0; i < len; i++)
 output[i] = input[i];
}

/* Note: Replace "for loop" with standard memset if possible.
 */
static void MD5_memset (output, value, len)
POINTER output;
int value;
unsigned int len;
{
  unsigned int i;

  for (i = 0; i < len; i++)
 ((char *)output)[i] = (char)value;
}





  /* Encode a 16-character MD5 digest in base-64 (25 characters) */

int cbf_md5digest_to64 (char *encoded_digest, const unsigned char *digest)
{
  static char basis_64 [] =

       "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  int todo;
  
  if (!encoded_digest || !digest)
  
    return 0;
  

    /* Encode the 16 characters in base 64 */
    
  for (todo = 0; todo < 18; todo += 3)
  {
    encoded_digest [0] = basis_64 [((digest [todo + 0] >> 2) & 0x03f)];

    if (todo < 15)
    {
      encoded_digest [1] = basis_64 [((digest [todo + 0] << 4) & 0x030) |
                                     ((digest [todo + 1] >> 4) & 0x00f)];
      encoded_digest [2] = basis_64 [((digest [todo + 1] << 2) & 0x03c) |
                                     ((digest [todo + 2] >> 6) & 0x003)];
      encoded_digest [3] = basis_64 [((digest [todo + 2])      & 0x03f)];
    }
    else
    {
      encoded_digest [1] = basis_64 [((digest [todo + 0] << 4) & 0x030)];

      encoded_digest [2] = encoded_digest [3] = '=';
    }

    encoded_digest += 4;
  } 
  
  *encoded_digest  = '\0';

  return 0;
}    



