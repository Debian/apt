// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   SRV record support

   ##################################################################### */
									/*}}}*/
#include <config.h>

#include <netdb.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <time.h>

#include <algorithm>
#include <tuple>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>


#include "srvrec.h"


bool SrvRec::operator==(SrvRec const &other) const
{
   return (std::tie(target, priority, weight, port) ==
           std::tie(other.target, other.priority, other.weight, other.port));
}

bool GetSrvRecords(std::string host, int port, std::vector<SrvRec> &Result)
{
   std::string target;
   int res;
   struct servent s_ent_buf;
   struct servent *s_ent = nullptr;
   std::vector<char> buf(1024);

   res = getservbyport_r(htons(port), "tcp", &s_ent_buf, buf.data(), buf.size(), &s_ent);
   if (res != 0 || s_ent == nullptr)
      return false;

   strprintf(target, "_%s._tcp.%s", s_ent->s_name, host.c_str());
   return GetSrvRecords(target, Result);
}

bool GetSrvRecords(std::string name, std::vector<SrvRec> &Result)
{
   unsigned char answer[PACKETSZ];
   int answer_len, compressed_name_len;
   int answer_count;

   if (res_init() != 0)
      return _error->Errno("res_init", "Failed to init resolver");

   answer_len = res_query(name.c_str(), C_IN, T_SRV, answer, sizeof(answer));
   if (answer_len == -1)
      return false;
   if (answer_len < (int)sizeof(HEADER))
      return _error->Warning("Not enough data from res_query (%i)", answer_len);

   // check the header
   HEADER *header = (HEADER*)answer;
   if (header->rcode != NOERROR)
      return _error->Warning("res_query returned rcode %i", header->rcode);
   answer_count = ntohs(header->ancount);
   if (answer_count <= 0)
      return _error->Warning("res_query returned no answers (%i) ", answer_count);

   // skip the header
   compressed_name_len = dn_skipname(answer+sizeof(HEADER), answer+answer_len);
   if(compressed_name_len < 0)
      return _error->Warning("dn_skipname failed %i", compressed_name_len);

   // pt points to the first answer record, go over all of them now
   unsigned char *pt = answer+sizeof(HEADER)+compressed_name_len+QFIXEDSZ;
   while ((int)Result.size() < answer_count && pt < answer+answer_len)
   {
      u_int16_t type, klass, priority, weight, port, dlen;
      char buf[MAXDNAME];

      compressed_name_len = dn_skipname(pt, answer+answer_len);
      if (compressed_name_len < 0)
         return _error->Warning("dn_skipname failed (2): %i",
                                compressed_name_len);
      pt += compressed_name_len;
      if (((answer+answer_len) - pt) < 16)
         return _error->Warning("packet too short");

      // extract the data out of the result buffer
      #define extract_u16(target, p) target = *p++ << 8; target |= *p++;

      extract_u16(type, pt);
      if(type != T_SRV)
         return _error->Warning("Unexpected type excepted %x != %x",
                                T_SRV, type);
      extract_u16(klass, pt);
      if(klass != C_IN)
         return _error->Warning("Unexpected class excepted %x != %x",
                                C_IN, klass);
      pt += 4;  // ttl
      extract_u16(dlen, pt);
      extract_u16(priority, pt);
      extract_u16(weight, pt);
      extract_u16(port, pt);

      #undef extract_u16

      compressed_name_len = dn_expand(answer, answer+answer_len, pt, buf, sizeof(buf));
      if(compressed_name_len < 0)
         return _error->Warning("dn_expand failed %i", compressed_name_len);
      pt += compressed_name_len;

      // add it to our class
      Result.emplace_back(buf, priority, weight, port);
   }

   // implement load balancing as specified in RFC-2782

   // sort them by priority
   std::stable_sort(Result.begin(), Result.end());

   for(std::vector<SrvRec>::iterator I = Result.begin();
      I != Result.end(); ++I)
   {
      if (_config->FindB("Debug::Acquire::SrvRecs", false) == true)
      {
         std::cerr << "SrvRecs: got " << I->target
                   << " prio: " << I->priority
                   << " weight: " << I->weight
                   << std::endl;
      }
   }

   return true;
}

SrvRec PopFromSrvRecs(std::vector<SrvRec> &Recs)
{
   // FIXME: instead of the simplistic shuffle below use the algorithm
   //        described in rfc2782 (with weights)
   //        and figure out how the weights need to be adjusted if
   //        a host refuses connections

#if 0  // all code below is only needed for the weight adjusted selection 
   // assign random number ranges
   int prev_weight = 0;
   int prev_priority = 0;
   for(std::vector<SrvRec>::iterator I = Result.begin();
      I != Result.end(); ++I)
   {
      if(prev_priority != I->priority)
         prev_weight = 0;
      I->random_number_range_start = prev_weight;
      I->random_number_range_end = prev_weight + I->weight;
      prev_weight = I->random_number_range_end;
      prev_priority = I->priority;

      if (_config->FindB("Debug::Acquire::SrvRecs", false) == true)
         std::cerr << "SrvRecs: got " << I->target
                   << " prio: " << I->priority
                   << " weight: " << I->weight
                   << std::endl;
   }

   // go over the code in reverse order and note the max random range
   int max = 0;
   prev_priority = 0;
   for(std::vector<SrvRec>::iterator I = Result.end();
      I != Result.begin(); --I)
   {
      if(prev_priority != I->priority)
         max = I->random_number_range_end;
      I->random_number_range_max = max;
   }
#endif

   // shuffle in a very simplistic way for now (equal weights)
   std::vector<SrvRec>::iterator I = Recs.begin();
   std::vector<SrvRec>::iterator const J = std::find_if(Recs.begin(), Recs.end(),
	 [&I](SrvRec const &J) { return I->priority != J.priority; });

   // clock seems random enough.
   I += std::max(static_cast<clock_t>(0), clock()) % std::distance(I, J);
   SrvRec const selected = std::move(*I);
   Recs.erase(I);

   if (_config->FindB("Debug::Acquire::SrvRecs", false) == true)
      std::cerr << "PopFromSrvRecs: selecting " << selected.target << std::endl;

   return selected;
}
