#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>

using namespace std;

int main(int argc,const char *argv[])
{
   Configuration Cnf;
   
   ReadConfigFile(Cnf,argv[1],true);
   
   // Process 'simple-key' type sections
   const Configuration::Item *Top = Cnf.Tree("simple-key");
   for (Top = (Top == 0?0:Top->Child); Top != 0; Top = Top->Next)
   {
      Configuration Block(Top);
      
      string VendorID = Top->Tag;
      string FingerPrint = Block.Find("Fingerprint");
      string Name = Block.Find("Name"); // Description?
      
      if (FingerPrint.empty() == true || Name.empty() == true)
	 _error->Error("Block %s is invalid",VendorID.c_str());
      
      cout << VendorID << ' ' << FingerPrint << ' ' << Name << endl;
   }   
	 
   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
   
   return 0;
}
