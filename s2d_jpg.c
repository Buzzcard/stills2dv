/***********************************************************************************************
 *
 * s2d_jpg.c
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
#include <jpeglib.h>
#include <math.h>


#include "stills2dv.h"

extern int jpegquality;

int writejpg(Image *im, char *filename)
{
  struct jpeg_compress_struct cinfo;   
  struct jpeg_error_mgr jerr;   
  FILE * outfile;       /* target file */   
  JSAMPROW row_pointer[1];  /* pointer to JSAMPLE row[s] */   
  int row_stride;       /* physical row width in image buffer */   
  TRACE;
  cinfo.err = jpeg_std_error(&jerr);   
  jpeg_create_compress(&cinfo);   
  if ((outfile = fopen(filename, "wb")) == NULL) {   
    fprintf(stderr, "can't open %s\n", filename);   
    exit(1);
  }
  jpeg_stdio_dest(&cinfo, outfile);   
  cinfo.image_width = im->width;  /* image width and height, in pixels */   
  cinfo.image_height = im->height;   
  cinfo.input_components = 3;       /*  of color components per pixel */   
  cinfo.in_color_space = JCS_RGB;   /* colorspace of input image */   
  jpeg_set_defaults(&cinfo);   
  jpeg_set_quality(&cinfo, jpegquality, TRUE /* limit to baseline-JPEG values */);   
  jpeg_start_compress(&cinfo, TRUE);   
  row_stride = im->width * 3; /* JSAMPLEs per row in data */   
      
  while (cinfo.next_scanline < cinfo.image_height) {   
    row_pointer[0] = & im->data[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);   
  }   
  jpeg_finish_compress(&cinfo);
  fclose(outfile);   
  jpeg_destroy_compress(&cinfo);   
  return 0;
}

Image *readjpg(char *fn)
{ 
  FILE *in;
  int i,bytecount;
  Image *res;
  unsigned long location=0;
  unsigned char *ptr;
  struct jpeg_decompress_struct cinfo={0};
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  //printf("openImage(%s);\n", fn);
  TRACE;
  if((in=fopen(fn, "rb"))==NULL)
    {
      fprintf(stderr, "Could not open %s for reading\n", fn);
      return NULL;
    }
  if((res=malloc(sizeof(Image)))==NULL)
    {
      fprintf(stderr, "Out of memory, could not alloc small struct\n");
      fclose(in);
      return NULL;
    }
  memset(res, 0, sizeof(Image)); 
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, in);
  if(jpeg_read_header(&cinfo, true)!=JPEG_HEADER_OK)
    {
      fprintf(stderr, "Error reading jpeg header of %s\n", fn);
      free(res);
      return NULL;
    }
  res->width=cinfo.image_width;
  res->height=cinfo.image_height;
  //printf("jpeg_read_header returned JPEG_HEADER_OK with image_width=%d and image_height=%d\n", res->width, res->height);
  //  printf("about to start decompress\n");
  jpeg_start_decompress( &cinfo );
  if((res->data=malloc(cinfo.output_width * cinfo.output_height * cinfo.num_components ))==NULL)
    {
      fprintf(stderr, "Out of memory, could not load picture %s with its %d x %d pixels\n", fn, res->width, res->height);
      jpeg_finish_decompress(&cinfo);
      free(res);
      return NULL;
    }
  // printf("about to alloc a row\n");
  if((row_pointer[0] = (unsigned char *)malloc( cinfo.output_width*cinfo.num_components ))==NULL)
    {
      fprintf(stderr, "Out of memory, could not allocate row pointer\n");
      free(res->data);
      free(res);
      jpeg_finish_decompress(&cinfo);
      return NULL;
    }
  // printf("about to scan\n");
  while( cinfo.output_scanline < res->height )
    {
      jpeg_read_scanlines( &cinfo, row_pointer, 1 );
      for( i=0; i<res->width*cinfo.num_components;i++) 
	res->data[location++] = row_pointer[0][i];
    }
  free(row_pointer[0]);
  // printf("checking num_components\n");
  if(cinfo.num_components==1)
    {
      // for speed performance issue at resizing we'll convert now monochromous jpgs in RGBs
      bytecount=cinfo.output_width * cinfo.output_height * 3;
      if((ptr=malloc(bytecount))==NULL)
	{
	  fprintf(stderr, "Out of memory, could not load picture %s with its %d x %d pixels\n", fn, res->width, res->height);
          jpeg_finish_decompress(&cinfo);
	  free(res->data);
	  free(res);
	  return NULL;
	}
      for (i=0;i<bytecount;i++)
	{
	  ptr[i]=res->data[i/3];
	}
      free(res->data);
      res->data=ptr;
    }
  // printf("About to fclose(in);\n");
  fclose(in);
  res->fn=strdup(fn);
  // printf("About to finish_decompress;\n");
  jpeg_finish_decompress(&cinfo);
  // printf("About to return with res=%p and res->data=%p\n", res, res->data);
  return res;
}
