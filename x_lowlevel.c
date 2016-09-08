

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "stills2dv.h"
#include "x_lowlevel.h"


/* Thank you Brian Stone */

void show_image(Image *im)
{
     static Display         *dis=NULL;
     static ImageWindow     *myImageWindow=NULL;
     unsigned long *pixel;
     unsigned char *pixpos;
     int             x, y = 0;
     XEvent          e;
     if(im==NULL)return;
     if (!dis)dis = XOpenDisplay(NULL);
     if (!dis) FatalError("Display could not be initialized.");
     if(!myImageWindow)myImageWindow = CreateDefaultImageWindow(dis, 1, 1, im->width, im->height);
     XSelectInput(dis, myImageWindow->window, ExposureMask);

         XNextEvent(dis, &e);
         switch  (e.type)
	    {
	        case Expose:
                 ExposeImageWindow(myImageWindow);
	        break;
/* 	        case KeyPress: */
/* 	            // Press "Q" to quit. */
/* 	            if(XLookupKeysym(&e.xkey, 0) == XK_q) */
/* 	                done = True; */
	        default:
	        break;
	    }
	
	 pixpos=im->data;
	 for(y=0; y<im->height;y++)
           for(x=0; x<im->width; x++)
	     {
	       pixel=(unsigned long *)pixpos;
	       pixpos+=3;
	       PutPixel(myImageWindow, x, y, *pixel);
	     }

         SendExposeEvent(myImageWindow);
     return;
}



ImageWindow* CreateDefaultImageWindow(Display *dis, int x, int y,  
unsigned int width, unsigned int height)
{
     ImageWindow   *iw;
     Window        parent;
     int           screen;
     Visual        *vis;
     int           depth;
     unsigned long black, white;

     parent = DefaultRootWindow(dis);
     screen = DefaultScreen(dis);
     vis    = DefaultVisual(dis, screen);
     depth  = DefaultDepth(dis, screen);
     black  = BlackPixel(dis,screen);
     white  = WhitePixel(dis,screen);

     // Create the window and offscreen image buffer.
     iw = CreateImageWindow(dis, parent, x, y, width, height, 0, white,  
black, vis, screen, depth);

     // Display the window.
     XMapWindow(iw->display, iw->window);

     // Return a pointer to the image window data structure.
     return(iw);
}


/* static int MyPalette[]= { 255,255,255, 255,0,0, 0,255,0, 0,0,255, 0,0,0, */
/* 		     -1,-1,-1}; /\* put this to specify the end of your palette *\/ */



ImageWindow* CreateImageWindow(Display *dis, Window parent, int x, int  
y, unsigned int width, unsigned int height, unsigned int border_width,  
unsigned long border_color, unsigned long background_color, Visual  
*vis, int screen, int depth)
{
     ImageWindow *iw;

     iw = (ImageWindow*)malloc(sizeof(ImageWindow));
     if (!iw) FatalError("Out of memory!");
     iw->window = XCreateSimpleWindow(dis, parent, x, y, width, height,  
border_width, border_color, background_color);
     XStoreName(dis,iw->window,"stills2dv preview");
     iw->gc = XCreateGC(dis, iw->window, 0, 0);
     if (!iw->gc) FatalError("Graphics Context Couldn't be Created.");
     iw->display = dis;
     iw->visual = vis;
     iw->screen = screen;
     iw->windowPixmap = XCreatePixmap(dis, iw->window, width, height,  
depth);
     if (!iw->windowPixmap) FatalError("Couldn't create window pixmap.");
     iw->pixmapImage = CreateImage(dis, vis, screen, width, height);

     return(iw);
}



void ClearImageWindow(ImageWindow *iw, unsigned long color)
{
     int             steps;
     unsigned long   fill;
     unsigned long   *long_ptr, *last_long_ptr;

     if ((iw->pixmapImage->bytes_per_line % 4) == 0)
     {
         // Create a 4-byte fill value.
         switch(iw->pixmapImage->bits_per_pixel)
         {
             case 8:
                 fill = (color<<24) | (color<<16) | (color<<8) | color;
             break;
             case 16:
                 fill = (color<<8) | color;
             break;
             default:
                 fill = color;
             break;
         }

         // Compute the first and last pointers to be visited.
         steps =  
(iw->pixmapImage->bytes_per_line*iw->pixmapImage->height)>>2;
         long_ptr = (unsigned long*)iw->pixmapImage->data;
         last_long_ptr = long_ptr + steps;

         // Fill the image buffer.
         while(long_ptr < last_long_ptr)
             *(long_ptr++) = fill;
     }
}

static inline unsigned short convert_to_16bits_x_color(unsigned long color)
{
  unsigned char *c= (unsigned char *) &color;
  return ((((unsigned short)c[0]/8)<<11)+(((unsigned short)c[1]/4)<<5)+((unsigned short)c[2]/8));
}
static inline unsigned long convert_to_24bits_x_color(unsigned long color)
{
  unsigned long ret;
  char *r=(char *) &ret;
  char *c= (char *) &color;
  r[0]=c[2];
  r[1]=c[1];
  r[2]=c[0];
  return ret;
}

void PutPixel(ImageWindow *iw, int x, int y, unsigned long color)
{
     char *pixel;
     // Return if (x,y) isn't inside the image bounds.
     if (!(x >= 0 && x < iw->pixmapImage->width && y >= 0 && y <  
iw->pixmapImage->height))
         return;

     // Compute the address of the first byte of pixel (x,y).
     pixel = iw->pixmapImage->data +  
       x*(iw->pixmapImage->bits_per_pixel>>3) +  
       y*iw->pixmapImage->bytes_per_line;

     // Write the data.
     switch(iw->pixmapImage->bits_per_pixel)
     {
         case 8:
	   // 8 bits? Haven't use it since the 80's. No support, it would look awful anyway
             *pixel = color;
         break;
         case 16:
	   *((unsigned short*)pixel) = convert_to_16bits_x_color(color);
         break;
         case 24:
         case 32:
	   *((unsigned long*)pixel) = convert_to_24bits_x_color(color);
         break;
     }
}



unsigned long GetPixel(ImageWindow *iw, int x, int y)
{
     char *pixel;

     // Return 0 if (x,y) isn't inside the image bounds.
     if (!(x >= 0 && x < iw->pixmapImage->width && y >= 0 && y <  
iw->pixmapImage->height))
         return(0);

     // Compute the address of the first byte of pixel (x,y).
     pixel = iw->pixmapImage->data +  
x*(iw->pixmapImage->bits_per_pixel>>3) +  
y*iw->pixmapImage->bytes_per_line;

     // Write the data.
     switch(iw->pixmapImage->bits_per_pixel)
     {
         case 8:
             return(*pixel);
         break;
         case 16:
             return(*((unsigned short*)pixel));
         break;
         case 32:
             return(*((unsigned long*)pixel));
         break;
     }

     // Should never get here.
     return(0);
}



XImage* CreateImage(Display *dis, Visual *vis, int screen, int width,  
int height)
{
     int depth;
     int byte_quantum;
     XImage *image;

     depth = DefaultDepth(dis, screen);
     switch(depth)
     {
       case 8:            byte_quantum = 1; break;
       case 15: case 16:  byte_quantum = 2; break;
       case 24: case 32:  byte_quantum = 4; break;
       default: FatalError("Couldn't determine screen depth.");
     }

     image = XCreateImage(dis, vis, depth, ZPixmap, 0, NULL, width,  
height, byte_quantum<<3, 0);
     if (!image) FatalError("Couldn't create image.");

     image->data = malloc(height * image->bytes_per_line);
     if (!image->data) FatalError("Out of memory.");

     return(image);
}



void ExposeImageWindow(ImageWindow *iw)
{
    	XPutImage (iw->display, iw->window, iw->gc, iw->pixmapImage, 0, 0,  
0, 0, iw->pixmapImage->width, iw->pixmapImage->height);
     XFlush(iw->display);
}



void SendExposeEvent(ImageWindow *iw)
{
   XExposeEvent Event;
   XWindowAttributes attrib;

   // Get the window's attributes.
   XGetWindowAttributes(iw->display, iw->window, &attrib);

   // Setup the exposure event.
   Event.type = Expose;
   Event.display = iw->display;
   Event.send_event = True;
   Event.window = iw->window;
   Event.x = 0;
   Event.y = 0;
   Event.width = attrib.width;
   Event.height = attrib.height;
   Event.count = 0;

   // Send the exposure event.
   XSendEvent(iw->display, iw->window, False, ExposureMask,  
(XEvent*)&Event);
   //XtDispatchEvent((XEvent*)&Event);
}


// print error.
void FatalError(const char *str)
{
   printf("%s\n", str);
   exit(0);
}
