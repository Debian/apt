The Make System
~~~ ~~~~ ~~~~~~
To compile this program you require GNU Make. In fact you probably need
GNU Make 3.76.1 or newer. The makefiles contained make use of many 
GNU Make specific features and will not run on other makes.

The make system has a number of interesting properties that are not found
in other systems such as automake or the GNU makefile standards. In
general some semblance of expectedness is kept so as not to be too
surprising. Basically the following will work as expected:

   ./configure
   make
 or
   cd build
   ../configure
   make

There are a number of other things that are possible that may make software
development and software packaging simpler. The first of these is the
environment.mak file. When configure is run it creates an environment.mak
file in the build directory. This contains -all- configurable parameters
for all of the make files in all of the subdirectories. Changing one
of these parameters will have an immediate effect. The use of makefile.in 
and configure substitutions across build makefiles is not used at all.

Furthermore, the make system runs with a current directory equal to the
source directory irregardless of the destination directory. This means
#include "" and #include <> work as expected and more importantly
running 'make' in the source directory will work as expected. The
environment variable or make parameter 'BUILD' sets the build directory.
It may be an absolute path or a path relative to the top level directory.
By default build-arch/ then build/ will be used with a fall back to ./ This 
means you can get all the advantages of a build directory without having to
cd into it to edit your source code!

The make system also performs dependency generation on the fly as the
compiler runs. This is extremely fast and accurate. There is however
one failure condition that occurs when a header file is erased. In
this case you should run make clean to purge the .o and .d files to
rebuild.

The final significant deviation from normal make practices is 
in how the build directory is managed. It is not nearly a mirror of
the source directory but is logically divided in the following manner
   bin/
     methods/
   doc/
     examples/
   include/
     apt-pkg/
   obj/
     apt-pkg/
     cmdline/
     [...]
Only .o and .d files are placed in the obj/ subdirectory. The final compiled
binaries are placed in bin, published headers for inter-component linking
are placed in include/ and documentation is generated into doc/. This means
all runnable programs are within the bin/ directory, a huge benefit for
debugging inter-program relationships. The .so files are also placed in
bin/ for simplicity.

By default make is put into silent mode. During operation there should be
no shell or compiler messages only status messages from the makefiles, 
if any pop up that indicates there may be a problem with your environment.
For debugging you can disable this by setting NOISY=1, ala
   make NOISY=1

Using the makefiles
~~~~~ ~~~ ~~~~~~~~~
The makefiles for the components are really simple. The complexity is hidden
within the buildlib/ directory. Each makefile defines a set of make variables
for the bit it is going to make then includes a makefile fragment from
the buildlib/. This fragment generates the necessary rules based on the
originally defined variables. This process can be repeated as many times as
necessary for as many programs or libraries as are in the directory.

Many of the make fragments have some useful properties involving sub
directories and other interesting features. They are more completely 
described in the fragment code in buildlib. Some tips on writing fragments
are included in buildlib/defaults.mak

The fragments are NEVER processed by configure, so if you make changes to 
them they will have an immediate effect.

Autoconf
~~~~~~~~
Straight out of CVS you have to initialize autoconf. This requires 
automake (I really don't know why) and autoconf and requires doing 
  aclocal -I buildlib
  autoconf
[Alternatively you can run make startup in the top level build dir]

Autoconf is configured to do some basic system probes for optional and 
required functionality and generate an environment.mak and include/config.h 
from it's findings. It will then write a 'makefile' and run make dirs to 
create the output directory tree.

It is not my belief that autoconf should be used to generate substantial 
source code markup to escape OS problems. If an OS problem does crop up 
it can likely be corrected by installing the correct files into the 
build include/ dir and perhaps writing some replacement code and 
linking it in. To the fullest extent possible the source code should conform
to standards and not cater to broken systems.

Autoconf will also write a makefile into the top level of the build dir, 
this simply acts as a wrapper to the main top level make in the source tree.
There is one big warning, you can't use both this make file and the
ones in the top level tree. Make is not able to resolve rules that 
go to the same file through different paths and this will confuse the
depends mechanism. I recommend always using the makefiles in the
source directory and exporting BUILD.
