/***********************************************************************************************
 *
 * s2d_ppm.c
 *
 * Author: Denis-Carl Robidoux
 *
 * Copyrights and license: GPL v3 (see file named gpl-3.0.txt for details
 *
 *
 *
 *
 ***********************************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "stills2dv.h"


/* This function can only read the ppms of the format produced by this program */
Image *readppm(char *filename)
{
  FILE *in;
  Image *im;
  char *c;
  char line[2048];
  char header[]="P6\n";
  char depthstring[]="255\n";
  long position=0;
  int ret, img_dta_siz;
  TRACE;  
  if((im=malloc(sizeof(Image)))==NULL)return NULL;
  if((in=fopen(filename, "rb"))==NULL)
    {
      free(im);
      return NULL;
    }
  if(fgets(line, 2048,in)==NULL)
    {
      free(im);
      fclose(in);
      return NULL;
    }
  position+=strlen(line);
  if(strcmp(line,header)!=0)
    {
      free(im);
      fclose(in);
      return NULL;
    }
  do
    {
      if(fgets(line, 2048,in)==NULL)
	{
	  free(im);
	  fclose(in);
	  return NULL;
	}
      position+=strlen(line);
    }while (line[0]=='#');
  if((c=strstr(line, " "))==NULL)
    {
      free(im);
      fclose(in);
      return NULL;
    }
  c[0]=0;
  c++;
  im->width=atoi(line);
  im->height=atoi(c);
  if((im->width<4)||(im->height<4))
    {
      free(im);
      fclose(in);
      return NULL;
    }
  position+=strlen(depthstring);
  if((im->data=malloc(im->width * im->height * 3))==NULL)
    {
      free(im);
      fclose(in);
      return NULL;
    }
  fseek(in, position, SEEK_SET);
  img_dta_siz=im->width * im->height * 3;
  if ((ret=fread(im->data, 1, img_dta_siz, in))!=img_dta_siz)
    {
      if(ret < 0){
	fprintf(stderr, "error #%d reading %s\n", ret, filename);
      } else {
	fprintf(stderr, "short count reading %s, received %d bytes when %d were expected\n", filename, ret, img_dta_siz);
      }      
    }
  fclose(in);
  im->fn=strdup(filename);
  return im;
}

int writeppm(Image *im, char *filename)
{
  FILE *out;
  char str[128];
  TRACE;
  if((out=fopen(filename, "wb"))==NULL)
    {
      fprintf(stderr, "could not open %s for writing\n", filename);
      return -1;
    }
  sprintf(str, "P6\n#this ppm was created by stills2dv\n%d %d\n255\n",im->width, im->height);
  fwrite(str, 1, strlen(str), out);
  fwrite(im->data, 1, im->width * im->height * 3, out);
  fclose(out);
  return 0;
}

