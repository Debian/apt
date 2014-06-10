#ifndef SOURCES_H
#define SOURCES_H

#include <apt-pkg/tagfile.h>

class DscExtract 
{
 public:
   //FIXME: do we really need to enforce a maximum size of the dsc file?
   static const int maxSize = 128*1024;

   char *Data;
   pkgTagSection Section;
   unsigned long Length;
   bool IsClearSigned;

   bool TakeDsc(const void *Data, unsigned long Size);
   bool Read(std::string FileName);
   
   DscExtract() : Data(0), Length(0) {
     Data = new char[maxSize];
   };
   ~DscExtract() { 
      if(Data != NULL) {
         delete [] Data;
         Data = NULL;
      } 
   };
};


#endif
