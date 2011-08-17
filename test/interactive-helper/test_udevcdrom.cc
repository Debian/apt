#include <apt-pkg/cdrom.h>
#include <stdio.h>
#include <assert.h>

int main()
{
   pkgUdevCdromDevices c;
   assert(c.Dlopen());

   vector<CdromDevice> l;
   l = c.Scan();
   assert(l.empty() == false);
   for (size_t i = 0; i < l.size(); ++i)
      std::cerr << l[i].DeviceName << " " 
		<< l[i].Mounted << " " 
		<< l[i].MountPath << std::endl;
   
}
