// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acqprogress.cc,v 1.11 1999/03/16 00:43:55 jgg Exp $
/* ######################################################################

   Acquire Progress - Command line progress meter 
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include "acqprogress.h"
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/strutl.h>

#include <stdio.h>
#include <signal.h>
									/*}}}*/

// AcqTextStatus::AcqTextStatus - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
AcqTextStatus::AcqTextStatus(unsigned int &ScreenWidth,unsigned int Quiet) :
    ScreenWidth(ScreenWidth), Quiet(Quiet)
{
}
									/*}}}*/
// AcqTextStatus::Start - Downloading has started			/*{{{*/
// ---------------------------------------------------------------------
/* */
void AcqTextStatus::Start() 
{
   pkgAcquireStatus::Start(); 
   BlankLine[0] = 0;
   ID = 1;
};
									/*}}}*/
// AcqTextStatus::IMSHit - Called when an item got a HIT response	/*{{{*/
// ---------------------------------------------------------------------
/* */
void AcqTextStatus::IMSHit(pkgAcquire::ItemDesc &Itm)
{
   if (Quiet > 1)
      return;

   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';   
   
   cout << "Hit " << Itm.Description;
   if (Itm.Owner->FileSize != 0)
      cout << " [" << SizeToStr(Itm.Owner->FileSize) << "b]";
   cout << endl;
   Update = true;
};
									/*}}}*/
// AcqTextStatus::Fetch - An item has started to download		/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the short description and the expected size */
void AcqTextStatus::Fetch(pkgAcquire::ItemDesc &Itm)
{
   Update = true;
   if (Itm.Owner->Complete == true)
      return;
   
   Itm.Owner->ID = ID++;
   
   if (Quiet > 1)
      return;

   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';
   
   cout << "Get:" << hex << Itm.Owner->ID << dec << ' ' << Itm.Description;
   if (Itm.Owner->FileSize != 0)
      cout << " [" << SizeToStr(Itm.Owner->FileSize) << "b]";
   cout << endl;
};
									/*}}}*/
// AcqTextStatus::Done - Completed a download				/*{{{*/
// ---------------------------------------------------------------------
/* We don't display anything... */
void AcqTextStatus::Done(pkgAcquire::ItemDesc &Itm)
{
   Update = true;
};
									/*}}}*/
// AcqTextStatus::Fail - Called when an item fails to download		/*{{{*/
// ---------------------------------------------------------------------
/* We print out the error text  */
void AcqTextStatus::Fail(pkgAcquire::ItemDesc &Itm)
{
   if (Quiet > 1)
      return;

   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';
   
   if (Itm.Owner->Status == pkgAcquire::Item::StatDone)
   {
      cout << "Ign " << Itm.Description << endl;
   }
   else
   {
      cout << "Err " << Itm.Description << endl;
      cout << "  " << Itm.Owner->ErrorText << endl;
   }
   
   Update = true;
};
									/*}}}*/
// AcqTextStatus::Stop - Finished downloading				/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the bytes downloaded and the overall average line
   speed */
void AcqTextStatus::Stop()
{
   pkgAcquireStatus::Stop();
   if (Quiet > 1)
      return;

   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';
   
   if (FetchedBytes != 0)
      cout << "Fetched " << SizeToStr(FetchedBytes) << "b in " <<
         TimeToStr(ElapsedTime) << " (" << SizeToStr(CurrentCPS) << 
         "b/s)" << endl;
}
									/*}}}*/
// AcqTextStatus::Pulse - Regular event pulse				/*{{{*/
// ---------------------------------------------------------------------
/* This draws the current progress. Each line has an overall percent
   meter and a per active item status meter along with an overall 
   bandwidth and ETA indicator. */
void AcqTextStatus::Pulse(pkgAcquire *Owner)
{
   if (Quiet > 0)
      return;
   
   pkgAcquireStatus::Pulse(Owner);
   
   enum {Long = 0,Medium,Short} Mode = Long;
   
   char Buffer[300];
   char *End = Buffer + sizeof(Buffer);
   char *S = Buffer;
   
   // Put in the percent done
   sprintf(S,"%ld%%",long(double((CurrentBytes + CurrentItems)*100.0)/double(TotalBytes+TotalItems)));

   bool Shown = false;
   for (pkgAcquire::Worker *I = Owner->WorkersBegin(); I != 0;
	I = Owner->WorkerStep(I))
   {
      S += strlen(S);
      
      // There is no item running 
      if (I->CurrentItem == 0)
      {
	 if (I->Status.empty() == false)
	 {
	    snprintf(S,End-S," [%s]",I->Status.c_str());
	    Shown = true;
	 }
	 
	 continue;
      }

      Shown = true;
      
      // Add in the short description
      if (I->CurrentItem->Owner->ID != 0)
	 snprintf(S,End-S," [%x %s",I->CurrentItem->Owner->ID,
		  I->CurrentItem->ShortDesc.c_str());
      else
	 snprintf(S,End-S," [%s",I->CurrentItem->ShortDesc.c_str());
      S += strlen(S);

      // Show the short mode string
      if (I->CurrentItem->Owner->Mode != 0)
      {
	 snprintf(S,End-S," %s",I->CurrentItem->Owner->Mode);
	 S += strlen(S);
      }
            
      // Add the current progress
      if (Mode == Long)
	 snprintf(S,End-S," %u",I->CurrentSize);
      else
      {
	 if (Mode == Medium || I->TotalSize == 0)
	    snprintf(S,End-S," %sb",SizeToStr(I->CurrentSize).c_str());
      }
      S += strlen(S);
      
      // Add the total size and percent
      if (I->TotalSize > 0 && I->CurrentItem->Owner->Complete == false)
      {
	 if (Mode == Short)
	    snprintf(S,End-S," %u%%",
		     long(double(I->CurrentSize*100.0)/double(I->TotalSize)));
	 else
	    snprintf(S,End-S,"/%sb %u%%",SizeToStr(I->TotalSize).c_str(),
		     long(double(I->CurrentSize*100.0)/double(I->TotalSize)));
      }      
      S += strlen(S);
      snprintf(S,End-S,"]");
   }

   // Show something..
   if (Shown == false)
      snprintf(S,End-S," [Working]");
      
   /* Put in the ETA and cps meter, block off signals to prevent strangeness
      during resizing */
   sigset_t Sigs,OldSigs;
   sigemptyset(&Sigs);
   sigaddset(&Sigs,SIGWINCH);
   sigprocmask(SIG_BLOCK,&Sigs,&OldSigs);
   
   if (CurrentCPS != 0)
   {      
      char Tmp[300];
      unsigned long ETA = (unsigned long)((TotalBytes - CurrentBytes)/CurrentCPS);
      sprintf(Tmp," %sb/s %s",SizeToStr(CurrentCPS).c_str(),TimeToStr(ETA).c_str());
      unsigned int Len = strlen(Buffer);
      unsigned int LenT = strlen(Tmp);
      if (Len + LenT < ScreenWidth)
      {	 
	 memset(Buffer + Len,' ',ScreenWidth - Len);
	 strcpy(Buffer + ScreenWidth - LenT,Tmp);
      }      
   }
   Buffer[ScreenWidth] = 0;
   BlankLine[ScreenWidth] = 0;
   sigprocmask(SIG_UNBLOCK,&OldSigs,0);

   // Draw the current status
   if (strlen(Buffer) == strlen(BlankLine))
      cout << '\r' << Buffer << flush;
   else
      cout << '\r' << BlankLine << '\r' << Buffer << flush;
   memset(BlankLine,' ',strlen(Buffer));
   BlankLine[strlen(Buffer)] = 0;
   
   Update = false;
}
									/*}}}*/
// AcqTextStatus::MediaChange - Media need to be swapped		/*{{{*/
// ---------------------------------------------------------------------
/* Prompt for a media swap */
bool AcqTextStatus::MediaChange(string Media,string Drive)
{
   if (Quiet <= 0)
      cout << '\r' << BlankLine << '\r';   
   cout << "Media Change: Please insert the disc labeled '" << Media << "' in "\
           "the drive '" << Drive << "' and press enter" << endl;

   char C = 0;
   while (C != '\n' && C != '\r')
      read(STDIN_FILENO,&C,1);
   
   Update = true;
   return true;
}
									/*}}}*/
