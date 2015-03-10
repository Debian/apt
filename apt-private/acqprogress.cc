// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acqprogress.cc,v 1.24 2003/04/27 01:56:48 doogie Exp $
/* ######################################################################

   Acquire Progress - Command line progress meter 
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include<config.h>

#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>

#include <apt-private/acqprogress.h>

#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// AcqTextStatus::AcqTextStatus - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
AcqTextStatus::AcqTextStatus(unsigned int &ScreenWidth,unsigned int const Quiet) :
    pkgAcquireStatus(), ScreenWidth(ScreenWidth), LastLineLength(0), ID(0), Quiet(Quiet)
{
   // testcases use it to disable pulses without disabling other user messages
   if (Quiet == 0 && _config->FindB("quiet::NoUpdate", false) == true)
      this->Quiet = 1;
}
									/*}}}*/
// AcqTextStatus::Start - Downloading has started			/*{{{*/
// ---------------------------------------------------------------------
/* */
void AcqTextStatus::Start()
{
   pkgAcquireStatus::Start();
   LastLineLength = 0;
   ID = 1;
}
									/*}}}*/
// AcqTextStatus::IMSHit - Called when an item got a HIT response	/*{{{*/
// ---------------------------------------------------------------------
/* */
void AcqTextStatus::IMSHit(pkgAcquire::ItemDesc &Itm)
{
   if (Quiet > 1)
      return;

   clearLastLine();

   cout << _("Hit ") << Itm.Description;
   cout << endl;
   Update = true;
}
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

   clearLastLine();

   cout << _("Get:") << Itm.Owner->ID << ' ' << Itm.Description;
   if (Itm.Owner->FileSize != 0)
      cout << " [" << SizeToStr(Itm.Owner->FileSize) << "B]";
   cout << endl;
}
									/*}}}*/
// AcqTextStatus::Done - Completed a download				/*{{{*/
// ---------------------------------------------------------------------
/* We don't display anything... */
void AcqTextStatus::Done(pkgAcquire::ItemDesc &/*Itm*/)
{
   Update = true;
}
									/*}}}*/
// AcqTextStatus::Fail - Called when an item fails to download		/*{{{*/
// ---------------------------------------------------------------------
/* We print out the error text  */
void AcqTextStatus::Fail(pkgAcquire::ItemDesc &Itm)
{
   if (Quiet > 1)
      return;

   // Ignore certain kinds of transient failures (bad code)
   if (Itm.Owner->Status == pkgAcquire::Item::StatIdle)
      return;

   clearLastLine();

   if (Itm.Owner->Status == pkgAcquire::Item::StatDone)
   {
      cout << _("Ign ") << Itm.Description << endl;
      if (Itm.Owner->ErrorText.empty() == false &&
	    _config->FindB("Acquire::Progress::Ignore::ShowErrorText", false) == true)
	 cout << "  " << Itm.Owner->ErrorText << endl;
   }
   else
   {
      cout << _("Err ") << Itm.Description << endl;
      cout << "  " << Itm.Owner->ErrorText << endl;
   }

   Update = true;
}
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

   clearLastLine();

   if (_config->FindB("quiet::NoStatistic", false) == true)
      return;

   if (FetchedBytes != 0 && _error->PendingError() == false)
      ioprintf(cout,_("Fetched %sB in %s (%sB/s)\n"),
	       SizeToStr(FetchedBytes).c_str(),
	       TimeToStr(ElapsedTime).c_str(),
	       SizeToStr(CurrentCPS).c_str());
}
									/*}}}*/
// AcqTextStatus::Pulse - Regular event pulse				/*{{{*/
// ---------------------------------------------------------------------
/* This draws the current progress. Each line has an overall percent
   meter and a per active item status meter along with an overall 
   bandwidth and ETA indicator. */
bool AcqTextStatus::Pulse(pkgAcquire *Owner)
{
   pkgAcquireStatus::Pulse(Owner);

   if (Quiet > 0)
      return true;

   enum {Long = 0,Medium,Short} Mode = Medium;

   std::string Line;
   {
      std::stringstream S;
      for (pkgAcquire::Worker *I = Owner->WorkersBegin(); I != 0;
	    I = Owner->WorkerStep(I))
      {
	 // There is no item running
	 if (I->CurrentItem == 0)
	 {
	    if (I->Status.empty() == false)
	       S << " [" << I->Status << "]";

	    continue;
	 }

	 // Add in the short description
	 S << " [";
	 if (I->CurrentItem->Owner->ID != 0)
	    S << I->CurrentItem->Owner->ID << " ";
	 S << I->CurrentItem->ShortDesc;

	 // Show the short mode string
	 if (I->CurrentItem->Owner->ActiveSubprocess.empty() == false)
	    S << " " << I->CurrentItem->Owner->ActiveSubprocess;

	 // Add the current progress
	 if (Mode == Long)
	    S << " " << I->CurrentSize;
	 else
	 {
	    if (Mode == Medium || I->TotalSize == 0)
	       S << " " << SizeToStr(I->CurrentSize) << "B";
	 }

	 // Add the total size and percent
	 if (I->TotalSize > 0 && I->CurrentItem->Owner->Complete == false)
	 {
	    if (Mode == Short)
	       ioprintf(S, " %.0f%%", (I->CurrentSize*100.0)/I->TotalSize);
	    else
	       ioprintf(S, "/%sB %.0f%%", SizeToStr(I->TotalSize).c_str(),
		     (I->CurrentSize*100.0)/I->TotalSize);
	 }
	 S << "]";
      }

      // Show at least something
      Line = S.str();
      S.clear();
      if (Line.empty() == true)
	 Line = _(" [Working]");
   }
   // Put in the percent done
   {
      std::stringstream S;
      ioprintf(S, "%.0f%%", Percent);
      S << Line;
      Line = S.str();
      S.clear();
   }

   /* Put in the ETA and cps meter, block off signals to prevent strangeness
      during resizing */
   sigset_t Sigs,OldSigs;
   sigemptyset(&Sigs);
   sigaddset(&Sigs,SIGWINCH);
   sigprocmask(SIG_BLOCK,&Sigs,&OldSigs);

   if (CurrentCPS != 0)
   {
      unsigned long long ETA = (TotalBytes - CurrentBytes)/CurrentCPS;
      std::string Tmp = " " + SizeToStr(CurrentCPS) + "B/s " + TimeToStr(ETA);
      size_t alignment = Line.length() + Tmp.length();
      if (alignment < ScreenWidth)
      {
	 alignment = ScreenWidth - alignment;
	 for (size_t i = 0; i < alignment; ++i)
	    Line.append(" ");
	 Line.append(Tmp);
      }
   }
   if (Line.length() > ScreenWidth)
      Line.erase(ScreenWidth);
   sigprocmask(SIG_SETMASK,&OldSigs,0);

   // Draw the current status
   if (_config->FindB("Apt::Color", false) == true)
      cout << _config->Find("APT::Color::Yellow");
   if (LastLineLength > Line.length())
      clearLastLine();
   else
      cout << '\r';
   cout << Line << flush;
   if (_config->FindB("Apt::Color", false) == true)
      cout << _config->Find("APT::Color::Neutral") << flush;

   LastLineLength = Line.length();
   Update = false;

   return true;
}
									/*}}}*/
// AcqTextStatus::MediaChange - Media need to be swapped		/*{{{*/
// ---------------------------------------------------------------------
/* Prompt for a media swap */
bool AcqTextStatus::MediaChange(string Media,string Drive)
{
   // If we do not output on a terminal and one of the options to avoid user
   // interaction is given, we assume that no user is present who could react
   // on your media change request
   if (isatty(STDOUT_FILENO) != 1 && Quiet >= 2 &&
       (_config->FindB("APT::Get::Assume-Yes",false) == true ||
	_config->FindB("APT::Get::Force-Yes",false) == true ||
	_config->FindB("APT::Get::Trivial-Only",false) == true))

      return false;

   clearLastLine();
   ioprintf(cout,_("Media change: please insert the disc labeled\n"
		   " '%s'\n"
		   "in the drive '%s' and press enter\n"),
	    Media.c_str(),Drive.c_str());

   char C = 0;
   bool bStatus = true;
   while (C != '\n' && C != '\r')
   {
      int len = read(STDIN_FILENO,&C,1);
      if(C == 'c' || len <= 0)
	 bStatus = false;
   }

   if(bStatus)
      Update = true;
   return bStatus;
}
									/*}}}*/
void AcqTextStatus::clearLastLine() {					/*{{{*/
   if (Quiet > 0)
      return;

   // do not try to clear more than the (now smaller) screen
   if (LastLineLength > ScreenWidth)
      LastLineLength = ScreenWidth;

   std::cout << '\r';
   for (size_t i = 0; i < LastLineLength; ++i)
      std::cout << ' ';
   std::cout << '\r' << std::flush;
}
									/*}}}*/
