#include <config.h>

#include <apt-pkg/cdrom.h>

#include <stddef.h>
#include <string>
#include <assert.h>
#include <vector>
#include <iostream>

int main()
{
   pkgUdevCdromDevices c;
   assert(c.Dlopen());

   std::vector<CdromDevice> l;
   l = c.Scan();
   assert(l.empty() == false);
   for (size_t i = 0; i < l.size(); ++i)
      std::cerr << l[i].DeviceName << " " 
		<< l[i].Mounted << " " 
		<< l[i].MountPath << std::endl;
}
