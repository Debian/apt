#include <apt-pkg/debversion.h>
#include <rpm/rpmio.h>
#include <rpm/misc.h>
#include <stdlib.h>

int main(int argc,const char *argv[])
{
   printf("'%s' <> '%s':  ",argv[1],argv[2]);
   printf("rpm: %i   deb:  %i\n",rpmvercmp(argv[1],argv[2]),
	  debVS.CmpFragment(argv[1],argv[1]+strlen(argv[1]),
			    argv[2],argv[2]+strlen(argv[2])));
   
   printf("'%s' <> '%s':  ",argv[2],argv[1]);
   printf("rpm: %i   deb:  %i\n",rpmvercmp(argv[2],argv[1]),
	  debVS.CmpFragment(argv[2],argv[2]+strlen(argv[2]),
			    argv[1],argv[1]+strlen(argv[1])));
   return 0;
}
