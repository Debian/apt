// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Delay URI - This method takes a uri encoding a delay as a hostname and
               the real URI as the path, sleeps for the specified delay,
	       and then redirects back to the real URI.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include "aptmethod.h"
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include <string>
#include <sys/stat.h>
#include <sys/time.h>

#include <apti18n.h>
									/*}}}*/

class DelayMethod : public aptMethod
{
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;

   public:
   DelayMethod() : aptMethod("delay", "1.0", SingleInstance | SendConfig | SendURIEncoded)
   {
      SeccompFlags = aptMethod::BASE;
   }
};
// DelayMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DelayMethod::Fetch(FetchItem *Itm)
{
   URI U(Itm->Uri);

   int delay = atoi(U.Host.c_str());

   // FIXME: We don't StartURI here, are we bad? We don't want to clutter
   // the log file with confusing Get lines, do we?

   if (DebugEnabled())
      std::clog << "Sleep " << delay << " seconds"
		<< "\n";
   sleep(delay);

   Redirect(DeQuoteString(U.Path.substr(1)));

   return true;
}
									/*}}}*/

int main()
{
   return DelayMethod().Run();
}
