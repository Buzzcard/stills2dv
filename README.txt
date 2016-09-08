README for compiling and installing stills2dv

Requirements: 
	      libc
	      gcc
	      make
	      libjpeg-dev
	      libpng-dev


If you do not know howto find/install those applications and libraries you 
should try to find your distribution's package manager like this one for 
Ubuntu 8.04:
   System->Administration->Synaptic Package Manager

Packages may have version numbers that you need to specify, I cannot tell you 
exactly which to use as this information may be outdated the second I write it 
but I will give you the version numbers of those installed in my distribution 
just as a guide if something goes wrong (try the latest version first, if an 
improbable problem arise it may be possible for you to find the same version 
as mine): 
   libc6, gcc-4.2, libjpeg62-dev and libpng12-dev


The Makefile has been set to install the application in the /usr/bin directory 
but it can be changed by changing the PREFIX value (you'll have to edit the 
Makefile in that case) 


In the Makefile I used the optimizing option -O4 which will only works if you 
have a Pentium4 or better, change it to -03 if your computer does not qualify 
as the application will simply not run on your CPU.



Compiling: 
    make

Installing:
    sudo make install




For information on howto use the program you should go read the file name 
index.html in the Documentation folder or at this web address: 
http://deniscarl.com/stills2dv/index.html

