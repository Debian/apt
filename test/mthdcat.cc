/* Usage, mthdcat < cmds | methods/mthd
   All this does is cat a file into the method without closing the FD when
   the file ends */

#include <unistd.h>

int main()
{
   char Buffer[4096];
   
   while (1)
   {
      int Res = read(STDIN_FILENO,Buffer,sizeof(Buffer));
      if (Res <= 0)
	 while (1) sleep(100);
      if (write(STDOUT_FILENO,Buffer,Res) != Res)
	 break;
   }
   return 0;
}
