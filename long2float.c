
/* convert binary " int" file to floats                                           -James Holton  2-22-16

example:

gcc -O -O -o long2float long2float.c -lm
./int_float file1.img floatfile.bin

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


char *infile1name = "";
FILE *infile1 = NULL;
char *outfilename = "floatfile.bin\0";
FILE *outfile = NULL;

int main(int argc, char** argv)
{

    int n,i,j,k,pixels;
    float *outimage;
     int *inimage1;
    float float1, float2, float3, float4;
    int header=512;
    int overflows=0;
    int underflows=0;
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
                header = atof(argv[i+1]);
            }
        }
    }

    if(infile1 == NULL ){
        printf("usage: long2float file1.img [outfile.bin] -header 512\n");
        printf("options:\n");\
        printf("\t-header \tnumber of bytes to skip in header of each file\n");
        exit(9);
    }


    /* load first  int-image */
    fseek(infile1,0,SEEK_END);
    n = ftell(infile1);
    pixels = (n-header)/sizeof( int);
    fseek(infile1,header,SEEK_SET);
    inimage1 = calloc(pixels,sizeof( int));
    fread(inimage1,pixels,sizeof( int),infile1);
    fclose(infile1);

    outfile = fopen(outfilename,"wb");
    if(outfile == NULL)
    {
        printf("ERROR: unable to open %s\n", outfilename);
        exit(9);
    }

    printf("header = %d bytes  %d %d-byte pixels\n",header,pixels,sizeof( int));

    outimage = calloc(pixels,sizeof(float));

    sum = sumsq = sumd = sumdsq = 0.0;
    min = 65535.0;
    max = 0.0;

    for(i=0;i<pixels;++i)
    {
        float1 = (float) inimage1[i];
	outimage[i] = float1;
        if(float1>max) max=float1;
        if(float1<min) min=float1;
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
    printf("max = %g min = %g\n",max,min);
    printf("mean = %g rms = %g rmsd = %g\n",avg,rms,rmsd);

    printf("writing %s as a %d %ld-byte floats\n",outfilename,pixels,sizeof(float));
    outfile = fopen(outfilename,"wb");
    fwrite(outimage,pixels,sizeof(float),outfile);
    fclose(outfile);


    return 0;
}

