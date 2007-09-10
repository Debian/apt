// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: progress.cc,v 1.12 2003/01/11 07:17:04 jgg Exp $
/* ######################################################################
   
   OpProgress - Operation Progress
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/progress.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>

#include <apti18n.h>

#include <iostream>
#include <stdio.h>
#include <cstring>
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
void OpProgress::Progress(unsigned long Cur)
{
   if (Total == 0 || Size == 0 || SubTotal == 0)
      Percent = 0;
   else
      Percent = (Current + Cur/((float)SubTotal)*Size)*100.0/Total;
   Update();
}
									/*}}}*/
// OpProgress::OverallProgress - Set the overall progress		/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpProgress::OverallProgress(unsigned long Current, unsigned long Total,
	  			 unsigned long Size,const string &Op)
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
void OpProgress::SubProgress(unsigned long SubTotal,const string &Op)
{
   this->SubTotal = SubTotal;
   SubOp = Op;
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
void OpProgress::SubProgress(unsigned long SubTotal)
{
   this->SubTotal = SubTotal;
   if (Total == 0)
      Percent = 0;
   else
      Percent = Current*100.0/Total;
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
   
   if ((int)LastPercent == (int)Percent)
      return false;

   LastPercent = Percent;
   
   if (Interval == 0)
      return false;
   
   // Check time delta
   struct timeval Now;
   gettimeofday(&Now,0);
   double Diff = Now.tv_sec - LastTime.tv_sec + (Now.tv_usec - LastTime.tv_usec)/1000000.0;
   if (Diff < Interval)
      return false;
   LastTime = Now;   
   return true;
}
									/*}}}*/
// OpTextProgress::OpTextProgress - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
OpTextProgress::OpTextProgress(Configuration &Config) : 
                               NoUpdate(false), NoDisplay(false), LastLen(0) 
{
   if (Config.FindI("quiet",0) >= 1)
      NoUpdate = true;
   if (Config.FindI("quiet",0) >= 2)
      NoDisplay = true;
};
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
	 cout << Op << "..." << flush;
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
   
   // Print the spinner
   snprintf(S,sizeof(S),"\r%s... %u%%",Op.c_str(),(unsigned int)Percent);
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
