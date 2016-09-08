/***********************************************************************************************
 *
 * s2d_png.c
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
#include <png.h>

#include "stills2dv.h"

Image *readpng(char *fn)
{
  unsigned char header[8]={0};
  int x, y, width, height;
  unsigned char *row;

  png_structp png_ptr;
  png_infop info_ptr;
  unsigned char  ** row_pointers;
  unsigned char *data;
  Image *img;
  FILE *in;
  TRACE;
  in = fopen(fn, "rb");
  if (!in)
    {
      fprintf(stderr," File %s could not be opened for reading", fn);
      return NULL;
    }
  if (fread(header, 1, 8, in)!=8)
    {
      fprintf(stderr, "Failed reading header of %s\n", fn);
      fclose(in);
      return NULL;
    }
  if (png_sig_cmp(header, 0, 8))
    {
      fprintf(stderr," File %s is not recognized as a PNG file", fn);
      fclose(in);
    }

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	
  if (!png_ptr)
    {
      fprintf(stderr," png_create_read_struct failed");
      fclose(in);
      return NULL;
    }

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    {
      fprintf(stderr," png_create_info_struct failed");
    }

  if (setjmp(png_jmpbuf(png_ptr)))
    {
      fprintf(stderr," Error during init_io");
    }
  png_init_io(png_ptr, in);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);
  width = info_ptr->width;
  height = info_ptr->height;
  png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  if (setjmp(png_jmpbuf(png_ptr)))
    {
      fprintf(stderr," Error during read_image");
    }
  if((row_pointers = (unsigned char **) malloc(sizeof(unsigned char *) * height))==NULL)
    {
	fprintf(stderr, "Out of memory allocating rows index for png\n");
	png_destroy_info_struct(png_ptr, &info_ptr);
	//png_destroy_struct(png_ptr);
	return NULL;
    }
  for (y=0; y<height; y++)
    if((row_pointers[y] = (unsigned char *) malloc(info_ptr->rowbytes))==NULL)
      {
	fprintf(stderr, "Out of memory allocating rows for png\n");
	while(y>1)
	  {	   
	    free(row_pointers[--y]);
	  }
	free(row_pointers);
	png_destroy_info_struct(png_ptr, &info_ptr);
	//png_destroy_struct(png_ptr);
	return NULL;
      }
  png_read_image(png_ptr, row_pointers);
  fclose(in);
  if((img=(Image *)malloc(sizeof(Image)))==NULL)
    {
      fprintf(stderr, "Out of memory creating header while opening png\n");
      png_destroy_info_struct(png_ptr, &info_ptr);
      //png_destroy_struct(png_ptr);
      for (y=0; y<height; y++)
	free(row_pointers[y]);
      free(row_pointers);     
      return NULL;
    }
  memset(img, 0, sizeof(Image)); 

  if((img->data=(unsigned char *)malloc(width * height * 3))==NULL)
    {
      fprintf(stderr, "Out of memory for image while opening png\n");
      png_destroy_info_struct(png_ptr, &info_ptr);
      //png_destroy_struct(png_ptr);
      for (y=0; y<height; y++)
	free(row_pointers[y]);
      free(row_pointers);     
      free(img);
      return NULL;      
    }
  if(info_ptr->color_type == PNG_COLOR_TYPE_RGB)
    {
      
      for (y=0;y<height;y++)
	{	  
	  memcpy(&img->data[y*width*3], row_pointers[y], width*3);
	}
    }else if (info_ptr->color_type == PNG_COLOR_TYPE_RGBA)
    {
      data=img->data;
      for (y=0;y<height;y++)
	{
	  row=row_pointers[y];
	  for(x=0;x<width;x++)
	    {
	      *(data++)=*(row++);
	      *(data++)=*(row++);
	      *(data++)=*(row++);
	      row++;
	    }
	}
    }else if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY)
    {
      data=img->data;
      for (y=0;y<height;y++)
	{
	  row=row_pointers[y];
	  for(x=0;x<width;x++)
	    {
	      *(data++)=*row;
	      *(data++)=*row;
	      *(data++)=*(row++);
	    }
	}
    }else if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
      data=img->data;
      for (y=0;y<height;y++)
	{
	  row=row_pointers[y];
	  for(x=0;x<width;x++)
	    {
	      *(data++)=*row;
	      *(data++)=*row;
	      *(data++)=*(row++);
	      row++;
	    }
	}
    }else
    {
      fprintf(stderr, "Unsupported PNG format, we do not support indexed color (yet)\n");
      png_destroy_info_struct(png_ptr, &info_ptr);
      //png_destroy_struct(png_ptr);
      for (y=0; y<height; y++)
	free(row_pointers[y]);
      free(row_pointers);     
      free(img->data);
      free(img);
      return NULL;
    }
  img->width=width;
  img->height=height;
  png_destroy_info_struct(png_ptr, &info_ptr);
  //png_destroy_struct(png_ptr);
  for (y=0; y<height; y++)
    free(row_pointers[y]);
  free(row_pointers);     
  img->fn=strdup(fn);
  return img;
}
