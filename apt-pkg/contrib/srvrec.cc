// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   SRV record support

   ##################################################################### */
									/*}}}*/
#include <config.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <apt-pkg/error.h>
#include "srvrec.h"

bool GetSrvRecords(std::string name, std::vector<SrvRec> &Result)
{
   unsigned char answer[PACKETSZ];
   int answer_len, compressed_name_len;
   int answer_count;

   if (res_init() != 0)
      return _error->Errno("res_init", "Failed to init resolver");

   answer_len = res_query(name.c_str(), C_IN, T_SRV, answer, sizeof(answer));
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
      SrvRec rec;
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
      rec.priority = priority;
      rec.weight = weight;
      rec.port = port;
      rec.target = buf;
      Result.push_back(rec);
   }

   return true;
}
