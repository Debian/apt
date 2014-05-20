// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   SRV record support
  
   ##################################################################### */
									/*}}}*/
#ifndef SRVREC_H
#define SRVREC_H

#include <arpa/nameser.h>
#include <vector>
#include <string>

class SrvRec
{
 public:
   std::string target;
   u_int16_t priority;
   u_int16_t weight;
   u_int16_t port;

   bool operator<(SrvRec const &other) const { 
      return this->priority < other.priority; 
   }
};

bool GetSrvRecords(std::string name, std::vector<SrvRec> &Result);

#endif
