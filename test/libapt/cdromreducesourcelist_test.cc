#include <apt-pkg/cdrom.h>
#include <apt-pkg/error.h>

#include <algorithm>
#include <string>
#include <vector>

#include "assert.h"

class Cdrom : public pkgCdrom {
public:
   std::vector<std::string> ReduceSourcelist(std::string CD,std::vector<std::string> List) {
      pkgCdrom::ReduceSourcelist(CD, List);
      return List;
   }
};

int main(int argc, char const *argv[]) {
   Cdrom cd;
   std::vector<std::string> List;
   std::string CD("/media/cdrom/");

   std::vector<std::string> R = cd.ReduceSourcelist(CD, List);
   equals(R.empty(), true);

   List.push_back(" wheezy main");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 1);
   equals(R[0], " wheezy main");

   List.push_back(" wheezy main");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 1);
   equals(R[0], " wheezy main");

   List.push_back(" wheezy contrib");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 1);
   equals(R[0], " wheezy contrib main");

   List.push_back(" wheezy-update contrib");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 2);
   equals(R[0], " wheezy contrib main");
   equals(R[1], " wheezy-update contrib");

   List.push_back(" wheezy-update contrib");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 2);
   equals(R[0], " wheezy contrib main");
   equals(R[1], " wheezy-update contrib");

   List.push_back(" wheezy-update non-free");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 2);
   equals(R[0], " wheezy contrib main");
   equals(R[1], " wheezy-update contrib non-free");

   List.push_back(" wheezy-update main");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 2);
   equals(R[0], " wheezy contrib main");
   equals(R[1], " wheezy-update contrib main non-free");

   List.push_back(" wheezy non-free");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 2);
   equals(R[0], " wheezy contrib main non-free");
   equals(R[1], " wheezy-update contrib main non-free");

   List.push_back(" sid main");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 3);
   equals(R[0], " sid main");
   equals(R[1], " wheezy contrib main non-free");
   equals(R[2], " wheezy-update contrib main non-free");

   List.push_back(" sid main-reduce");
   R = cd.ReduceSourcelist(CD, List);
   equals(R.size(), 3);
   equals(R[0], " sid main main-reduce");
   equals(R[1], " wheezy contrib main non-free");
   equals(R[2], " wheezy-update contrib main non-free");

   return 0;
}
