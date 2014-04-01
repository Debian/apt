#include <config.h>

#include <apt-pkg/install-progress.h>

#include <string>

#include "assert.h"

int main() {
   APT::Progress::PackageManagerFancy p;
   std::string s;   

   s= p.GetTextProgressStr(0.5, 60);
   equals(s.size(), 60);
   
   s= p.GetTextProgressStr(0.5, 4);
   equals(s, "[#.]");

   s= p.GetTextProgressStr(0.1, 12);
   equals(s, "[#.........]");
   
   s= p.GetTextProgressStr(0.9, 12);
   equals(s, "[#########.]");

   // deal with incorrect inputs gracefully (or should we die instead?)
   s= p.GetTextProgressStr(-999, 12);
   equals(s, "");

   return 0;
}
