/*
**
**	adsc2pgm  v1.3.1										James Holton 4-7-14
**
**	converts a region of an x-ray diffraction image in the ADSC "SMV" format 
**	into a portable greymap (text) image file.
**
**	this output can then easily be converted to a jpeg, etc.
**
**	compile this file with: 
**		cc -o adsc2pgm adsc2pgm.c -lm -static
**
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define TRUE 1
#define FALSE 0

/* global notifier of frame byte-order */
int swap_bytes = FALSE;

/* frame handling routines */
FILE *GetFrame(char *filename);
double ValueOf( const char *keyword, FILE *smvfile);

main(int argv, char** argc)
{
    /* exit status */
    int return_code = 0;

    /* utility variables */
    int i;
    char *filename;
    FILE *frame = NULL;
    unsigned short int **image;

    /* frame information variables */
    int record_size;
    int num_records;
    int pixels_per_line;
    int image_width;
    int image_height;
    
    /* output image variables */
    int xsize;
    int ysize;

    /* indicies */
    int x, y;
    int out_x, out_y;
    int overloads = 0;
    
    /* limits */
    int high = 0;
    int low = 0xFFFF;
    double sum = 0;
    double sigma = 0;
    int stat_pixels = 0;
    double scale = 0;
    double zoom = 1;
    int negate  = 0;
    int start_x = 0;
    int start_y = 0;
    int  stop_x = 0;
    int  stop_y = 0;
    int box_offset_x = 0;
    int box_offset_y = 0;
    
    /* storage */
    unsigned short int *record;
    unsigned short int *swapped;
    unsigned short int *temp;

    
    /* output PGM stuff */
    unsigned char *PGMdata;
    FILE *pgmout = NULL;
    char outfilename[1024] = "new.pgm";

    /* begin progam here */
    printf("ADSC2PGM 1.3.1\t\tby James Holton 4-7-14\n\n");
    
    /* check argument list */
    for(i=1; i<argv; ++i)
    {
	if(strstr(argc[i], ".img"))
	{
	    /* filename specified */
	    if(frame == NULL)
	    {
		/* mimic input filename for output */
		filename = argc[i];
		while((char *) strchr(filename, '/') != NULL)
		{
		    /* how come basename doesn't work? */
		    filename = (char *) strchr(filename, '/') + 1;
		}

		strncpy(outfilename, filename, strlen(filename)-4);
		strcat(outfilename, ".pgm");

		filename = argc[i];
		frame = GetFrame(filename);
	    }
	}
	if(strstr(argc[i], ".pgm"))
	{
	    strcpy(outfilename, argc[i]);
	}
	if(argc[i][0] == '-')
	{
	    /* option specified */
	    if(strstr(argc[i], "-negate")) negate = 1;
	    if(strstr(argc[i], "-invert")) negate = 1;
	    if(strstr(argc[i], "-scale") && (argv >= (i+1)))
	    {
		scale = atof(argc[i+1]);
	    }
	    if(strstr(argc[i], "-zoom") && (argv >= (i+1)))
	    {
		zoom = atof(argc[i+1]);
	    }
	    if(strstr(argc[i], "-box") && (argv >= (i+4)))
	    {
		start_x = atol(argc[i+1]);
		start_y = atol(argc[i+2]);
		 stop_x = atol(argc[i+3]);
		 stop_y = atol(argc[i+4]);
	    }
	    if(strstr(argc[i], "-adxvbox") && (argv >= (i+4)))
	    {
		start_y = atol(argc[i+1]);
		start_x = atol(argc[i+2]);
		 stop_y = atol(argc[i+3]);
		 stop_x = atol(argc[i+4]);
	    }
	}
    }
    
    if(frame != NULL)
    {
	/* Print intentions */
	printf("converting %s to %s\n", filename, outfilename);
	printf("scaling size by %g\n", zoom); /*  */
	if(scale == 0)
	{
	    printf("autoscaling intensity \n");
	}
	else
	{
	    printf("intensity scale set to %g\n", scale);
	}
	printf("\n");	

	/* Filename */
	printf("reading %s ", filename);

	record_size     = (int) 2*ValueOf("SIZE1",frame);
	num_records     = (int) ValueOf("SIZE2",frame);
	pixels_per_line = (int) ValueOf("SIZE1",frame);
	
	/* handle weird box definitions */
	if(stop_x < start_x) {i = start_x; start_x = stop_x; stop_x = i;}
	if(stop_y < start_y) {i = start_y; start_y = stop_y; stop_y = i;}
	/* default to whole image */
	if(stop_x == 0) stop_x = num_records;
	if(stop_y == 0) stop_y = pixels_per_line;
	printf("box from (%d,%d) to (%d,%d)\n", start_x, start_y, stop_x, stop_y);
	
	/* preserve output image size (for later) */
	xsize = (int) (zoom* (double) (stop_x - start_x));
	ysize = (int) (zoom* (double) (stop_y - start_y));
	box_offset_x = start_x;
	box_offset_y = start_y;
	
	/* clip edges (for reading only) */
	if(stop_x > num_records)     stop_x = num_records;
	if(stop_y > pixels_per_line) stop_y = pixels_per_line;
	if(start_x < 0)              start_x = 0;
	if(start_y < 0)              start_y = 0;
	image_width  = stop_x - start_x;
	image_height = stop_y - start_y;
	box_offset_x -= start_x;
	box_offset_y -= start_y;
	
	/* allocate space for memeory-resident image */
	image = (unsigned short int **) calloc(sizeof(unsigned short int *), image_width);

	/* need a little extra memory for swapping bytes */
	if(swap_bytes == TRUE)
	{
	    swapped = calloc(2, image_height);
	    printf("swapping bytes...\n");
	}
	
	/* load appropriate records (lines) into memory */
	for(x = 0; x < image_width; ++x)
	{
	    /* allocate space for this line */
	    record = calloc(2, image_height);

	    /* position file at beginning of the desired scan line */
	    fseek(frame, 512+((x+start_x)*record_size)+(start_y*2), SEEK_SET);
	
	    /* read the line into memory */
	    fread(record, 2, image_height, frame);
	
	    /* Account for byte swapping */
	    if(swap_bytes == TRUE)
	    {
		/* preserve location of read-in pixels */
		temp = record;
	    
		/* swab() can only COPY swapped bytes */
		swab((char*)record, (char*)swapped, image_height*2);
		record = swapped;
	    
		/* exchange buffers for next time */
		swapped = temp;
	    }

	    /* enter this record into the image */
	    image[x] = record;
	    
	    /* do statistics on pixel values */
	    for(y = 0; y < image_height; ++y)
	    {
		/* initialize from words in file */
		if(record[y] == 0xFFFF)
		{
		    ++overloads;
		}
		if((record[y] != 0)&&(record[y] != 0xFFFF))
		{
		    ++stat_pixels;
		    sigma += ( (double) record[y]) * ((double) record[y]);
		}

		/* debug *
		printf("reading (%d,%d) ", x, y);
		printf("from (%d+%d)*%d+%d*2+512 ", x, start_x, record_size, start_y );
		printf(" = %d  \n", image[x][y]);  /*  */
	    }
	}
	
	/* image has now been completely read into memory */
	fclose(frame);
	
	/* finish statistics */
	if(stat_pixels == 0)
	{
	    /* no data read? */
	    stat_pixels=1; sigma=1;
	}
	
	sigma /= (double) (stat_pixels);
	sigma = sqrt(sigma);
	
	/* autoscale output image to 5-sigma cuttoff */
	if(scale == 0)
	{
	    scale = 256.0/(5*sigma);
	    printf("intensity scale set to %g\n", scale);
	}
	
	
	
	/* convert and copy all image data into new PGM file */
	PGMdata = (unsigned char *) calloc(xsize, ysize);

	
	printf("transforming data ...\n");
	for(out_y = 0; out_y < ysize; ++out_y)
	{
	    for(out_x = 0; out_x < xsize; ++out_x)
	    {
		/* transform output coordinates to input image frame (pixel units) */
		x = (int) (out_x/zoom) + box_offset_x;
		y = (int) (out_y/zoom) + box_offset_y;
		
		/* debug *
		printf("xform (%d,%d) -> (%d,%d) = ", out_x, out_y, x, y);
		/* printf("%d\n", image[x][y]); /*  */
		
		/* check for out-of-bounds data */
		if((x<0)||(y<0)||(y>=image_height)||(x>=image_width))
		{
		    /* slam non-image output to 1 */
		    sum = 1.0;
		}
		else
		{
		    /* transform frame image */
		    sum = scale*image[x][y];
		    /* reserve zero for overloads */
		    if(sum < 1) sum = 1;
		    /* burn-thru effect for overloads */
		    if(image[x][y] == 0xFFFF) sum = 0;
		}
		/* clip output intensity */
		if(sum > 255) sum = 255;
		/* optional negative image */
		if(negate) sum = 255-sum;
		
		PGMdata[(out_x*ysize)+out_y] = (unsigned char) sum;
	    }
	}
    
	/* write out the PGM image */
	pgmout = fopen(outfilename, "wb");
	if(pgmout != NULL)
	{
	    printf("writing %s\n", outfilename);
	    fprintf(pgmout, "P2\n%d %d\n255\n", xsize, ysize);
	    fprintf(pgmout, "# (%d,%d) to (%d,%d) on %s\n", start_x, start_y, stop_x, stop_y, filename);
	    fprintf(pgmout, "# pixels scaled by %f and zoomed in %fX\n", scale, zoom);
	    
	    /* print out bytes as text integers */
	    i=0;
	    for(y = ysize-1; y >= 0; --y)
	    {
		for(x = xsize-1; x >= 0; --x)
		{
		    fprintf(pgmout, "%3d ", PGMdata[(x*ysize)+y]);
		    
		    /* wrap long lines as per PGM definition */
		    ++i;
		    if((i%20==0)||((i%xsize) == (xsize-1)))
		    {
			fprintf(pgmout, "\n");
			i=0;
		    }
		}
	    }
	    
	    fclose(pgmout);
	}
	else
	{
	    printf("ERROR: could not open %s\n", outfilename);
	}

    }
    else
    {
	printf("usage: %s framefile.img [outfile.pgm] [-zoom factor] [-box x1 y1  x2 y2] [-negate]\n", argc[0]);
		
	printf("\n\t framefile.img - the ADSC CCD image file you want to convert\n");
	printf(  "\t   outfile.pgm - name of output PGM (portable graymap) file.\n");
	printf(  "\t        factor - zoom in by this factor.\n");
	printf(  "\t  x1 y1  x2 y2 - corners of box to convert (original image pixel coordinates).\n");
	printf(  "\t       -negate - black spots on white background\n");
	printf("\n");
	printf(  "NOTE:\t denzo/mosflm (X,Y) detector coordinates are used here.");
	printf("\n");
		
	/* report unhappy ending */
	return_code = 2;
    }
    
    return return_code;
}


FILE *GetFrame(char *filename)
{
    FILE *frame;
    unsigned char header[512];
    unsigned char *string;
    unsigned char *byte_order;
    
    typedef union
    {
	unsigned char string[2];
	unsigned short integer;
    } TWOBYTES;
    TWOBYTES twobytes;
    twobytes.integer = 24954;
    
    frame = fopen(filename, "rb");

    if(frame != NULL)
    {
	fread(header, 1, 512, frame);

	/* What kind of file is this? */
	string = header;

	if(0!=strncmp(string, "{\nHEADER_BYTES=  512;\nDIM=2;\nBYTE_ORDER=", 12))
	{
	    /* probably not an ADSC frame */

	    /* inform the user */
	    printf("ERROR: %s does not look like an ADSC frame!\n", filename);
	    /* skip this file */
	    fclose(frame);
	    
	    frame = NULL;
	}
	else
	{
	    /* determine byte order on this machine */
	    if(0==strncmp(twobytes.string, "az", 2))
	    {
		byte_order = "big_endian";
	    }
	    else
	    {
		byte_order = "little_endian";
	    }
	    
	    /* see if we will need to swap bytes */
	    string = (char *) strstr(header, "BYTE_ORDER=")+11;
	    /* find last instance of keyword in the header */
	    while ((char *) strstr(string, "BYTE_ORDER=") != NULL)
	    {
		string = (char *) strstr(string, "BYTE_ORDER=")+11;
	    }
	    if(0==strncmp(byte_order, string, 10))
	    {
		swap_bytes = FALSE;
	    }
	    else
	    {
		swap_bytes = TRUE;
	    }
	}
    }
    else
    {
	/* fopen() failed */
	perror("adsc2pgm");
    }
    
    return frame;
}

/* read floating-point values from keywords in an SMV header */
double ValueOf(const char *keyword, FILE *frame)
{
    double value;
    char header[600];
    char *string;
    int keylen = strlen(keyword);

    /* read the header in from the file */
    fseek(frame, 1, SEEK_SET);

    fread(header, 1, 512, frame);
    string = header;

    /* find first instance of keyword in the header */
    /* string = (char *) strstr(header, keyword);
    string = string + keylen;
    /* find last instance of keyword in the header */
    while ((char *) strstr(string, keyword) != NULL)
    {
	string = (char *) strstr(string, keyword)+keylen;
    }
    if(string == header) return 0.0;

    /* advance to just after the "=" sign */
    string = (char *) strstr(string, "=");
    if(string == NULL) return 0.0;
    ++string;

    value = atof(string);

    return value;
}
