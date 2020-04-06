// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   OpProgress - Operation Progress
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/progress.h>

#include <cmath>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/time.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// OpProgress::OpProgress - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
OpProgress::OpProgress() : Current(0), Total(0), Size(0), SubTotal(1), 
                           LastPercent(0), Percent(0)
{
   memset(&LastTime,0,sizeof(LastTime));
}
									/*}}}*/
// OpProgress::Progress - Sub progress with no state change		/*{{{*/
// ---------------------------------------------------------------------
/* Current is the Base Overall progress in units of Total. Cur is the sub
   progress in units of SubTotal. Size is a scaling factor that says what
   percent of Total SubTotal is. */
void OpProgress::Progress(unsigned long long Cur)
{
   if (Total == 0 || Size == 0 || SubTotal == 0)
      Percent = 0;
   else
      Percent = (Current + Cur/((double)SubTotal)*Size)*100.0/Total;
   Update();
}
									/*}}}*/
// OpProgress::OverallProgress - Set the overall progress		/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpProgress::OverallProgress(unsigned long long Current, unsigned long long Total,
	  			 unsigned long long Size,const string &Op)
{
   this->Current = Current;
   this->Total = Total;
   this->Size = Size;
   this->Op = Op;
   SubOp = string();
   if (Total == 0)
      Percent = 0;
   else
      Percent = Current*100.0/Total;
   Update();
}
									/*}}}*/
// OpProgress::SubProgress - Set the sub progress state			/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpProgress::SubProgress(unsigned long long SubTotal,const string &Op,
			     float const Percent)
{
   this->SubTotal = SubTotal;
   if (Op.empty() == false)
      SubOp = Op;
   if (Total == 0 || Percent == 0)
      this->Percent = 0;
   else if (Percent != -1)
      this->Percent = this->Current += (Size*Percent)/SubTotal;
   else
      this->Percent = Current*100.0/Total;
   Update();
}
									/*}}}*/
// OpProgress::CheckChange - See if the display should be updated	/*{{{*/
// ---------------------------------------------------------------------
/* Progress calls are made so frequently that if every one resulted in 
   an update the display would be swamped and the system much slower.
   This provides an upper bound on the update rate. */
bool OpProgress::CheckChange(float Interval)
{
   // For absolute progress, we assume every call is relevant.
   if (_config->FindB("APT::Internal::OpProgress::Absolute", false))
      return true;
   // New major progress indication
   if (Op != LastOp)
   {
      MajorChange = true;
      LastOp = Op;
      return true;
   }
   MajorChange = false;

   if (SubOp != LastSubOp)
   {
      LastSubOp = SubOp;
      return true;
   }
   
   if (std::lround(LastPercent) == std::lround(Percent))
      return false;

   LastPercent = Percent;
   
   if (Interval == 0)
      return false;
   
   // Check time delta
   auto const Now = std::chrono::steady_clock::now().time_since_epoch();
   auto const Now_sec = std::chrono::duration_cast<std::chrono::seconds>(Now);
   auto const Now_usec = std::chrono::duration_cast<std::chrono::microseconds>(Now - Now_sec);
   struct timeval NowTime = { Now_sec.count(), Now_usec.count() };

   std::chrono::duration<decltype(Interval)> Delta =
      std::chrono::seconds(NowTime.tv_sec - LastTime.tv_sec) +
      std::chrono::microseconds(NowTime.tv_usec - LastTime.tv_usec);

   if (Delta.count() < Interval)
      return false;
   LastTime = NowTime;
   return true;
}
									/*}}}*/
// OpTextProgress::OpTextProgress - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
OpTextProgress::OpTextProgress(Configuration &Config) :
                               NoUpdate(false), NoDisplay(false), LastLen(0)
{
   if (Config.FindI("quiet",0) >= 1 || Config.FindB("quiet::NoUpdate", false) == true)
      NoUpdate = true;
   if (Config.FindI("quiet",0) >= 2 || Config.FindB("quiet::NoProgress", false) == true)
      NoDisplay = true;
}
									/*}}}*/
// OpTextProgress::Done - Clean up the display				/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpTextProgress::Done()
{
   if (NoUpdate == false && OldOp.empty() == false)
   {
      char S[300];
      if (_error->PendingError() == true)
	 snprintf(S,sizeof(S),_("%c%s... Error!"),'\r',OldOp.c_str());
      else
	 snprintf(S,sizeof(S),_("%c%s... Done"),'\r',OldOp.c_str());
      Write(S);
      cout << endl;
      OldOp = string();
   }

   if (NoUpdate == true && NoDisplay == false && OldOp.empty() == false)
   {
      OldOp = string();
      cout << endl;
   }
}
									/*}}}*/
// OpTextProgress::Update - Simple text spinner				/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpTextProgress::Update()
{
   if (CheckChange((NoUpdate == true?0:0.7)) == false)
      return;
   
   // No percent spinner
   if (NoUpdate == true)
   {
      if (MajorChange == false)
	 return;
      if (NoDisplay == false)
      {
	 if (OldOp.empty() == false)
	    cout << endl;
	 OldOp = "a";
	 cout << Op << _("...") << flush;
      }
      
      return;
   }

   // Erase the old text and 'log' the event
   char S[300];
   if (MajorChange == true && OldOp.empty() == false)
   {
      snprintf(S,sizeof(S),"\r%s",OldOp.c_str());
      Write(S);
      cout << endl;
   }

   // Print the spinner. Absolute progress shows us a time progress.
   if (_config->FindB("APT::Internal::OpProgress::Absolute", false) && Total != -1llu)
      snprintf(S, sizeof(S), _("%c%s... %llu/%llus"), '\r', Op.c_str(), Current, Total);
   else if (_config->FindB("APT::Internal::OpProgress::Absolute", false))
      snprintf(S, sizeof(S), _("%c%s... %llus"), '\r', Op.c_str(), Current);
   else
      snprintf(S, sizeof(S), _("%c%s... %u%%"), '\r', Op.c_str(), (unsigned int)Percent);
   Write(S);

   OldOp = Op;
}
									/*}}}*/
// OpTextProgress::Write - Write the progress string			/*{{{*/
// ---------------------------------------------------------------------
/* This space fills the end to overwrite the previous text */
void OpTextProgress::Write(const char *S)
{
   cout << S;
   for (unsigned int I = strlen(S); I < LastLen; I++)
      cout << ' ';
   cout << '\r' << flush;
   LastLen = strlen(S);
}
									/*}}}*/
