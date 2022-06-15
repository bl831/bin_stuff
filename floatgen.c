/*
**
**      floatgen.c  v0.1                                             James Holton 5-19-11
**
**      converts text number on stdin to a 4-byte float on stdout
**
**      compile this file with:
**              gcc -o floatgen floatgen.c -lm -static
**
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argv, char** argc)
{
    float fourbytes;
    char text[1024];
    char *token;
    const char delimiters[] = " \t,;:!";
    const char numberstuf[] = "0123456789-+.EGeg";

    while ( fgets ( text, sizeof text, stdin ) != NULL ) {

        token = text;
        token += strspn(token,delimiters);
        if(strcmp(token,"\n")==0) {
            //printf("blank\n");
            continue;
        }

        fourbytes=atof(token);
 
	fwrite(&fourbytes,sizeof(float),1,stdout);
    }
}
