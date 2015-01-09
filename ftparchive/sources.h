#ifndef SOURCES_H
#define SOURCES_H

#include <apt-pkg/tagfile.h>

#include <string>

class DscExtract
{
 public:
   std::string Data;
   pkgTagSection Section;
   unsigned long long Length;
   bool IsClearSigned;

   bool TakeDsc(const void *Data, unsigned long long Size);
   bool Read(std::string FileName);

   DscExtract() : Length(0), IsClearSigned(false) {};
   ~DscExtract() {};
};


#endif
