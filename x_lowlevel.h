typedef struct
{
     Display     *display;
     Window      window;
     GC          gc;
     int         screen;
     Visual      *visual;
     Pixmap      windowPixmap;
     XImage      *pixmapImage;
}ImageWindow;


ImageWindow* CreateDefaultImageWindow(Display *dis, int x, int y, unsigned int width, unsigned int height);

ImageWindow* CreateImageWindow(Display *dis, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width,  
unsigned long border_color, unsigned long background_color, Visual *vis, int screen, int depth);

void SendExposeEvent(ImageWindow *iw);
void ExposeImageWindow(ImageWindow *iw);

void PutPixel(ImageWindow *iw, int x, int y, unsigned long color);
unsigned long GetPixel(ImageWindow *iw, int x, int y);
void ClearImageWindow(ImageWindow *iw, unsigned long color);

XImage* CreateImage(Display *dis, Visual *vis, int screen, int width,  
int height);
void FatalError(const char *str);

