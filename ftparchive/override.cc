// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: override.cc,v 1.4 2003/02/10 07:34:41 doogie Exp $
/* ######################################################################

   Override
   
   Store the override file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include "override.h"
    
#include <apti18n.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>

#include <stdio.h>
    
#include "override.h"
									/*}}}*/

// Override::ReadOverride - Read the override file			/*{{{*/
// ---------------------------------------------------------------------
/* This parses the override file and reads it into the map */
bool Override::ReadOverride(string File,bool Source)
{
   if (File.empty() == true)
      return true;
   
   FILE *F = fopen(File.c_str(),"r");
   if (F == 0)
      return _error->Errno("fopen",_("Unable to open %s"),File.c_str());
   
   char Line[500];
   unsigned long Counter = 0;
   while (fgets(Line,sizeof(Line),F) != 0)
   {
      Counter++;
      Item Itm;

      // Silence 
      for (char *I = Line; *I != 0; I++)
	 if (*I == '#')
	    *I = 0;

      // Strip space leading up to the package name, skip blank lines
      char *Pkg = Line;
      for (; isspace(*Pkg) && *Pkg != 0;Pkg++);
      if (Pkg == 0)
	 continue;

      // Find the package and zero..
      char *Start = Pkg;
      char *End = Pkg;
      for (; isspace(*End) == 0 && *End != 0; End++);
      if (*End == 0)
      {
	 _error->Warning(_("Malformed override %s line %lu #1"),File.c_str(),
			 Counter);
	 continue;
      }      
      *End = 0;

      // Find the priority
      if (Source == false)
      {
	 for (End++; isspace(*End) != 0 && *End != 0; End++);
	 Start = End;
	 for (; isspace(*End) == 0 && *End != 0; End++);
	 if (*End == 0)
	 {
	    _error->Warning(_("Malformed override %s line %lu #2"),File.c_str(),
			    Counter);
	    continue;
	 }
	 *End = 0;
	 Itm.Priority = Start;
      }
      
      // Find the Section
      for (End++; isspace(*End) != 0 && *End != 0; End++);
      Start = End;
      for (; isspace(*End) == 0 && *End != 0; End++);
      if (*End == 0)
      {
	 _error->Warning(_("Malformed override %s line %lu #3"),File.c_str(),
			 Counter);
	 continue;
      }      
      *End = 0;
      Itm.FieldOverride["Section"] = Start;

      // Source override files only have the two columns
      if (Source == true)
      {
	 Mapping[Pkg] = Itm;
	 continue;
      }
      
      // Find the =>
      for (End++; isspace(*End) != 0 && *End != 0; End++);
      if (*End != 0)
      {
	 Start = End;
	 for (; *End != 0 && (End[0] != '=' || End[1] != '>'); End++);
	 if (*End == 0 || strlen(End) < 4)
	 {
	    Itm.OldMaint = "*";
	    Itm.NewMaint = _strstrip(Start);
	 }
	 else
	 {
	    *End = 0;
	    Itm.OldMaint = _strstrip(Start);
	    
	    End += 3;
	    Itm.NewMaint = _strstrip(End);
	 }	 
      }

      Mapping[Pkg] = Itm;
   }

   if (ferror(F))
      _error->Errno("fgets",_("Failed to read the override file %s"),File.c_str());
   fclose(F);
   return true;
}
									/*}}}*/
// Override::ReadExtraOverride - Read the extra override file		/*{{{*/
// ---------------------------------------------------------------------
/* This parses the extra override file and reads it into the map */
bool Override::ReadExtraOverride(string File,bool Source)
{
   if (File.empty() == true)
      return true;
   
   FILE *F = fopen(File.c_str(),"r");
   if (F == 0)
      return _error->Errno("fopen",_("Unable to open %s"),File.c_str());
  
   char Line[500];
   unsigned long Counter = 0;
   while (fgets(Line,sizeof(Line),F) != 0)
   {
      Counter++;

      // Silence 
      for (char *I = Line; *I != 0; I++)
	 if (*I == '#')
	    *I = 0;

      // Strip space leading up to the package name, skip blank lines
      char *Pkg = Line;
      for (; isspace(*Pkg) && *Pkg != 0;Pkg++);
      if (Pkg == 0)
	 continue;

      // Find the package and zero..
      char *End = Pkg;
      for (; isspace(*End) == 0 && *End != 0; End++);
      if (*End == 0)
      {
	 _error->Warning(_("Malformed override %s line %lu #1"),File.c_str(),
			 Counter);
	 continue;
      }      
      *End = 0;

      // Find the field
      for (End++; isspace(*End) != 0 && *End != 0; End++);
      char *Field = End;
      for (; isspace(*End) == 0 && *End != 0; End++);
      if (*End == 0)
      {
	 _error->Warning(_("Malformed override %s line %lu #2"),File.c_str(),
			 Counter);
	 continue;
      }
      *End = 0;
      
      // Find the field value 
      for (End++; isspace(*End) != 0 && *End != 0; End++);
      char *Value = End;
      for (; *End != 0; End++);
      for (; isspace(*(End-1)) && End > Value; End--);
      if (End == Value)
      {
	 _error->Warning(_("Malformed override %s line %lu #3"),File.c_str(),
			 Counter);
	 continue;
      }      
      *End = 0;

      Mapping[Pkg].FieldOverride[Field] = Value;
   }

   if (ferror(F))
      _error->Errno("fgets",_("Failed to read the override file %s"),File.c_str());
   fclose(F);
   return true;
}
									/*}}}*/

// Override::GetItem - Get a architecture specific item 	/*{{{*/
// ---------------------------------------------------------------------
/* Returns a override item for the given package and the given architecture.
 * Treats "all" special
 */
Override::Item* Override::GetItem(string Package, string Architecture)
{
   map<string,Item>::iterator I = Mapping.find(Package);
   map<string,Item>::iterator J = Mapping.find(Package + "/" + Architecture);

   if (I == Mapping.end() && J == Mapping.end())
   {
      return 0;
   }

   Item *result = new Item;
   if (I == Mapping.end()) *result = J->second;
   else
   {
      *result = I->second;
      if (J != Mapping.end())
      {
	 Item *R = &J->second;
	 if (R->Priority != "") result->Priority = R->Priority;
	 if (R->OldMaint != "") result->OldMaint = R->OldMaint;
	 if (R->NewMaint != "") result->NewMaint = R->NewMaint;
	 for (map<string,string>::iterator foI = R->FieldOverride.begin();
	      foI != R->FieldOverride.end(); foI++)
         {
	    result->FieldOverride[foI->first] = foI->second;
	 }
      } 
   } 
   return result;
};


// Override::Item::SwapMaint - Swap the maintainer field if necessary	/*{{{*/
// ---------------------------------------------------------------------
/* Returns the new maintainer string after evaluating the rewriting rule. If
   there is a rule but it does not match then the empty string is returned,
   also if there was no rewrite rule the empty string is returned. Failed
   indicates if there was some kind of problem while rewriting. */
string Override::Item::SwapMaint(string Orig,bool &Failed)
{
   Failed = false;
   
   // Degenerate case..
   if (NewMaint.empty() == true)
	 return OldMaint;
   
   if (OldMaint == "*")
      return NewMaint;
   
   /* James: ancient, eliminate it, however it is still being used in the main
      override file. Thus it persists.*/
#if 1
   // Break OldMaint up into little bits on double slash boundaries.
   string::iterator End = OldMaint.begin();
   while (1)
   {
      string::iterator Start = End;      
      for (; End < OldMaint.end() &&
	   (End + 3 >= OldMaint.end() || End[0] != ' ' || 
	    End[1] != '/' || End[2] != '/'); End++);
      if (stringcasecmp(Start,End,Orig.begin(),Orig.end()) == 0)
	 return NewMaint;
      
      if (End >= OldMaint.end())
	 break;

      // Skip the divider and white space
      for (; End < OldMaint.end() && (*End == '/' || *End == ' '); End++);
   }
#else
   if (stringcasecmp(OldMaint.begin(),OldMaint.end(),Orig.begin(),Orig.end()) == 0)
      return NewMaint;   
#endif
   
   Failed = true;
   return string();
}
									/*}}}*/
