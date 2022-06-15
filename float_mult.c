
/* multiply two binary "float" files together						-James Holton		9-20-16

example:

gcc -O -O -o float_mult float_mult.c -lm
./float_mult file1.bin file2.bin output.bin 

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


char *infile1name = "";
char *infile2name = "";
FILE *infile1 = NULL;
FILE *infile2 = NULL;
char *outfilename = "output.bin\0";
FILE *outfile = NULL;

int main(int argc, char** argv)
{
     
    int n,i,j,k,pixels;
    float *outimage;
    float *inimage1;
    float *inimage2;
    float power1=1.0,power2=1.0;
    float sum,sumd,sumsq,sumdsq,avg,rms,rmsd,min,max;
        
    /* check argument list */
    for(i=1; i<argc; ++i)
    {
	if(strlen(argv[i]) > 4)
	{
	    if(strstr(argv[i]+strlen(argv[i])-4,".bin"))
	    {
		printf("filename: %s\n",argv[i]);
		if(infile1 == NULL){
		    infile1 = fopen(argv[i],"r");
		    if(infile1 != NULL) infile1name = argv[i];
		}
		else
		{
		    if(infile2 == NULL){
		        infile2 = fopen(argv[i],"r");
			if(infile2 != NULL) infile2name = argv[i];
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
            if(strstr(argv[i], "-power1") && (argc >= (i+1)))
            {
                power1 = atof(argv[i+1]);
            }
            if(strstr(argv[i], "-power2") && (argc >= (i+1)))
            {
                power2 = atof(argv[i+1]);
            }
        }
    }

    if(infile1 == NULL || infile2 == NULL){
	printf("usage: float_mult file1.bin file2.bin [outfile.bin] -power1 1.0 -power2 1.0\n");
	printf("options:\n");\
//        printf("\t-atom\tnumber of atoms in the following file\n");
//	printf("\t-file filename.txt\ttext file containing point scatterer coordinates in Angstrom relative to the origin.  The x axis is the x-ray beam and Y and Z are parallel to the detector Y and X coordinates, respectively\n");
exit(9);
    }


    /* load first float-image */
    fseek(infile1,0,SEEK_END);
    n = ftell(infile1);
    rewind(infile1);
//    printf("allocating %d bytes\n",n);
    inimage1 = calloc(n,1);
    if(inimage1 == NULL) {
	perror(argv[0]);
	exit(9);
    }
//    printf("allocating %d bytes\n",n);
    inimage2 = calloc(n,1);
    if(inimage2 == NULL) {
	perror(argv[0]);
	exit(9);
    }
    fread(inimage1,n,1,infile1);
    fclose(infile1);
    fread(inimage2,n,1,infile2);
    fclose(infile2);

    pixels = n/sizeof(float);
    outfile = fopen(outfilename,"wb");
    if(outfile == NULL)
    {
	printf("ERROR: unable to open %s\n", outfilename);
	exit(9);
    }
    

//    printf("allocating %d bytes\n",n);
//    outimage = calloc(pixels,sizeof(float));
    /* re-use one of these */
    outimage = inimage1;
    if(outimage == NULL) {
	perror(argv[0]);
	exit(9);
    }
    sum = sumsq = sumd = sumdsq = 0.0;
    min = 1e99;max=-1e99;
    for(i=0;i<pixels;++i)
    {
	if(inimage1[i]<0.0 && power1 != ((int) power1) ) inimage1[i] = 0.0;
	if(inimage2[i]<0.0 && power2 != ((int) power2) ) inimage2[i] = 0.0;
	outimage[i] = powf(inimage1[i],power1) * powf(inimage2[i],power2);
	if(outimage[i]>max) max=outimage[i];
	if(outimage[i]<min) min=outimage[i];
	sum += outimage[i];
	sumsq += outimage[i]*outimage[i];
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


    printf("writing %s as %d %d-byte floats\n",outfilename,pixels,sizeof(float));
    outfile = fopen(outfilename,"wb");
    fwrite(outimage,pixels,sizeof(float),outfile);
    fclose(outfile);


    return 0;
}

