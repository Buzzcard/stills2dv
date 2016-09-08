/***********************************************************************************************
 *
 * stills2dv.h
 *
 * Author: Denis-Carl Robidoux
 *
 * Copyrights and license: GPL v3 (see file named gpl-3.0.txt for details
 *
 *
 *
 *
 ***********************************************************************************************/


#ifndef true
#define true (-1)
#define false 0
#endif


#define TRACE
// #define TRACE printf("%s();\n", __FUNCTION__)


// This is the internal image format, very simple..  24 bits RGB, image is in 1 block
typedef struct {
  char *fn;
  int width;
  int height;
  int serial;
  unsigned char *data;
}Image;

double smoothed_step (double start, double end, int count, double total, double smoothratio);

int writejpg(Image *, char *);
Image *readjpg(char *);

int writeppm(Image *, char *);
Image *readppm(char *);

Image *readpng(char *fn);

void show_image(Image *im);

