/***********************************************************************************************
 *
 * s2d_main.c
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
#include <png.h>
#include <math.h>


#include "stills2dv.h"


#define CENTER      -2000000001
#define LEFT        -2000000002
#define RIGHT       -2000000003
#define TOP         -2000000004
#define BOTTOM      -2000000005
#define FITWIDTH    -2000000006
#define FITHEIGHT   -2000000007
#define FIT         -2000000008
#define TOPRIGHT    -2000000009
#define TOPLEFT     -2000000010
#define BOTTOMRIGHT -2000000011
#define BOTTOMLEFT  -2000000012

#define PPM_FORMAT 0
#define JPG_FORMAT 1

static int outputwidth=720;
static int outputheight=480;
static int showoutput=false;
static int fastrender=false;
static int motionblur=true;

int jpegquality=90;

// just an improbable value so we can determinate that there is no value to read.
#define LOST_IN_THE_WOOD -2147483646



static Image *im=NULL;
static int *prev=NULL;
static int prev_allocated=0;


typedef struct
{
  char *filename;
  int width;
  int height;
  int startx;
  int starty;
  int endx;
  int endy;
  int zscode;
  float zoomstart;
  int zecode;
  float zoomend;
  float duration;
  int crossfade;
  int crossframes;
  float startsmooth;
  float endsmooth;
}Movement;

// defaults

static float framerate=30.00;
static int outputformat=PPM_FORMAT;
static int defaultcolor=0x000000;

static double pansmoothratio=0.2;
static double zoomsmoothratio=0.2;


static int sharpness=1.0;

static int framecount=0;
static char *tmpdir=NULL;
static char *usage="stills2dv [-tmpdir <temp dir>] [-showoutput] [-fastrender ^ -nomotionblur] <scriptfile>\n";
static char lastfilename[2048];
static int lastsx= -1, lastsy, lastx, lasty;
static char **split(char *);
static Movement ms;

FILE *scriptfile=NULL;
FILE *output=NULL;


char biggestblowfilename[2048];
int biggestzoom, biggestx, biggesty;
float biggestarea=0.0;


int stricmp(char *a, char *b)
{
  if (a==b)return 0;
  if (a==NULL)return -1;
  if (b==NULL) return 1;
  do
    {
      if(toupper(*a) > toupper(*b))return  1;
      if(toupper(*b) > toupper(*a))return -1;
      a++;
      b++;      
    }while((*a != 0) || (*b != 0));
  return 0;
}


static void clear_prev_buffer(void)
{
  int i;
  for (i=0;i<(prev_allocated / sizeof(int)); i++)
    {

      prev[i]=LOST_IN_THE_WOOD;

    }

}
static int check_prev_buffer(int width, int height)
{
  int size=width * height * sizeof(int) * 2; 
  if(prev_allocated < size)
    {
      if(prev!=NULL)free(NULL);
      if((prev=malloc(sizeof(int)* 2 * width * height))==NULL)
	{
	  fprintf(stderr, "Out of memory trying to allocate for the prev buffer\n");
	  exit(0);
	}
      prev_allocated=size;
      clear_prev_buffer();
    }
  return 0;
}

static void releaseImage(Image *im)
{
  TRACE;
  // printf("Releasing %p\n", im);
  if (im==NULL)return;
  if (im->data!=NULL) free(im->data);
  im->data=NULL;
  if (im->fn != NULL) free(im->fn);
  free(im);
}


Image *openImage(char *fn, int cache)
{
  char *ext;
  int len;
  Image *res=NULL;
  static Image *stored=NULL;
  static int count=0;

  TRACE;
  if(stored!=NULL)
    {
      if(strcmp(stored->fn, fn)==0)
	{
	  return stored;
	}
    }
  
  len=strlen(fn);
  if(len<5)
    {
      // not enough chars for a filename with a proper extension
      return NULL;
    }
  ext=&fn[len-4];
  if(*ext != '.')
    {
      ext--;
    }
  if(*ext != '.')
    {
      return NULL;
    }
  if((stricmp(ext, ".jpg")==0) || (stricmp(ext, ".jpeg")==0))
    {
      // printf("jpg detected\n");
      res=readjpg(fn);
    }
  else if(stricmp(ext, ".png")==0)
    {
      res=readpng(fn);
    }
  else if (stricmp(ext, ".ppm")==0)
    {
      res=readppm(fn);
    }
  
  if(cache && (res!=NULL))
   {
      if(stored!=NULL)releaseImage(stored);
      stored=res;
    }
  res->serial=count++;
  return res;
}

/* simple math function */
int power(int base, int n)
{
  int i, p; 
  TRACE;
  if (n == 0)
    return 1;
  p = 1;
  for (i = 1; i <= n; ++i)
    p = p * base;
  return p;
}

/* htoi is to hexadecimal what atoi is to decimal */
int htoi(char s[]) 
{
  int len, value = 1, digit = 0,  total = 0;
  int c, x, y, i = 0;
  char hexchars[] = "abcdef";
  TRACE;
  if (s[i] == '0')
    {
      i++;
      if (s[i] == 'x' || s[i] == 'X')
	{
          i++;
	}
    }
  len = strlen(s);
  for (x = i; x < len; x++)
    {
      c = tolower(s[x]);
      if (c >= '0' && c <= '9')
	{
          digit = c - '0';
	} 
      else if (c >= 'a' && c <= 'f')
	{
          for (y = 0; hexchars[y] != '\0'; y++)
	    {
	      if (c == hexchars[y])
		{
		  digit = y + 10;
		}
	    }
	} else {
	return 0;
      }
      value = power(16, len-x-1);
      total += value * digit;
    }
  return total;
}


unsigned char *fast_but_cheap_resize(Image *im, float fx, float fy, float fzoom)
{
  unsigned char *res;
  float fw, frx, fry, frw, frh, fdx, fdy;
  int bufsize, outoffset, inoffset, ix, iy, i, j;
  TRACE;
  bufsize=outputwidth * outputheight * 3;
  if((res=malloc(bufsize))==NULL)
    {
      fprintf(stderr, "Out of memory trying to allocate for a new ppm\n");
      return NULL;
    }
/*   for(i=0; i<bufsize ; i+=3) */
/*     { */
/*       memcpy(&res[i], &defaultcolor, 3); */
/*     } */
  // printf("after memcpy\n");
  fw=im->width;
  frw=outputwidth;  
  frh=outputheight;
  // printf("About to big loop with im=%p, im->data=%p, res=%p, outputwidth=%d, outputheight=%d\n",im, im->data, res, outputwidth, outputheight);
  for(iy=0;iy<outputheight;iy++)
    {
      fry=iy;
      for(ix=0;ix<outputwidth;ix++)
	{
	  
	  outoffset=(iy * outputwidth + ix)*3;
	  frx=ix;
	  fdx=fx - (fw / (fzoom * 2.0)) + (frx * fw / ( frw * fzoom ));
	  fdy=fy - (fw * frh / (fzoom * 2.0 *frw)) + (fry  * fw / ( frw * fzoom ));
	  j=fdx;
	  i=fdy;	  
          if((i>=0) && (j>=0) && (j<im->width) && (i<im->height))
	    {
	      inoffset=(i*im->width+j)*3;
	      res[outoffset]=im->data[inoffset];
	      res[outoffset+1]=im->data[inoffset+1];
	      res[outoffset+2]=im->data[inoffset+2];	      
	    }
	  else
	    {
	      memcpy(&res[outoffset], &defaultcolor, 3);
	    }
	}
    }
  // printf("After the big loop\n");
  return res;    
}
unsigned char *resize_no_blur(Image *im, float fx, float fy, float fzoom)
{
  unsigned char *res;
  float rpw, fw, frx, fry, frw, frh, fdx, fdy, fstartx, fstarty, fendx, fendy, fred, icolor; 
  float fweight, ftotalweight, fdelta, fgreen, fblue, fi, fj, fcolor;
  int bufsize, outoffset, inoffset, ix, iy, i, j, istartx, istarty, iendx, iendy, *pprev=NULL;
  TRACE;
  bufsize=outputwidth * outputheight * 3;
  if((res=malloc(bufsize))==NULL)
    {
      fprintf(stderr, "Out of memory trying to allocate for a new ppm\n");
      return NULL;
    }
  for(i=0; i<bufsize ; i+=3)
    {
      memcpy(&res[i], &defaultcolor, 3);
    }
  // printf("after memcpy\n");
  fw=im->width;
  frw=outputwidth;  
  frh=outputheight;
  rpw=fw/(frw*fzoom*sharpness);
  if(rpw<1.33333)rpw=1.33333;
  // printf("About to big loop with im=%p, im->data=%p, res=%p, outputwidth=%d, outputheight=%d\n",im, im->data, res, outputwidth, outputheight);
  check_prev_buffer(outputwidth, outputheight);
  if(motionblur)
    {
      pprev=prev;
    }
  for(iy=0;iy<outputheight;iy++)
    {
      fry=iy;
      for(ix=0;ix<outputwidth;ix++)
	{
	  
	  outoffset=(iy * outputwidth + ix)*3;
	  frx=ix;
	  fdx=fx - (fw / (fzoom * 2.0)) + (frx * fw / ( frw * fzoom ));
	  fdy=fy - (fw * frh / (fzoom * 2.0 *frw)) + (fry  * fw / ( frw * fzoom ));
	  if(motionblur) 
	    {
	      *pprev=fdx;
	      pprev++;
	      *pprev=fdy;
	      pprev++;
	    }
	  fstartx = fdx - rpw;
	  fendx = fdx + rpw;
	  fstarty = fdy - rpw;
	  fendy = fdy + rpw;
	  istartx=fstartx;
	  istarty=fstarty;
	  iendx=fendx;
	  iendy=fendy;
	  fred=0.0;
	  fgreen=0.0;
	  fblue=0.0;
	  ftotalweight=0.0;	  
	  for(i=istarty; i<= iendy; i++)
	    {
	      for(j=istartx; j<= iendx; j++)
		{
		  if((i>=0) && (j>=0) && (j<im->width) && (i<im->height))
		    {		      
		      inoffset=(i * im->width + j ) * 3;
		      fi=i;
		      fj=j;
		      fdelta=sqrtf(((fj - fdx) * (fj - fdx)) + ((fi-fdy) * (fi-fdy)));
		      if((rpw - fdelta)<=0.0)
			{
			}
		      else
			{
			  if((rpw - fdelta)>0.001)fweight=rpw-fdelta;
			  else fweight=0.001;
			  ftotalweight=ftotalweight+fweight;
			  fcolor=im->data[inoffset];
			  fred=fred + (fcolor * fweight);
			  fcolor=im->data[inoffset+1];
			  fgreen=fgreen + (fcolor * fweight);
			  fcolor=im->data[inoffset+2];
			  fblue=fblue + (fcolor * fweight);	  
			}
		    }
		}
	    }
	  if(ftotalweight <= 0.0)
	    {
/* 	      printf("totalweight under 0???\n"); */
	    }
	  else
	    {
	      fred = fred / ftotalweight;
	      fgreen = fgreen / ftotalweight;
	      fblue = fblue / ftotalweight;
	      icolor=fred;
	      res[outoffset]=icolor;
	      icolor=fgreen;
	      res[outoffset+1]=icolor;
	      icolor=fblue;
	      res[outoffset+2]=icolor;
	    }
	}
    }
  // printf("After the big loop\n");
  return res;    
}


static void motion_blur(Image *im, float *fred, float *fgreen, float *fblue, float *ftotalweight, int x1, int y1, int x2, int y2)
{
  int inoffset, midx, midy;
  float r, g, b;
  if(x2==LOST_IN_THE_WOOD)return;
  x1=x1;
  y1=y1;
  x2=x2;
  y2=y2;
  midx=(x1 + x2) / 2;
  midy=(y1 + y2) / 2;
  if((midx==x1)&&(midy==y1))return;
  if((midx==x2)&&(midy==y2))return;
  motion_blur(im, fred, fgreen, fblue, ftotalweight, x1, y1, midx, midy);
  motion_blur(im, fred, fgreen, fblue, ftotalweight, x2, y2, midx, midy);
  if((midx>=0) && (midy>=0) && (midx<im->width) && (midy<im->height))
    {
      inoffset=(midy * im->width + midx) * 3;
      r=im->data[inoffset];
      g=im->data[inoffset+1];
      b=im->data[inoffset+2];      
    }
  else
    {
     	  r=(defaultcolor & 0xff0000)/0x10000;
	  g=defaultcolor & 0x00ff00;
	  b=(defaultcolor & 0x0000ff)*0x10000;

    }
  *fred= *fred + r;
  *fgreen= *fgreen + g;
  *fblue= *fblue + b;
  *ftotalweight= *ftotalweight + 1.0;
}


unsigned char *resize(Image *im, float fx, float fy, float fzoom)
{
  unsigned char *res;
  float rpw, fw, frx, fry, frw, frh, fdx, fdy, fstartx, fstarty, fendx, fendy, fred, icolor; 
  float fweight, ftotalweight, fdelta, fgreen, fblue, fi, fj, fcolor;
  int bufsize, outoffset, inoffset, ix, iy, i, j, istartx, istarty, iendx, iendy, *pprev=NULL;
  static int last_serial= -1;
  static float last_fx, last_fy, last_fzoom;
  TRACE;
  if(last_serial!=im->serial)
    {
      last_fx=fx;
      last_fy=fy;
      last_fzoom=fzoom;
      last_serial=im->serial;
      return resize_no_blur(im, fx, fy, fzoom);
    }
  if((last_fx==fx)&&(last_fy==fy)&&(last_fzoom==fzoom))
    {
      return resize_no_blur(im, fx, fy, fzoom);
    }
  bufsize=outputwidth * outputheight * 3;
  if((res=malloc(bufsize))==NULL)
    {
      fprintf(stderr, "Out of memory trying to allocate for a new ppm\n");
      return NULL;
    }
  for(i=0; i<bufsize ; i+=3)
    {
      memcpy(&res[i], &defaultcolor, 3);
    }
  // printf("after memcpy\n");
  fw=im->width;
  frw=outputwidth;  
  frh=outputheight;
  rpw=fw/(frw*fzoom*sharpness);
  if(rpw<1.33333)rpw=1.33333;
  // printf("About to big loop with im=%p, im->data=%p, res=%p, outputwidth=%d, outputheight=%d\n",im, im->data, res, outputwidth, outputheight);
  check_prev_buffer(outputwidth, outputheight);
  if(motionblur)
    {
      pprev=prev;
    }
  for(iy=0;iy<outputheight;iy++)
    {
      fry=iy;
      for(ix=0;ix<outputwidth;ix++)
	{
	  
	  outoffset=(iy * outputwidth + ix)*3;
	  frx=ix;
	  fdx=fx - (fw / (fzoom * 2.0)) + (frx * fw / ( frw * fzoom ));
	  fdy=fy - (fw * frh / (fzoom * 2.0 *frw)) + (fry  * fw / ( frw * fzoom ));
	  fstartx = fdx - rpw;
	  fendx = fdx + rpw;
	  fstarty = fdy - rpw;
	  fendy = fdy + rpw;
	  istartx=fstartx;
	  istarty=fstarty;
	  iendx=fendx;
	  iendy=fendy;
	  fred=0.0;
	  fgreen=0.0;
	  fblue=0.0;
	  ftotalweight=0.0;	  
	  
	  for(i=istarty; i<= iendy; i++)
	    {
	      for(j=istartx; j<= iendx; j++)
		{
		  if((i>=0) && (j>=0) && (j<im->width) && (i<im->height))
		    {
		      inoffset=(i * im->width + j ) * 3;
		      fi=i;
		      fj=j;
		      fdelta=sqrtf(((fj - fdx) * (fj - fdx)) + ((fi-fdy) * (fi-fdy)));
		      if((rpw - fdelta)<=0.0)
			{
			}
		      else
			{
			  if((rpw - fdelta)>0.001)fweight=rpw-fdelta;
			  else fweight=0.001;
			  ftotalweight=ftotalweight+fweight;
			  fcolor=im->data[inoffset];
			  fred=fred + (fcolor * fweight);
			  fcolor=im->data[inoffset+1];
			  fgreen=fgreen + (fcolor * fweight);
			  fcolor=im->data[inoffset+2];
			  fblue=fblue + (fcolor * fweight);	  
			}
		    }
		}
	    }
	  if(ftotalweight <= 0.0)
	    {
/* 	      printf("totalweight under 0???\n"); */
	    }
	  else
	    {
	      fred = fred / ftotalweight;
	      fgreen = fgreen / ftotalweight;
	      fblue = fblue / ftotalweight;
	      if(motionblur)
		{
	          ftotalweight=1.0;
	          istartx=fdx;
	          istarty=fdy;
	          motion_blur(im, &fred, &fgreen, &fblue, &ftotalweight, istartx, istarty, pprev[0], pprev[1]);
 	          fred = fred / ftotalweight;
	          fgreen = fgreen / ftotalweight;
	          fblue = fblue / ftotalweight;
	          pprev[0]=istartx;
	          pprev[1]=istarty;
		}
	      
	      

	      icolor=fred;
	      res[outoffset]=icolor;
	      icolor=fgreen;
	      res[outoffset+1]=icolor;
	      icolor=fblue;
	      res[outoffset+2]=icolor;
	    }
	  if(motionblur)
	    {
	      pprev+=2;
	    }
	}
    }
  // printf("After the big loop\n");
  return res;    
}




void frame(Movement *p, float x, float y, float zoom)
{
  static int i,bytecount,ix,iy,jx,jy,lx,ly;
  int resizex, resizey, cropx, cropy;
  char fn[2048];
  unsigned char *ppm, *crossdata=NULL;
  Image *crossedjpg, out;
  float ratioA, ratioB, valueA, valueB, mixedvalue;
  float fx, fy;
  float px, py,qx,qy; // precropx, precropy
  TRACE;
  //printf("frame(%p, %9.3f, %9.3f, %9.3f);\n", p, x, y, zoom);
  if(lastsx== -1)
    {
      memset(lastfilename,0,2048);
    }
  fx=p->width;
  fx= fx * zoom;
  resizex=fx;
  if(resizex < outputwidth)resizex=outputwidth;
  fy=p->height;
  fy = fy * zoom;
  resizey = fy;
  if(resizey < outputheight)resizey=outputheight;
  fx = x * zoom;
  cropx = fx;
  cropx -= outputwidth / 2;
  if (cropx < 0) cropx = 0;
  fy = y * zoom;
  cropy = fy;
  cropy -= outputheight / 2;  
  if(cropy < 0) cropy = 0;
  
  sprintf(fn, "%s/%05d.", tmpdir, framecount);
  im=openImage(p->filename, true);
  if(im==NULL)
    {
      fprintf(stderr,"Could not open image file %s\n", p->filename);
      return;
    }
  fprintf(stdout, "Creating frame #%05d at x=%8.2f y=%8.2f and zoom %8.2f\r", framecount, x, y, zoom);
  fflush(stdout);
  if(fastrender)
    {
      ppm=fast_but_cheap_resize(im, x, y, zoom);
    }
  else
    {
      ppm=resize(im, x, y, zoom);
    }
  if(outputformat==JPG_FORMAT)
    {
      strcat(fn, "jpg");
    }
  else
    {
      strcat(fn, "ppm");
    }
  if(p->crossfade)
    {
      if((crossedjpg=openImage(fn, false))!=NULL)
	{
	  crossdata=crossedjpg->data;
	  crossedjpg->data=NULL;
	  releaseImage(crossedjpg);
	  crossedjpg=NULL;
	}
      if(crossdata!=NULL)
	{
	  bytecount=outputwidth * outputheight * 3;
	  ratioA=p->crossfade;
	  ratioB=p->crossframes;
	  ratioA=ratioA/ratioB;
	  ratioB=1.0 - ratioA;	  
	  for(i=0;i<bytecount;i++)
	    {
	      valueA=crossdata[i];
	      valueB=ppm[i];
	      mixedvalue=(valueA*ratioA)+(valueB*ratioB);
	      ppm[i]=mixedvalue;
	    }
	  free(crossdata);
	  p->crossfade--;
	}
      else
	{
	  p->crossfade=0;
	  p->crossframes=0;
	}
    }
  out.width=outputwidth;
  out.height=outputheight;
  out.fn=fn;
  out.data=ppm;
  if(showoutput)
    {
      show_image(&out);
    }
  if(outputformat==JPG_FORMAT)
    {
      writejpg(&out, fn);
    }
  else
    {
      writeppm(&out, fn);
    }
  free(ppm);
  if((strcmp(p->filename,lastfilename)==0)&&(lastsx==resizex)&&(lastsy==resizey)&&(lastx==cropx)&&
     (lasty==cropy))
    {
    }
  else if((resizex < 740)&&(resizey < 490))
    {
      strcpy(lastfilename,p->filename);
      lastsx=resizex;
      lastsy=resizey;
      lastx=cropx;
      lasty=cropy;
    }
  else
    {
      strcpy(lastfilename,p->filename);
      lastsx=resizex;
      lastsy=resizey;
      lastx=cropx;
      lasty=cropy;
      px = cropx;
      px = px / zoom;
      py = cropy;
      py = py / zoom;
      qx = (float)(outputwidth + (zoom  * 2.0) + 1 ) / zoom;
      qy = (float)(outputheight+ (zoom  * 2.0) + 1 ) / zoom;
      ix=px;
      iy=py;
      jx=qx;
      jy=qy;
      if((ix + jx)> p->width)jx=p->width - ix;
      if((iy + jy)> p->height)jy=p->height - iy;
      px = zoom * (float)jx;
      py = zoom * (float)jy;
      qx=ix;
      qx = qx * zoom;
      qy=iy;
      qy = qy * zoom;
      lx=qx;
      ly=qy;
      cropx -= lx;
      cropy -= ly;
      
    }
  framecount++;
}

void action(Movement *p)
{
  double total;
  double x,y,zoom, f_startx, f_starty, f_endx, f_endy;
  int i,icount;
  total = p->duration * framerate;
  icount=total;
  //  printf("action: from (%d, %d) to (%d, %d) in %d frames.\n", p->startx, p->starty, p->endx, p->endy,  icount);
  if(icount < 1) {
    icount=1;
    total=icount;
  }
  f_startx=p->startx;
  f_starty=p->starty;
  f_endx=p->endx;
  f_endy=p->endy;
  x=f_startx;
  y=f_starty;
  zoom=p->zoomstart;
  for(i=0;i<icount;i++)
    {
      x=   x + smoothed_step(f_startx, f_endx,   i, (double)icount, pansmoothratio);
      y=   y + smoothed_step(f_starty, f_endy,   i, (double)icount, pansmoothratio);
      zoom=zoom + smoothed_step((double)p->zoomstart, (double)p->zoomend, i, (double)icount, zoomsmoothratio);
      frame(p,x,y,zoom);
    }
}

void cleanstring(char *str)
{
  char *c;
  TRACE;
  if (str==NULL)return;
  while((c=strstr(str,"\r"))!=NULL)c[0]=0;
  while((c=strstr(str,"\n"))!=NULL)c[0]=0;
}

int isawhitechar (char c)
{
  TRACE;
  if((c==' ')||(c=='\t')||(c=='\n')||(c=='\r'))return -1; // true
  return 0;                                               // false
}

unsigned int hash(char *str)
{
  int res=0;
  int c;
  TRACE;
  if(str==NULL) return 0;
  while(str[0]!=0)
    {
      c=tolower(str[0]);
      if((c>='a')&&(c<='z'))
	{
	  res *= 26;
	  res += c - 'a';
	}
      else return res;
      str++;
    };  
  return res;
}



int translate(char *s)
{
  TRACE;
  switch (hash(s))
    {
    case 25832109U: // center
      return CENTER;
    case 196189U: //left
      return LEFT;
    case 7913457U: // right
      return RIGHT;
    case 13223U: // top
      return TOP;
    case 18626204U: // bottom
      return BOTTOM;
    case 4211614177U: // fitwidth
      return FITWIDTH;
    case 1947816769U: // fitheight
      return FITHEIGHT;
    case 3607U: // fit
      return FIT;
    case 2496525649U: // topright
    case 1647979983U: // righttop
      return TOPRIGHT;
    case 3448231087U: // lefttop
    case 1747822541U: // topleft
      return TOPLEFT;
    case 2456196465U: // bottomright
    case 3717923036U: // rightbottom
      return BOTTOMRIGHT;
    case 3907257308U: // leftbottom
    case 3398181917U: // bottomleft
      return BOTTOMLEFT;

    default:
      return 0;
    }
  return 0;
}

void normalize(Movement *p)
{
  float fitheightzoom, fitzoom;
  float oresx, oresy,iresx, iresy;
  TRACE;
  oresx=outputwidth;
  oresy=outputheight;
  iresx=p->width;
  iresy=p->height;
  fitheightzoom=oresy * iresx / ( oresx * iresy);
  if(fitheightzoom<1.0)fitzoom=fitheightzoom;
  else fitzoom=1.0;
  if(p->zecode == FITHEIGHT)p->zoomend=fitheightzoom;
  if(p->zecode == FITWIDTH)p->zoomend=1.0;
  if(p->zecode == FIT)p->zoomend=fitzoom;
  if(p->zscode == FITHEIGHT)p->zoomstart=fitheightzoom;
  if(p->zscode == FITWIDTH)p->zoomstart=1.0;
  if(p->zscode == FIT)p->zoomstart=fitzoom;
  
  if(p->startx==CENTER)
    {
      p->startx=p->width / 2;
      p->starty=p->height/2;
    }
  else if(p->startx==LEFT)
    {
      p->startx=iresx / (p->zoomstart * 2);
      p->starty=p->height/2;
    }
  else if(p->startx==RIGHT)
    {
      p->startx=iresx - (iresx / (p->zoomstart * 2));
      p->starty=p->height/2;
    }
  else if(p->startx==BOTTOM)
    {
      p->startx=p->width/2;
      p->starty=iresy - (iresx * oresy / (p->zoomstart * 2 * oresx));
    }
  else if(p->startx==TOP)
    {
      p->startx=p->width/2;
      p->starty=iresx * oresy / (p->zoomstart * 2 * oresx);
    }
  else if(p->startx==TOPRIGHT)
    {
      p->startx=iresx - (iresx / (p->zoomstart * 2));
      p->starty=iresx * oresy / (p->zoomstart * 2 * oresx);
    }
  else if(p->startx==TOPLEFT)
    {
      p->startx=iresx / (p->zoomstart * 2);
      p->starty=iresx * oresy / (p->zoomstart * 2 * oresx);
    }
  else if(p->startx==BOTTOMRIGHT)
    {
      p->startx=iresx - (iresx / (p->zoomstart * 2));
      p->starty=iresy - (iresx * oresy / (p->zoomstart * 2 * oresx));
    }
  else if(p->startx==BOTTOMLEFT)
    {
      p->startx=iresx / (p->zoomstart * 2);
      p->starty=iresy - (iresx * oresy / (p->zoomstart * 2 * oresx));
    }

  if(p->endx==CENTER)
    {
      p->endx=p->width/2;
      p->endy=p->height/2;
    }
  else if(p->endx==LEFT)
    {
      p->endx=iresx / (p->zoomend * 2);
      p->endy=p->height/2;
    }
  else if(p->endx==RIGHT)
    {
      p->endx=iresx - (iresx / (p->zoomend * 2));
      p->endy=p->height/2;
    }
  else if(p->endx==BOTTOM)
    {
      p->endx=p->width/2;
      p->endy=iresy - (iresx * oresy / (p->zoomend * 2 * oresx));
    }
  else if(p->endx==TOP)
    {
      p->endx=p->width/2;
      p->endy=iresx * oresy / (p->zoomend * 2 * oresx);
    }
  else if(p->endx==TOPRIGHT)
    {
      p->endx=iresx - (iresx / (p->zoomstart * 2));
      p->endy=iresx * oresy / (p->zoomstart * 2 * oresx);
    }
  else if(p->endx==TOPLEFT)
    {
      p->endx=iresx / (p->zoomstart * 2);
      p->endy=iresx * oresy / (p->zoomstart * 2 * oresx);
    }
  else if(p->endx==BOTTOMRIGHT)
    {
      p->endx=iresx - (iresx / (p->zoomstart * 2));
      p->endy=iresy - (iresx * oresy / (p->zoomstart * 2 * oresx));
    }
  else if(p->endx==BOTTOMLEFT)
    {
      p->endx=iresx / (p->zoomstart * 2);
      p->endy=iresy - (iresx * oresy / (p->zoomstart * 2 * oresx));
    }
 


}

int splitted2struct(Movement *p,char **strs)
{
  static Movement lastp;
  int val,r,g,b,w,h,i=0;
  unsigned long uval;
  float fval;
  int continued=0;
  int actiondata=0;
  char *c;
  TRACE;
  if(p==NULL)return -1;
  if(strs==NULL) return -1;
  p->filename=NULL;
  p->width=0;
  p->height=0;
  p->startx=CENTER;
  p->starty=CENTER;
  p->endx=CENTER;
  p->endy=CENTER;
  p->zscode=0;
  p->zecode=0;
  p->zoomstart=(float)1.0;
  p->zoomend=(float)1.0;
  p->startsmooth=(float)0.0;
  p->endsmooth=(float)0.0;
  while(strs[i]!=NULL)
    {
      switch (hash(strs[i]))
	{
	case 322430U: // size
	case 204133726U: // resize
	case 2353790494U: // geometry
	case 3513677489U: // resolution
	  if(strs[++i]==NULL)continue;
	  w=atoi(strs[i]);
	  c=strstr(strs[i],"x");
	  if(c==NULL)
	    c=strstr(strs[i],":");
	  if(c==NULL)continue;
	  c++;
	  h=atoi(c);
	  if((w<1)||(h<1))continue;
	  outputwidth=w;
	  outputheight=h;
	  break;

	case 10196609U: // width
	case 176535873U: // owidth
	case 3465197793U: // outputwidth
	  if(strs[++i]==NULL)continue;
	  w=atoi(strs[i]);
	  if(w>0)outputwidth=w;
	  break;

	case 85142401U: // height
	case 114995969U: // oheight
	case 4015827265U: // outputheight
	  if(strs[++i]==NULL)continue;
	  h=atoi(strs[i]);
	  if(h>0)outputheight=h;
	  break;

	case 1387944462U: // sharpness
	  if(strs[++i]==NULL)continue;
	  fval=atof(strs[i]);
	  if(fval>0.0)sharpness=fval;
	  break;
	  
	case 3694453710U: // pansmoothness
	  if(strs[++i]==NULL)continue;
	  fval=atof(strs[i]);
	  if(fval < 0.0) fval=0.0;
	  if(fval > 0.5) fval=0.5;
	  pansmoothratio=fval;	  
	  break;

	case 2552266702U: // zoomsmoothness
	  if(strs[++i]==NULL)continue;
	  fval=atof(strs[i]);
	  if(fval < 0.0) fval=0.0;
	  if(fval > 0.5) fval=0.5;
	  zoomsmoothratio=fval;
	  break;
	  
	case 885511902U: // quality
	case 2001802206U: // jpegquality
	  if(strs[++i]==NULL)continue;
	  val=atoi(strs[i]);
	  if(val>0)jpegquality=val;
	  break;
	  
	case 3788U: // fps
	case 3999760474U: // framerate
	  if(strs[++i]==NULL)continue;
	  fval=atof(strs[i]);
	  if(fval>0.0)framerate=fval;
	  break;
	  
	case 175823407U: // output
	case 350562U: // type
	case 1124381522U: // outputtype
	  if(strs[++i]==NULL)continue;
	  uval=hash(strs[i]);
	  if((uval==168434U)||(uval==6480U)) /* 168434U:jpeg  6480U: jpg */ 
	    {
	      outputformat=JPG_FORMAT;
	    }
	  else if(uval==10542U)              /*  10542U:ppm	         */
	    {
	      outputformat=PPM_FORMAT;
	    }
	  break;

	case 559228793U: // defaultcolor
	case 381371865U: // bgcolor
	case 2544635897U: // backgroundcolor
	  if(strs[++i]==NULL)continue;
	  defaultcolor=htoi(strs[i]);
	  // we now need to invert the byte order if we want to memcopy 3 bytes at a time...
	  r=(defaultcolor & 0xff0000)/0x10000;
	  g=defaultcolor & 0x00ff00;
	  b=(defaultcolor & 0x0000ff)*0x10000;
	  defaultcolor=r|g|b;
	  break;

	case 5726U:        // img
	  if(strs[++i]==NULL)continue;
	  p->filename=strdup(strs[i]);
	  if((im=openImage(strs[i], true))==NULL)
	    {
	      fprintf(stderr, "Could not open image %s\n", strs[i]);
	      continue;
	    }
	  p->width=im->width;
	  p->height=im->height;
	  actiondata=1;
	  break;
	case 396152826U: // crossfade
	  if(strs[++i]==NULL)continue;
	  p->duration=atof(strs[i]);
	  p->crossfade=(int) (float) (atof(strs[i]) * framerate) - 2.0;
	  if(framecount<p->crossfade)
	    {
	      p->crossfade=framecount;
	    }
	  framecount -= p->crossfade;
	  p->crossframes=p->crossfade;
	  break;
	case 3734267333U:  // startpoint
	  if(strs[++i]==NULL)continue;
	  p->startx=atoi(strs[i]);
	  if((c=strstr(strs[i],","))==NULL)
	    {
	      p->endx=p->endy=p->startx=p->starty = translate(strs[i]);
	      i++;
	      continue;
	    }
	  c++;
	  p->starty=atoi(c);
	  p->endx=p->startx;
	  p->endy=p->starty;
	  actiondata=1;
	  break;
	case 1826158021U:  // endpoint
	  if(strs[++i]==NULL)continue;
	  p->endx=atoi(strs[i]);
	  if((c=strstr(strs[i],","))==NULL)
	    {
	      p->endx=p->endy = translate(strs[i]);
	      i++;
	      continue;
	    }
	  c++;
	  p->endy=atoi(c);
	  actiondata=1;
	  break;
	case 449240U:      // zoom
	  if(strs[++i]==NULL)continue;
	  if((c=strstr(strs[i],","))!=NULL)
	    {
	      c[0]=0;
	      c++;
	      p->zoomstart=atof(strs[i]);
	      if(p->zoomstart==(float)0.0)p->zscode=translate(strs[i]);
	      p->zoomend=atof(c);
	      if(p->zoomend==(float)0.0)p->zecode=translate(c);
	    }
	  else
	    {
	      if(continued)
		{
		  p->zoomend=atof(strs[i]);
		  if(p->zoomend==(float)0.0)
		    {
		      p->zecode=translate(strs[i]);
		    }
		}
	      else
		{
		  p->zoomstart=p->zoomend=atof(strs[i]);
		  if(p->zoomstart==(float)0.0)
		    {
		      p->zscode=p->zecode=translate(strs[i]);
		    }
		} 
	    }
	  actiondata=1;
	  break;
	case 411298097U:   // duration
	  if(strs[++i]==NULL)continue;
	  p->duration=atof(strs[i]);
	  actiondata=1;
	  break;
	case 2636506461U:  // startsmooth
	  if(strs[++i]==NULL)continue;
	  p->startsmooth=atof(strs[i]);
	  actiondata=1;
	  break;
        
	case 270304605U:   // endsmooth
	  if(strs[++i]==NULL)continue;
	  p->endsmooth=atof(strs[i]);
	  actiondata=1;
	  break;
	case 3371862384U:   // continue
	  continued=1;
	  p->endx=p->startx=lastp.endx;
	  p->endy=p->starty=lastp.endy;
	  p->filename=strdup(lastp.filename);
	  p->width=lastp.width;
	  p->height=lastp.height;
	  p->zoomend=p->zoomstart=lastp.zoomend;
	  actiondata=1;
	  break;
	default:
	  fprintf(stderr,"Unknown commande (%s)\n",strs[i]);
	}
      i++;
    }
  if(actiondata)
    {
      normalize(p);
      lastp= *p;  
      return true;
    }
  else
    {
      return false;
    }
}

// split, splits a phrase into an array of words.
static char **split(char *ligne)
{
  int count=1;
  char *cursor;
  char *ourcopy;
  int escapechar=0;
  int escapeescape=0;
  char **ret;
  TRACE;
  while(isawhitechar(ligne[0]))ligne++;
  if(strlen(ligne)<1)return NULL;
  cursor=ligne;
  do
    {
      if(cursor[0]=='\"')
	{
	  if(!escapeescape)
	    {
	      escapechar= !escapechar;
	    }
	}
      if(!escapechar)
	{
	  if(isawhitechar(cursor[0]))
	    {
	      count++;
	      do { cursor++; } while(isawhitechar(cursor[0]));
	    }
	}
      else
	{
	  if(cursor[0]=='\\')escapeescape= -1;
	  else escapeescape=0;
	}
    }while((++cursor)[0]!=0);  
  if((ret=malloc(sizeof(char *) * (count + 1)))==NULL)
    {
      fprintf(stderr,"malloc failed\n");
      return NULL;
    }
  if((ourcopy=ret[0]=strdup(ligne))==NULL)
    {
      fprintf(stderr,"strdup failed\n");
      free(ret);
      return NULL;
    }
  count=1;
  escapechar=0;
  escapeescape=0;
  cursor=ourcopy;
  ret[0]=cursor;
  do
    {
      if(cursor[0]=='\"')
	{
	  if(!escapeescape)
	    {
	      escapechar= !escapechar;
	    }
	}
      if(!escapechar)
	{
	  if(isawhitechar(cursor[0]))
	    {
	      cursor[0]=0;
	      do { cursor++; } while(isawhitechar(cursor[0]));
	      cleanstring(cursor);
	      ret[count]=cursor;
	      count++;
	    }
	}
      else
	{
	  if(cursor[0]=='\\')escapeescape= -1;
	  else escapeescape=0;
	}
    }while((++cursor)[0]!=0);  
  ret[count]=NULL;
  return ret;
}




static char scriptline[2048];
int main (int argc, char *argv[])
{
  char **splitted;
  int param;
  char *fn=NULL;
  TRACE;
  if((tmpdir=getenv("TMPDIR"))==NULL)tmpdir="/tmp";
  param=1;
  while(param<argc)
    {
      
      if (argv[param][0]=='-')
	{
	  switch (hash(&argv[param][1]))
	    {
	    case 2247880249U: // version
	      printf("stills2dv version Alpha 0.601, http://www.deniscarl.com/stills2dv/\n");
	      return 0;
	    case 231495749U: // tmpdir	  
	      tmpdir=strdup(argv[++param]);
	      break;
	    case 4039168431U: // showoutput
	      printf("Setting showouput to true!\n");
	      showoutput= true;
	      break;
  	    case 3647253645U: // fastrender
	      fastrender=true;
	      break;
	    case 3286000605U: // nomotionblur
	      motionblur=false;
	      break;
	    default:	      
	      fprintf(stderr, "Unknown command: %s\nUsage: %s\n", argv[param], usage);
	      return 0;	 
	    }
	}
      else
	{
	  if(fn!=NULL)
	    {
	      fprintf(stderr, "Usage: %s\n", usage);
	      return 0;
	    }
	  fn=strdup(argv[param]);
	}
      param++;
    }
  if((scriptfile=fopen(fn,"rt"))==NULL)
    {
      return -ENOENT;
    }
  while (fgets(scriptline,2048,scriptfile)!=NULL)
    {
      fprintf(stdout, "\n%s",scriptline);
      if((splitted=split(scriptline))!=NULL)
	{
	  if(splitted2struct(&ms,splitted))
	    {
	      /* 	      printf("%s (%03dx%03d) from %03dx%03d at zoom %6.3f to %03dx%03d at zoom %6.3f\n", */
	      /* 		     ms.filename, ms.width, ms.height, */
	      /* 		     ms.startx, ms.starty, ms.zoomstart, */
	      /* 		     ms.endx, ms.endy, ms.zoomend); */

	      action (&ms);
	    }
	}
    }
  printf("\n");
  return 0;
}

