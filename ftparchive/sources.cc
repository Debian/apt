#include <string>
#include <sstream>

// for memcpy
#include <cstring>

#include <apt-pkg/error.h>
#include <apt-pkg/gpgv.h>

#include "sources.h"

bool DscExtract::TakeDsc(const void *newData, unsigned long long newSize)
{
   if (newSize == 0)
   {
      // adding two newlines 'off record' for pkgTagSection.Scan() calls
      Data = "\n\n";
      Length = 0;
      return true;
   }

   Data = std::string((const char*)newData, newSize);
   // adding two newlines 'off record' for pkgTagSection.Scan() calls
   Data.append("\n\n");
   Length = newSize;

   return true;
}

bool DscExtract::Read(std::string FileName)
{
   Data.clear();
   Length = 0;

   FileFd F;
   if (OpenMaybeClearSignedFile(FileName, F) == false)
      return false;

   IsClearSigned = (FileName != F.Name());

   std::ostringstream data;
   char buffer[1024];
   do {
      unsigned long long actual = 0;
      if (F.Read(buffer, sizeof(buffer)-1, &actual) == false)
	 return _error->Errno("read", "Failed to read dsc file %s", FileName.c_str());
      if (actual == 0)
	 break;
      Length += actual;
      buffer[actual] = '\0';
      data << buffer;
   } while(true);

   // adding two newlines 'off record' for pkgTagSection.Scan() calls
   data << "\n\n";
   Data = data.str();
   return true;
}


