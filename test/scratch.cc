#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/init.h>
#include <apt-pkg/error.h>
#include <strutl.h>

#include <signal.h>
#include <stdio.h>

class AcqTextStatus : public pkgAcquireStatus
{
   unsigned int ScreenWidth;
   char BlankLine[300];
   unsigned long ID;
   
   public:
   
   virtual void IMSHit(pkgAcquire::ItemDesc &Itm);
   virtual void Fetch(pkgAcquire::ItemDesc &Itm);
   virtual void Done(pkgAcquire::ItemDesc &Itm);
   virtual void Fail(pkgAcquire::ItemDesc &Itm);
   virtual void Start() {pkgAcquireStatus::Start(); BlankLine[0] = 0; ID = 1;};
   virtual void Stop();
   
   void Pulse(pkgAcquire *Owner);
};

// AcqTextStatus::IMSHit - Called when an item got a HIT response	/*{{{*/
// ---------------------------------------------------------------------
/* */
void AcqTextStatus::IMSHit(pkgAcquire::ItemDesc &Itm)
{
   cout << '\r' << BlankLine << '\r';   
   cout << "Hit " << Itm.Description;
   if (Itm.Owner->FileSize != 0)
      cout << " [" << SizeToStr(Itm.Owner->FileSize) << ']';
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
   
   cout << '\r' << BlankLine << '\r';   
   cout << hex << Itm.Owner->ID << dec << " Get " << Itm.Description;
   if (Itm.Owner->FileSize != 0)
      cout << " [" << SizeToStr(Itm.Owner->FileSize) << ']';
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
   cout << '\r' << BlankLine << '\r';   
   cout << "Err " << Itm.Description << endl;
   cout << "  " << Itm.Owner->ErrorText << endl;
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
   cout << '\r' << BlankLine << '\r';
   
   if (FetchedBytes == 0)
      cout << flush;
   else
      cout << "Fetched " << SizeToStr(FetchedBytes) << " in " << 
         TimeToStr(ElapsedTime) << " (" << SizeToStr(CurrentCPS) << 
         "/s)" << endl;
}
									/*}}}*/
// AcqTextStatus::Pulse - Regular event pulse				/*{{{*/
// ---------------------------------------------------------------------
/* This draws the current progress. Each line has an overall percent
   meter and a per active item status meter along with an overall 
   bandwidth and ETA indicator. */
void AcqTextStatus::Pulse(pkgAcquire *Owner)
{
   pkgAcquireStatus::Pulse(Owner);
   
   enum {Long = 0,Medium,Short} Mode = Long;
   
   ScreenWidth = 78;
   char Buffer[300];
   char *End = Buffer + sizeof(Buffer);
   char *S = Buffer;
   
   // Put in the percent done
   sprintf(S,"%ld%%",long(double(CurrentBytes*100.0)/double(TotalBytes)));   
   
   for (pkgAcquire::Worker *I = Owner->WorkersBegin(); I != 0;
	I = Owner->WorkerStep(I))
   {
      S += strlen(S);
      
      // There is no item running 
      if (I->CurrentItem == 0)
      {
	 if (I->Status.empty() == false)
	    snprintf(S,End-S," [%s]",I->Status.c_str());
	 continue;
      }

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
	    snprintf(S,End-S," %s",SizeToStr(I->CurrentSize).c_str());
      }
      S += strlen(S);
      
      // Add the total size and percent
      if (I->TotalSize > 0 && I->CurrentItem->Owner->Complete == false)
      {
	 if (Mode == Long)
	    snprintf(S,End-S,"/%u %u%%",I->TotalSize,
		     long(double(I->CurrentSize*100.0)/double(I->TotalSize)));
	 else
	 {
	    if (Mode == Medium)
	       snprintf(S,End-S,"/%s %u%%",SizeToStr(I->TotalSize).c_str(),
			long(double(I->CurrentSize*100.0)/double(I->TotalSize)));
	    else
	       snprintf(S,End-S," %u%%",
			long(double(I->CurrentSize*100.0)/double(I->TotalSize)));
	 }      
      }      
      S += strlen(S);
      snprintf(S,End-S,"]");
   }

   // Put in the ETA and cps meter
   if (CurrentCPS != 0)
   {      
      char Tmp[300];
      unsigned long ETA = (unsigned long)((TotalBytes - CurrentBytes)/CurrentCPS);
      sprintf(Tmp," %s/s %s",SizeToStr(CurrentCPS).c_str(),TimeToStr(ETA).c_str());
      unsigned int Len = strlen(Buffer);
      unsigned int LenT = strlen(Tmp);
      if (Len + LenT < ScreenWidth)
      {	 
	 memset(Buffer + Len,' ',ScreenWidth - Len);
	 strcpy(Buffer + ScreenWidth - LenT,Tmp);
      }      
   }
   Buffer[ScreenWidth] = 0;
   
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

int main(int argc,char *argv[])
{
   signal(SIGPIPE,SIG_IGN);

/*   URI Foo(argv[1]);
   cout << Foo.Access << '\'' << endl;
   cout << Foo.Host << '\'' << endl;
   cout << Foo.Path << '\'' << endl;
   cout << Foo.User << '\'' << endl;
   cout << Foo.Password << '\'' << endl;
   cout << Foo.Port << endl;
   
   return 0;*/

   pkgInitialize(*_config);
   
   pkgSourceList List;
   AcqTextStatus Stat;
   pkgAcquire Fetcher(&Stat);
   List.ReadMainList();
   
   pkgSourceList::const_iterator I;
   for (I = List.begin(); I != List.end(); I++)
   {
      new pkgAcqIndex(&Fetcher,I);
      if (_error->PendingError() == true)
	 break;
   }
   
   Fetcher.Run();
   
   _error->DumpErrors();
}
