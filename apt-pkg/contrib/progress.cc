// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: progress.cc,v 1.1 1998/07/21 05:33:21 jgg Exp $
/* ######################################################################
   
   OpProgress - Operation Progress
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/progress.h"
#endif 
#include <apt-pkg/progress.h>
#include <stdio.h>
									/*}}}*/

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
/* This assumes that Size is the same as the current sub size */
void OpProgress::Progress(unsigned long Cur)
{
   Percent = (Current + Cur/((float)SubTotal)*Size)*100.0/Total;
   Update();
}
									/*}}}*/
// OpProgress::OverallProgress - Set the overall progress		/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpProgress::OverallProgress(unsigned long Current, unsigned long Total,
	  			 unsigned long Size,string Op)
{
   this->Current = Current;
   this->Total = Total;
   this->Size = Size;
   this->Op = Op;
   SubOp = string();
   Percent = Current*100.0/Total;
   Update();
}
									/*}}}*/
// OpProgress::SubProgress - Set the sub progress state			/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpProgress::SubProgress(unsigned long SubTotal,string Op)
{
   this->SubTotal = SubTotal;
   SubOp = Op;
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

   if ((int)LastPercent == (int)Percent)
      return false;
   LastPercent = Percent;
   
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
// OpTextProgress::Done - Clean up the display				/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpTextProgress::Done()
{
   if (NoUpdate == false && OldOp.empty() == false)
   {
      char S[300];
      snprintf(S,sizeof(S),"\r%s",OldOp.c_str());
      Write(S);
      cout << endl;
      OldOp = string();
   }      
}
									/*}}}*/
// OpTextProgress::Update - Simple text spinner				/*{{{*/
// ---------------------------------------------------------------------
/* */
void OpTextProgress::Update()
{
   if (CheckChange(0) == false)
      return;
   
   // No percent spinner
   if (NoUpdate == true)
   {
      if (MajorChange == false)
	 return;
      cout << Op << endl;
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
