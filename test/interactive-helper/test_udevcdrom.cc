#include <config.h>

#include <apt-pkg/cdrom.h>

#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <stddef.h>

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
