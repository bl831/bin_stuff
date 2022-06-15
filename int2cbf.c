
/* convert integer data into dectris-style cbf data                                           -James Holton  5-10-17

example:

gcc -O -O -o int2cbf int2cbf.c -lm
./int2cbf file.img file1.cbf 

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
char *outfilename = "compressed.cbf\0";
FILE *outfile = NULL;


/* Eikenberry MD5 stuff */
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
    int32_t *decompressed_data;
    void *outimage;
    int8_t   *dint8inimage;
    int16_t  *dint16inimage;
    int32_t  *dint32inimage;
    uint8_t  *uint8inimage;
    uint16_t *uint16inimage;
    uint32_t *uint32inimage;
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
    int nomd5check = 0;
    int md5checkonly = 0;
    int print_timing = 1;
    int nowrite = 0;
    int nostats = 0;
    float scale=1, offset=0;
    float sum,sumd,sumsq,sumdsq,avg,diff,rms,rmsd;
    long max,min,isum=0,valid_pixels=0;
    int max_x=0,max_y=0,xpixels=0,ypixels=0;
    int start_x=-1,stop_x=-1,start_y=-1,stop_y=-1;

    union {
        int8_t *int8ptr;
	uint8_t *uint8ptr;
        int16_t *int16ptr;
	uint16_t *uint16ptr;
	int32_t *int32ptr;
    } bitshuffle;
    int32_t cval,nval,cdiff,pcnt,npix4;

starttime = get_time();

    /* check argument list */
    for(i=1; i<argc; ++i)
    {
        if(strlen(argv[i]) > 4)
        {
            if(strstr(argv[i]+strlen(argv[i])-4,".bin") ||
               strstr(argv[i]+strlen(argv[i])-4,".img") ||
               strstr(argv[i]+strlen(argv[i])-4,".cbf"))
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
                header_bytes = atoi(argv[i+1]);
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
            if(strstr(argv[i], "-detpixels") && (strlen(argv[i]) == 10) && (argc > (i+1)))
            {
                xpixels = ypixels = atoi(argv[i+1]);
            }
            if(strstr(argv[i], "-detpixels_x") && (argc > (i+1)))
            {
                xpixels = atoi(argv[i+1]);
            }
            if(strstr(argv[i], "-detpixels_y") && (argc > (i+1)))
            {
                ypixels = atoi(argv[i+1]);
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
        printf("usage: int2cbf file1.img [outfile.cbf]\n");
        printf("options:\n");\
        printf("\t-header \tnumber of lines to skip in header of each file\n");
        printf("\t-outheader \tnumber of bytes to transfer from input or pad with zeros in output file\n");
        printf("\t-infile \tspecify input file, use - for stdin\n");
        printf("\t-outfile \tspecify output file, use - for stdout\n");
        printf("\t-scale \tapply a scale factor to int data before converting to cbf\n");
        printf("\t-offset \tapply an offset after scale factor before converting to cbf\n");
        printf("\t-bits \t8, 16 or 32-bit integer input\n");
        printf("\t-signed \tsigned integer input\n");
        printf("\t-unsigned \tunsigned integer input\n");
        exit(9);
    }

    /* follow-up calcs */
    bytes = bits/8;

    /* decide if we should NOT be printing stuff to stdout */
    if( strstr(outfilename, "-") && strlen(outfilename) == 1 ){
        outfile = stdout;
    }

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

    if ( outfile != stdout ) {
        if(sign == 0) printf("reading %s as a %d-byte header and %d %d-byte unsigned ints\n",infile1name,header_bytes,pixels,bytes);
        if(sign == 1) printf("reading %s as a %d-byte header and %d %d-byte signed ints\n",infile1name,header_bytes,pixels,bytes);
    }
    dint8inimage  = (int8_t *) inimage1;
    dint16inimage = (int16_t *) inimage1;
    dint32inimage = (int32_t *) inimage1;
    uint8inimage  = (uint8_t *) inimage1;
    uint16inimage = (uint16_t *) inimage1;
    uint32inimage = (uint32_t *) inimage1;

    /* other sensibe defaults */
    if(! xpixels && ! ypixels) {
	/* hmm... guess? */
	printf("WARNING: guessing xy pixel dimensions.\n");
	xpixels = sqrt(pixels);
	ypixels = pixels/xpixels;
	while( pixels != xpixels*ypixels && xpixels > 0 )
	{
	    --xpixels;
	    ypixels = pixels/xpixels;
	}
	if( pixels != xpixels*ypixels) {
	     xpixels = pixels;
	     ypixels = 1;
	}
    }
    if(xpixels && ! ypixels) {
	ypixels = pixels/xpixels;
    }
    if(! xpixels && ypixels) {
	xpixels = pixels/ypixels;
    }
    

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

    /* allocate 2x the needed output bytes */
    outimage = (void *) calloc(2*pixels,sizeof(int32_t));

    if(print_timing && outfile != stdout) printf("starting compression delta-T: %g\n",get_time()-starttime);
    sum = isum = sumsq = sumd = sumdsq = 0.0;
    min = INT_MAX;
    max = 0;
    pcnt = 0;
    npix4 = pixels/4;
    /* intialize pointer union to start of compressed data */
    bitshuffle.int8ptr = (signed char *) outimage;
    cval = 0;
    /* now convert to 32-bit int and compress on the fly */
    for(i=0;i<pixels;++i)
    {
        if(bits == 32 && sign == 0)
        {
	    nval = (int32_t) uint32inimage[i];
	    if(uint32inimage[i] == UINT_MAX) ++overflows;
	    if(uint32inimage[i] == 0)        ++underflows;
        }
        if(bits == 32 && sign == 1)
        {
	    nval = (int32_t) dint32inimage[i];
	    if(dint32inimage[i] == INT_MAX) ++overflows;
	    if(dint32inimage[i] == INT_MIN) ++underflows;
        }
        if(bits == 16 && sign == 0)
        {
	    nval = (int32_t) uint16inimage[i];
	    if(uint16inimage[i] == USHRT_MAX) ++overflows;
	    if(uint16inimage[i] == 0)         ++underflows;
        }
        if(bits == 16 && sign == 1)
        {
	    nval = (int32_t) dint16inimage[i];
	    if(dint16inimage[i] == SHRT_MAX) ++overflows;
	    if(dint16inimage[i] == SHRT_MIN) ++underflows;
        }
        if(bits == 8 && sign == 0)
        {
	    nval = (int32_t) uint8inimage[i];
	    if(uint8inimage[i] == CHAR_MAX) ++overflows;
	    if(uint8inimage[i] == 0)        ++underflows;
        }
        if(bits == 8 && sign == 1)
        {
	    nval = (int32_t) dint8inimage[i];
	    if(dint8inimage[i] == SCHAR_MAX) ++overflows;
	    if(dint8inimage[i] == SCHAR_MIN) ++underflows;
        }
//printf("GOTHERE: %d  nval= %d\n",i,nval);

        /* now do the actual compression */
        /* first, take the derivative */
	cdiff = nval - cval;
//printf("GOTHERE: %d cdiff= %d\n",i,cdiff);
	if (abs(cdiff) <= 127)
            /* count is small enough to store with one byte, so do this and move on */
	    *bitshuffle.int8ptr++ = (char)cdiff;
	else
	{
            /* special one-byte value indicating the count is bigger than one byte, save and move on */
	    *bitshuffle.uint8ptr++ = 0x80;
	    if (abs(cdiff) <= 32767)
                /* count fits into two bytes, so store it and move on */
	        *bitshuffle.int16ptr++ = (short)cdiff;
	    else
	    {
                /* special two-byte value indicating the count needs 32 bits, save and move on */
	        *bitshuffle.uint16ptr++ = 0x8000;
                /* store as 4 bytes, we just used 7 bytes to store a 4-byte number, oh well */
	        *bitshuffle.int32ptr++ = cdiff;
                /* see if this is getting out of hand */
	        if ((++pcnt) > npix4)
                {
                    /* we are better off storing uncompressed */
                    perror("data too random\n");
	            exit(9);
                }
	    }
	}
	cval = nval;			// current value

        if(cval >= 0 && ! nostats)
        {
	        /* do some of our own stats */
                 ++valid_pixels;
                sum += cval;
                isum += cval;
                if(cval>max) {
                    max=cval;
                    max_x = i % xpixels;
                    max_y = i / xpixels;
                }
                if(cval<min || i==0) min=cval;
		float1=cval;
		sumsq += float1*float1;
        }
    }
    /* compute compressed size */
    compressed_size = (int) ( bitshuffle.int8ptr - (int8_t *) outimage );
    /* end of compression */
    if(print_timing && outfile != stdout)
    {
        printf("compression done. delta-T: %g\n",get_time()-starttime);
        printf("sum: %ld max: %ld (@ %d %d) min: %ld valid_pixels: %ld\n",isum,max,max_x,max_y,min,valid_pixels);
    }

    avg = sum/pixels;
    rms = sqrt(sumsq/pixels);
    if(outfile != stdout) printf("mean = %g rms = %g\n",avg,rms);

    if(outfile != stdout) printf("writing %s as a %d-byte header and %d-bytes of cbf\n", 
          outfilename,outheader_bytes,compressed_size);
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

    if(print_timing && outfile != stdout) printf(" about to write, delta-T: %g\n",get_time()-starttime);

    char digest[25] = "N/D";
    if(! nomd5check)
    {
	if(print_timing && outfile != stdout) printf("starting MD5sum delta-T: %g\n",get_time()-starttime);
	// calculate the Eikenberry md5 checksum
	MD5_CTX context;
	unsigned char rawdigest[17];

	MD5Init(&context);
	MD5Update(&context, outimage, compressed_size);
	MD5Final(rawdigest, &context);
	cbf_md5digest_to64 (digest, rawdigest);
    }

    fprintf(outfile,"--CIF-BINARY-FORMAT-SECTION--\r\n");
    fprintf(outfile,"Content-Type: application/octet-stream;\r\n");
    fprintf(outfile,"     conversions=\"x-CBF_BYTE_OFFSET\"\r\n");
    fprintf(outfile,"Content-Transfer-Encoding: BINARY\r\n");
    fprintf(outfile,"X-Binary-Size: %d\r\n",compressed_size);
    fprintf(outfile,"X-Binary-ID: 1\r\n");
    fprintf(outfile,"X-Binary-Element-Type: \"signed 32-bit integer\"\r\n");
    fprintf(outfile,"X-Binary-Element-Byte-Order: LITTLE_ENDIAN\r\n");
    fprintf(outfile,"Content-MD5: %s\r\n",digest);
    fprintf(outfile,"X-Binary-Number-of-Elements: %d\r\n",pixels);
    fprintf(outfile,"X-Binary-Size-Fastest-Dimension: %d\r\n",xpixels);
    fprintf(outfile,"X-Binary-Size-Second-Dimension: %d\r\n",ypixels);
    fprintf(outfile,"X-Binary-Size-Padding: 4095\r\n\r\n\f\032\004\325");
    if(print_timing && outfile != stdout) printf("detpixels: %d %d\n",xpixels,ypixels);

    fwrite(outimage,compressed_size+4095,1,outfile);

    /* does anybody use this? */
    fprintf(outfile,"\r\n--CIF-BINARY-FORMAT-SECTION----\r\n;\r\n\r\n");

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



