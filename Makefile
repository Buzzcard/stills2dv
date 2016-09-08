PREFIX = /usr
CC = gcc
COPTIONS = -Wall -g -O4
CLIBS = -ljpeg -lpng -lm -I/usr/X11R6/include/ -L/usr/X11R6/lib/ -lXext -lX11

all: stills2dv

hash: hash.c stills2dv.h
	$(CC) -o hash $(COPTIONS) hash.c

stills2dv: s2d_main.o s2d_ppm.o s2d_jpg.o s2d_png.o s2d_smoothing.o x_lowlevel.o
	$(CC) -o stills2dv  $(COPTIONS) s2d_smoothing.o s2d_ppm.o s2d_jpg.o s2d_png.o s2d_main.o x_lowlevel.o $(CLIBS)
	echo You may try the command: make example

s2d_main.o: s2d_main.c stills2dv.h
	$(CC) $(COPTIONS) -c s2d_main.c

s2d_jpg.o: s2d_jpg.c stills2dv.h
	$(CC) $(COPTIONS) -c s2d_jpg.c

s2d_png.o: s2d_png.c stills2dv.h
	$(CC) $(COPTIONS) -c s2d_png.c

s2d_ppm.o: s2d_ppm.c stills2dv.h
	$(CC) $(COPTIONS) -c s2d_ppm.c

s2d_smoothing.o: s2d_smoothing.c stills2dv.h
	$(CC) $(COPTIONS) -c s2d_smoothing.c

x_lowlevel.o: x_lowlevel.c
	$(CC) $(COPTIONS) -c x_lowlevel.c

clean:
	rm -f *.o stills2dv hash Example/out.mpg tmp/* out.mpg tmp.mpg ffmpeg2pass*.log *~

clean_temp:
	rm -f *.o tmp/* tmp.mpg ffmpeg2pass*.log

example: stills2dv
	mkdir -p tmp
	rm -f tmp.mpg out.mpg
	./stills2dv -tmpdir tmp -showoutput exampleworkfile.s2d
	ffmpeg -r 30.00 -f image2 -i tmp/%05d.jpg -i example_data_files/09_1_2_3_4.mp3 -vcodec mpeg4 -vb 9000k -ab 128k -aspect 16:9 -pass 1 tmp.avi
	ffmpeg -r 30.00 -f image2 -i tmp/%05d.jpg -i example_data_files/09_1_2_3_4.mp3 -vcodec mpeg4 -vb 9000k -ab 128k -aspect 16:9 -pass 2 out.avi
	rm -f tmp/* tmp.avi
	mplayer out.avi -fs



install: stills2dv
	cp -f stills2dv $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/stills2dv
	chown root:root $(PREFIX)/bin/stills2dv

