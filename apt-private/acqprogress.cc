// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Acquire Progress - Command line progress meter

   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-output.h>

#include <iostream>
#include <sstream>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

// AcqTextStatus::AcqTextStatus - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
AcqTextStatus::AcqTextStatus(std::ostream &out, unsigned int &ScreenWidth,unsigned int const Quiet) :
    pkgAcquireStatus(), out(out), ScreenWidth(ScreenWidth), LastLineLength(0), ID(0), Quiet(Quiet)
{
   // testcases use it to disable pulses without disabling other user messages
   if (Quiet == 0 && _config->FindB("quiet::NoUpdate", false) == true)
      this->Quiet = 1;
   if (Quiet < 2 && _config->FindB("quiet::NoProgress", false) == true)
      this->Quiet = 2;
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
void AcqTextStatus::AssignItemID(pkgAcquire::ItemDesc &Itm)		/*{{{*/
{
   /* In theory calling it from Fetch() would be enough, but to be
      safe we call it from IMSHit and Fail as well.
      Also, an Item can pass through multiple stages, so ensure
      that it keeps the same number */
   if (Itm.Owner->ID == 0)
      Itm.Owner->ID = ID++;
}
									/*}}}*/
// AcqTextStatus::IMSHit - Called when an item got a HIT response	/*{{{*/
// ---------------------------------------------------------------------
/* */
void AcqTextStatus::IMSHit(pkgAcquire::ItemDesc &Itm)
{
   if (Quiet > 1)
      return;

   AssignItemID(Itm);
   clearLastLine();

   // TRANSLATOR: Very short word to be displayed before unchanged files in 'apt-get update'
   ioprintf(out, _("Hit:%lu %s"), Itm.Owner->ID, Itm.Description.c_str());
   out << std::endl;
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
   AssignItemID(Itm);

   if (Quiet > 1)
      return;

   clearLastLine();

   // TRANSLATOR: Very short word to be displayed for files processed in 'apt-get update'
   // Potentially replaced later by "Hit:", "Ign:" or "Err:" if something (bad) happens
   ioprintf(out, _("Get:%lu %s"), Itm.Owner->ID, Itm.Description.c_str());
   if (Itm.Owner->FileSize != 0)
      out << " [" << SizeToStr(Itm.Owner->FileSize) << "B]";
   out << std::endl;
}
									/*}}}*/
// AcqTextStatus::Done - Completed a download				/*{{{*/
// ---------------------------------------------------------------------
/* We don't display anything... */
void AcqTextStatus::Done(pkgAcquire::ItemDesc &Itm)
{
   Update = true;
   AssignItemID(Itm);
}
									/*}}}*/
// AcqTextStatus::Fail - Called when an item fails to download		/*{{{*/
// ---------------------------------------------------------------------
/* We print out the error text  */
void AcqTextStatus::Fail(pkgAcquire::ItemDesc &Itm)
{
   if (Quiet > 1)
      return;

   AssignItemID(Itm);
   clearLastLine();

   bool ShowErrorText = true;
   if (Itm.Owner->Status == pkgAcquire::Item::StatDone || Itm.Owner->Status == pkgAcquire::Item::StatIdle)
   {
      // TRANSLATOR: Very short word to be displayed for files in 'apt-get update'
      // which failed to download, but the error is ignored (compare "Err:")
      ioprintf(out, _("Ign:%lu %s"), Itm.Owner->ID, Itm.Description.c_str());
      if (Itm.Owner->ErrorText.empty() ||
	    _config->FindB("Acquire::Progress::Ignore::ShowErrorText", false) == false)
	 ShowErrorText = false;
   }
   else
   {
      // TRANSLATOR: Very short word to be displayed for files in 'apt-get update'
      // which failed to download and the error is critical (compare "Ign:")
      ioprintf(out, _("Err:%lu %s"), Itm.Owner->ID, Itm.Description.c_str());
   }

   if (ShowErrorText)
   {
      std::string::size_type line_start = 0;
      std::string::size_type line_end;
      while ((line_end = Itm.Owner->ErrorText.find_first_of("\n\r", line_start)) != std::string::npos) {
	 out << std::endl << "  " << Itm.Owner->ErrorText.substr(line_start, line_end - line_start);
	 line_start = Itm.Owner->ErrorText.find_first_not_of("\n\r", line_end + 1);
	 if (line_start == std::string::npos)
	    break;
      }
      if (line_start == 0)
	 out << std::endl << "  " << Itm.Owner->ErrorText;
      else if (line_start != std::string::npos)
	 out << std::endl << "  " << Itm.Owner->ErrorText.substr(line_start);
   }
   out << std::endl;

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
      ioprintf(out,_("Fetched %sB in %s (%sB/s)\n"),
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
	    S << std::to_string(I->CurrentItem->Owner->ID) << " ";
	 S << I->CurrentItem->ShortDesc;

	 // Show the short mode string
	 if (I->CurrentItem->Owner->ActiveSubprocess.empty() == false)
	    S << " " << I->CurrentItem->Owner->ActiveSubprocess;

	 enum {Long = 0,Medium,Short} Mode = Medium;
	 // Add the current progress
	 if (Mode == Long)
	    S << " " << std::to_string(I->CurrentItem->CurrentSize);
	 else
	 {
	    if (Mode == Medium || I->CurrentItem->TotalSize == 0)
	       S << " " << SizeToStr(I->CurrentItem->CurrentSize) << "B";
	 }

	 // Add the total size and percent
	 if (I->CurrentItem->TotalSize > 0 && I->CurrentItem->Owner->Complete == false)
	 {
	    if (Mode == Short)
	       ioprintf(S, " %.0f%%", (I->CurrentItem->CurrentSize*100.0)/I->CurrentItem->TotalSize);
	    else
	       ioprintf(S, "/%sB %.0f%%", SizeToStr(I->CurrentItem->TotalSize).c_str(),
		     (I->CurrentItem->CurrentSize*100.0)/I->CurrentItem->TotalSize);
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
      out << _config->Find("APT::Color::Yellow");
   if (LastLineLength > Line.length())
      clearLastLine();
   else
      out << '\r';
   out << Line << std::flush;
   if (_config->FindB("Apt::Color", false) == true)
      out << _config->Find("APT::Color::Neutral") << std::flush;

   LastLineLength = Line.length();
   Update = false;

   return true;
}
									/*}}}*/
// AcqTextStatus::MediaChange - Media need to be swapped		/*{{{*/
// ---------------------------------------------------------------------
/* Prompt for a media swap */
bool AcqTextStatus::MediaChange(std::string Media, std::string Drive)
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
   ioprintf(out,_("Media change: please insert the disc labeled\n"
		   " '%s'\n"
		   "in the drive '%s' and press [Enter]\n"),
	    Media.c_str(),Drive.c_str());

   char C = 0;
   bool bStatus = true;
   while (C != '\n' && C != '\r')
   {
      int len = read(STDIN_FILENO,&C,1);
      if(C == 'c' || len <= 0) {
	 bStatus = false;
	 break;
      }
   }

   if(bStatus)
      Update = true;
   return bStatus;
}
									/*}}}*/
bool AcqTextStatus::ReleaseInfoChanges(metaIndex const * const L, metaIndex const * const N, std::vector<ReleaseInfoChange> &&Changes)/*{{{*/
{
   if (Quiet >= 2 || isatty(STDOUT_FILENO) != 1 || isatty(STDIN_FILENO) != 1 ||
	 _config->FindB("APT::Get::Update::InteractiveReleaseInfoChanges", false) == false)
      return pkgAcquireStatus::ReleaseInfoChanges(nullptr, nullptr, std::move(Changes));

   _error->PushToStack();
   auto const confirmed = pkgAcquireStatus::ReleaseInfoChanges(L, N, std::move(Changes));
   if (confirmed == true)
   {
      _error->MergeWithStack();
      return true;
   }
   clearLastLine();
   _error->DumpErrors(out, GlobalError::NOTICE, false);
   _error->RevertToStack();
   return YnPrompt(_("Do you want to accept these changes and continue updating from this repository?"), false, false, out, out);
}
									/*}}}*/
void AcqTextStatus::clearLastLine() {					/*{{{*/
   if (Quiet > 0 || LastLineLength == 0)
      return;

   // do not try to clear more than the (now smaller) screen
   if (LastLineLength > ScreenWidth)
      LastLineLength = ScreenWidth;

   out << '\r';
   for (size_t i = 0; i < LastLineLength; ++i)
      out << ' ';
   out << '\r' << std::flush;
}
									/*}}}*/
