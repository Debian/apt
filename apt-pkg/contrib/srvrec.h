// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   SRV record support

   ##################################################################### */
									/*}}}*/
#ifndef SRVREC_H
#define SRVREC_H

#include <string>
#include <vector>
#include <arpa/nameser.h>

class SrvRec
{
 public:
   std::string target;
   u_int16_t priority;
   u_int16_t weight;
   u_int16_t port;

   // each server is assigned a interval [start, end] in the space of [0, max]
   int random_number_range_start;
   int random_number_range_end;
   int random_number_range_max;

   bool operator<(SrvRec const &other) const {
      return this->priority < other.priority;
   }
   bool operator==(SrvRec const &other) const;

   SrvRec(std::string const Target, u_int16_t const Priority,
	 u_int16_t const Weight, u_int16_t const Port) :
      target(Target), priority(Priority), weight(Weight), port(Port),
      random_number_range_start(0), random_number_range_end(0),
      random_number_range_max(0) {}
};

/** \brief Get SRV records from host/port (builds the query string internally) 
 */
bool GetSrvRecords(std::string name, std::vector<SrvRec> &Result);

/** \brief Get SRV records for query string like: _http._tcp.example.com
 */
bool GetSrvRecords(std::string host, int port, std::vector<SrvRec> &Result);

/** \brief Pop a single SRV record from the vector of SrvRec taking
 *         priority and weight into account
 */
SrvRec PopFromSrvRecs(std::vector<SrvRec> &Recs);

#endif
