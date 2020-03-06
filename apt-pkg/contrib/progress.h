// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   OpProgress - Operation Progress
   
   This class allows lengthy operations to communicate their progress 
   to the GUI. The progress model is simple and is not designed to handle
   the complex case of the multi-activity acquire class.
   
   The model is based on the concept of an overall operation consisting
   of a series of small sub operations. Each sub operation has it's own
   completion status and the overall operation has it's completion status.
   The units of the two are not mixed and are completely independent.
   
   The UI is expected to subclass this to provide the visuals to the user.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_PROGRESS_H
#define PKGLIB_PROGRESS_H

#include <apt-pkg/macros.h>
#include <string>
#include <sys/time.h>


class Configuration;
class APT_PUBLIC OpProgress
{
   friend class OpTextProgress;
   unsigned long long Current;
   unsigned long long Total;
   unsigned long long Size;
   unsigned long long SubTotal;
   float LastPercent;
   
   // Change reduction code
   struct timeval LastTime;
   std::string LastOp;
   std::string LastSubOp;
   
   protected:
   
   std::string Op;
   std::string SubOp;
   float Percent;
   
   bool MajorChange;
   
   bool CheckChange(float Interval = 0.7);		    
   virtual void Update() {};
   
   public:
   
   void Progress(unsigned long long Current);
   void SubProgress(unsigned long long SubTotal, const std::string &Op = "", float const Percent = -1);
   void OverallProgress(unsigned long long Current,unsigned long long Total,
			unsigned long long Size,const std::string &Op);
   virtual void Done() {};
   
   OpProgress();
   virtual ~OpProgress() {};
};

class APT_PUBLIC OpTextProgress : public OpProgress
{
   protected:

   std::string OldOp;
   bool NoUpdate;
   bool NoDisplay;
   unsigned long LastLen;
   virtual void Update() APT_OVERRIDE;
   void Write(const char *S);
   
   public:

   virtual void Done() APT_OVERRIDE;
   
   explicit OpTextProgress(bool NoUpdate = false) : NoUpdate(NoUpdate),
                NoDisplay(false), LastLen(0) {};
   explicit OpTextProgress(Configuration &Config);
   virtual ~OpTextProgress() {Done();};
};

#endif
